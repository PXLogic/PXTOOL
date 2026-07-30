/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS

#include "storesession.h"
#include "sigsession.h"

#include "data/logicsnapshot.h"
#include "data/dsosnapshot.h"
#include "data/analogsnapshot.h"
#include "data/formatcapability.h"
#include "data/iooptions.h"
#include "data/analogpacketadapter.h"
#include "data/decoderstack.h"
#include "data/decode/decoder.h"
#include "data/decode/row.h"
#include "view/trace.h"
#include "view/signal.h"
#include "view/logicsignal.h"
#include "view/dsosignal.h"
#include "view/decodetrace.h"
#include "dock/protocoldock.h" 
 
#include <QFileDialog>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <cmath>
#include <limits>
#include <math.h>
#include <list>

#include <libsigrokdecode.h>
#include "config/appconfig.h"
#include "dsvdef.h"
#include "utility/path.h"
#include "log.h" 


#define DEOCDER_CONFIG_VERSION  2
 
namespace pv { 

namespace {

class OutputInstance final {
public:
    OutputInstance(const sr_output_module *module, GHashTable *options,
                   const sr_dev_inst *sdi, uint64_t start_sample_index)
        : value_(sr_output_new_with_start_sample_index(
            module, options, sdi, start_sample_index))
    {
    }

    ~OutputInstance()
    {
        if (value_)
            sr_output_free(value_);
    }

    const sr_output *get() const
    {
        return value_;
    }

private:
    const sr_output *value_ = nullptr;
};

bool format_requires_standard_analog(const sr_output_module *module)
{
    return module && (!strcmp(module->id, "analog") ||
                      !strcmp(module->id, "wav"));
}

} // namespace

StoreSession::StoreSession(SigSession *session) :
	_session(session),
    _outModule(NULL),
	_units_stored(0),
    _unit_count(0),
    _has_error(false),
    _canceled(false)
{ 
    _sessionDataGetter = NULL;
    _start_index = 0;
    _end_index = 0;
    _is_busy = false;
}

StoreSession::~StoreSession()
{
	wait();
}

SigSession* StoreSession::session()
{
    return _session;
}

void StoreSession::get_progress(uint64_t *writed, uint64_t *total)
{
    assert(writed);
    assert(total);

    *writed = _units_stored;
    *total = _unit_count;
}

const QString& StoreSession::error()
{ 
	return _error;
}

void StoreSession::wait()
{
    if (_thread.joinable())
        _thread.join();
}

void StoreSession::cancel()
{ 
    _canceled = true; 
}

void StoreSession::setSelectedOutputFormatId(const QString &format_id)
{
    _selectedOutputFormatId = format_id;
    _selectedOutputFormatExplicit = !format_id.isEmpty();
}

void StoreSession::setSelectedOutputOptions(const data::IoOptions &options)
{
    _selectedOutputOptions = options;
    _selectedOptionsFormatId = _selectedOutputFormatId;
    _hasSelectedOutputOptions = true;
}

bool StoreSession::hasExplicitSelectedOutputFormat() const
{
    return _selectedOutputFormatExplicit;
}

bool StoreSession::append_output(QFile &file, GString *chunk)
{
    if (!chunk)
        return true;

    const gsize length = chunk->len;
    const qint64 written = file.write(chunk->str, static_cast<qint64>(length));
    g_string_free(chunk, TRUE);
    return written == static_cast<qint64>(length);
}

bool StoreSession::save_start()
{ 
    assert(_sessionDataGetter);
    _has_error = false;
    _canceled = false;
    _error.clear();

    std::set<int> type_set;
    for(auto s : _session->get_signals()) {
        type_set.insert(s->get_type());
    }

    const bool mixed_signal_session =
        _session->get_device()->get_work_mode() == MSO &&
        type_set.size() == 2 &&
        type_set.count(SR_CHANNEL_LOGIC) && type_set.count(SR_CHANNEL_ANALOG);
    if (type_set.size() > 1 && !mixed_signal_session) {
        _error = tr("DSView does not currently support\nfile saving for multiple data types.");
        return false;

    } else if (type_set.size() == 0) {
        _error = tr("No data to save.");
        return false;
    }

    if (_file_name == ""){
        _error = tr("No file name.");
        return false;
    }

    const auto snapshot = _session->get_snapshot(
        mixed_signal_session ? SR_CHANNEL_LOGIC : *type_set.begin());
	assert(snapshot);
    // Check we have data
    if (snapshot->empty()) {
        _error = tr("No data to save.");
        return false;
    }
    if (mixed_signal_session) {
        const auto analog_snapshot = _session->get_snapshot(SR_CHANNEL_ANALOG);
        if (!analog_snapshot || analog_snapshot->empty()) {
            _error = tr("No data to save.");
            return false;
        }
    }

    std::string meta_data;
    std::string decoder_data;
    std::string session_data;
    
    meta_gen(snapshot, meta_data);
    decoders_gen(decoder_data);
    _sessionDataGetter->genSessionData(session_data);

    if (meta_data.empty()) {
        _error = tr("Generate temp file data failed.");
        QFile::remove(_file_name);
        return false;
    }
    if (decoder_data.empty()){
        _error = tr("Generate decoder file data failed.");
        QFile::remove(_file_name);
        return false;
    }
    if (session_data.empty()){
        _error = tr("Generate session file data failed.");
        QFile::remove(_file_name);
        return false;
    }
   
    auto _filename = path::ConvertPath(_file_name);
    
    if (m_zipDoc.CreateNew(_filename.c_str(), false))
    {    
        if ( !m_zipDoc.AddFromBuffer("header", meta_data.c_str(), meta_data.size())
            || !m_zipDoc.AddFromBuffer("decoders", decoder_data.c_str(), decoder_data.size())
            || !m_zipDoc.AddFromBuffer("session", session_data.c_str(), session_data.size())
        )
        {
            _has_error = true;
            _error = m_zipDoc.GetError();
        }
        else
        {
            if (_thread.joinable()) _thread.join();
            _thread = std::thread(&StoreSession::save_proc, this, snapshot);
            return !_has_error;
        }
    }
    else{
         _error = tr("Generate zip file failed.");
    }

    m_zipDoc.Release();
    QFile::remove(_file_name);
    return false;
}

void StoreSession::abort_save(const QString &error)
{
    if (!_has_error)
        _has_error = true;
    if (!error.isEmpty())
        _error = error;
    m_zipDoc.Release();
    QFile::remove(_file_name);
}

void StoreSession::save_logic(pv::data::LogicSnapshot *logic_snapshot, bool finalize)
{
    char chunk_name[20] = {0};
    uint16_t to_save_probes = 0;
    bool sample;
    int ret = SR_ERR;
    int num;

    for(auto s : _session->get_signals()) {
        if (s->enabled() && logic_snapshot->has_data(s->get_index()))
            to_save_probes++;
    }

    _unit_count = logic_snapshot->get_ring_sample_count() / 8 * to_save_probes;
    num = logic_snapshot->get_block_num();

    uint64_t start_index = _start_index;
    uint64_t end_index = _end_index;
    uint64_t start_offset = 0;
    uint64_t end_offset = 0;
    int start_block = 0;
    int end_block = 0;

    if (start_index > logic_snapshot->get_ring_sample_count()){
        dsv_err("ERROR:the start curosr is invalid!");
        _units_stored = -1;
        progress_updated();
        return;
    }
    if (end_index > logic_snapshot->get_ring_sample_count()){
        end_index = 0;
    }

    if (start_index > 0){
        start_index -= start_index % 64;         
        start_block = LogicSnapshot::get_block_with_sample(start_index, &start_offset);
    }
    if (end_index > 0){
        if (end_index % 64 != 0){
            end_index += (64 - end_index % 64);
        }        

        if (end_index > logic_snapshot->get_ring_sample_count()){
            end_index = 0;
        }
        else{
            end_block = LogicSnapshot::get_block_with_sample(end_index, &end_offset);
        }
    }

    if (start_index > 0 && end_index > 0){
        _unit_count = (end_index - start_index) / 8 * to_save_probes;
    }
    else if (start_index > 0){
        _unit_count = (logic_snapshot->get_ring_sample_count() - start_index) / 8 * to_save_probes;
    }
    else if (end_index > 0){
        _unit_count = end_index / 8 * to_save_probes;
    }

    for(auto s : _session->get_signals()) 
    {
        int ch_type = s->get_type();
        if (ch_type == SR_CHANNEL_LOGIC) {
            int ch_index = s->get_index();
            if (!s->enabled() || !logic_snapshot->has_data(ch_index))
                continue;

            for (int i = 0; !_canceled && i < num; i++) 
            {
                if (i < start_block){
                    continue;
                }
                if (i > end_block && end_block > 0){
                    break;
                }

                uint8_t *buf = logic_snapshot->get_block_buf(i, ch_index, sample);
                uint64_t size = logic_snapshot->get_block_size(i);
                bool need_malloc = (buf == NULL);

                if (i == end_block && end_offset / 8 < size && end_offset > 0){
                    size = end_offset / 8;
                }

                if (i == start_block && start_offset > 0){
                    if (buf != NULL){
                        buf += start_offset / 8;
                    }
                    size -= start_offset / 8;
                }
                
                if (need_malloc) {
                    buf = (uint8_t *)malloc(size);
                    if (buf == NULL) {
                        _has_error = true;
                        _error = tr("Failed to create zip file. Malloc error.");
                    } else {
                        memset(buf, sample ? 0xff : 0x0, size);
                    }
                }
                
                MakeChunkName(chunk_name, i - start_block, ch_index, ch_type, HEADER_FORMAT_VERSION);
                ret = m_zipDoc.AddFromBuffer(chunk_name, (const char*)buf, size) ? SR_OK : -1;

                if (ret != SR_OK) {
                    if (!_has_error) {
                        _has_error = true;
                        _error = tr("Failed to create zip file. Please check write permission of this path.");
                    }
                    progress_updated();
                    if (_has_error)
                        QFile::remove(_file_name);
                    return;
                }
                _units_stored += size;

                if (_units_stored > _unit_count 
                        && start_index == 0
                        && end_index == 0){
                    dsv_err("Read block data error!");
                    assert(false);
                }

                if (need_malloc)
                    free(buf);
                progress_updated();
            }
        }
    }

    progress_updated();

    if (_canceled || num == 0){
        QFile::remove(_file_name);
    }
    else if (finalize) {
        bool bret = m_zipDoc.Close();
        m_zipDoc.Release();

        if (!bret){
            _has_error = true;
            _error = m_zipDoc.GetError();
        }
    } 
}

void StoreSession::save_analog(pv::data::AnalogSnapshot *analog_snapshot, bool finalize)
{
    char chunk_name[20] = {0};
    int num = 0;
    int ret = SR_ERR;

    const int ch_type = SR_CHANNEL_ANALOG;

    if (ch_type != -1) {
        num = analog_snapshot->get_block_num();
        _unit_count = analog_snapshot->get_sample_count() *
                        analog_snapshot->get_unit_bytes() *
                        analog_snapshot->get_channel_num();
        uint8_t *buf = NULL;
        uint8_t *buf_start = NULL;

        buf = (uint8_t *)analog_snapshot->get_data() +
                        (analog_snapshot->get_ring_start() * analog_snapshot->get_unit_bytes()
                                         * analog_snapshot->get_channel_num());

        buf_start = (uint8_t *)analog_snapshot->get_data();

        const uint8_t *buf_end = buf_start + _unit_count;

        for (int i = 0; !_canceled && i < num; i++) {
            const uint64_t size = analog_snapshot->get_block_size(i);
            if ((buf + size) > buf_end) {
                uint8_t *tmp = (uint8_t *)malloc(size);
                if (tmp == NULL) {
                    _has_error = true;
                    _error = tr("Failed to create zip file. Malloc error.");
                } else {
                    memcpy(tmp, buf, buf_end-buf);
                    memcpy(tmp+(buf_end-buf), buf_start, buf+size-buf_end);
                } 

                MakeChunkName(chunk_name, i, 0, ch_type, HEADER_FORMAT_VERSION);
                ret = m_zipDoc.AddFromBuffer(chunk_name, (const char*)tmp, size) ? SR_OK : -1;

                buf += (size - _unit_count);
                if (tmp)
                    free(tmp);
            } 
            else { 
                MakeChunkName(chunk_name, i, 0, ch_type, HEADER_FORMAT_VERSION);
                ret = m_zipDoc.AddFromBuffer(chunk_name, (const char*)buf, size) ? SR_OK : -1;

                buf += size;
            }

            if (ret != SR_OK) {
                if (!_has_error) {
                    _has_error = true;
                    _error = tr("Failed to create zip file. Please check write permission of this path.");
                }
                progress_updated();
                if (_has_error)
                    QFile::remove(_file_name);
                return;
            }
            _units_stored += size;
            progress_updated();
        }
    }

    progress_updated();

    if (_canceled || num == 0){
        QFile::remove(_file_name);
    }
    else if (finalize) {
        bool bret = m_zipDoc.Close();
        m_zipDoc.Release();

        if (!bret){
            _has_error = true;
            _error = m_zipDoc.GetError();
        }
    } 
}

void StoreSession::save_dso(pv::data::DsoSnapshot *dso_snapshot)
{
    char chunk_name[20] = {0};
    int ret = SR_ERR; 
 
    uint64_t size = dso_snapshot->get_sample_count();
    int ch_num = dso_snapshot->get_channel_num();
    _unit_count = size * ch_num;

    for(auto s : _session->get_signals()) 
    {
        if (s->get_type() == SR_CHANNEL_DSO) {
            int ch_index = s->get_index();
 
            if (!dso_snapshot->has_data(ch_index))
                continue;

            if (_canceled)
                break;

            const uint8_t *data_buffer = dso_snapshot->get_samples(0, 0, ch_index);
        
            snprintf(chunk_name, 19, "O-%d/0", ch_index);
            ret = m_zipDoc.AddFromBuffer(chunk_name, (const char*)data_buffer, size) ? SR_OK : -1;

            if (ret != SR_OK) {
                if (!_has_error) {
                    _has_error = true;
                    _error = tr("Failed to create zip file. Please check write permission of this path.");
                }
                progress_updated();
                if (_has_error)
                    QFile::remove(_file_name);
                return;
            }

            _units_stored += size;
            progress_updated();
        }
    }

    progress_updated();

    if (_canceled || size == 0 || ch_num == 0){
        QFile::remove(_file_name);
    }
    else {
        bool bret = m_zipDoc.Close();
        m_zipDoc.Release();

        if (!bret){
            _has_error = true;
            _error = m_zipDoc.GetError();
        }
    }
}

void StoreSession::save_proc(data::Snapshot *snapshot)
{
	assert(snapshot);

    data::LogicSnapshot *logic_snapshot = NULL;
    data::AnalogSnapshot *analog_snapshot = NULL;
    data::DsoSnapshot *dso_snapshot = NULL;

    _is_busy = true;

    dsv_info("save task start.");

    if (_session->get_device()->get_work_mode() == MSO) {
        logic_snapshot = dynamic_cast<data::LogicSnapshot*>(
            _session->get_snapshot(SR_CHANNEL_LOGIC));
        analog_snapshot = dynamic_cast<data::AnalogSnapshot*>(
            _session->get_snapshot(SR_CHANNEL_ANALOG));
        if (logic_snapshot && analog_snapshot)
            save_mso(logic_snapshot, analog_snapshot);
        else
            abort_save(tr("Failed to save mixed signal data."));
    }
    else if ((logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot))) {
        save_logic(logic_snapshot);
    }
    else if ((analog_snapshot = dynamic_cast<data::AnalogSnapshot*>(snapshot))) {
        save_analog(analog_snapshot);
    }
    else if ((dso_snapshot = dynamic_cast<data::DsoSnapshot*>(snapshot))) {
        save_dso(dso_snapshot);
    }
 
    dsv_info("save task end.");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    _is_busy = false;   
}

void StoreSession::save_mso(data::LogicSnapshot *logic_snapshot,
                            data::AnalogSnapshot *analog_snapshot)
{
    save_logic(logic_snapshot, false);
    if (_has_error || _canceled) {
        abort_save();
        return;
    }
    save_analog(analog_snapshot, false);
    if (_has_error || _canceled) {
        abort_save();
        return;
    }

    if (!m_zipDoc.Close()) {
        _has_error = true;
        _error = m_zipDoc.GetError();
    }
    m_zipDoc.Release();
    if (_has_error)
        QFile::remove(_file_name);
}

bool StoreSession::meta_gen(data::Snapshot *snapshot, std::string &str)
{
    GSList *l;
    struct sr_channel *probe;
    int probecnt;
    char *s;
    struct sr_status status;
    char meta[300] = {0};
  
    sprintf(meta, "%s", "[version]\n"); str += meta;
    sprintf(meta, "version = %d\n", HEADER_FORMAT_VERSION); str += meta;
    sprintf(meta, "%s", "[header]\n"); str += meta;

    int mode = _session->get_device()->get_work_mode();

    if (true) {
        sprintf(meta, "driver = %s\n", _session->get_device()->driver_name().toLocal8Bit().data()); str += meta;
        sprintf(meta, "device mode = %d\n", mode); str += meta;
    }
 
    sprintf(meta, "capturefile = data\n"); str += meta;
    uint64_t total_samples = snapshot->get_sample_count();
    if (mode == MSO) {
        const auto analog_snapshot = dynamic_cast<data::AnalogSnapshot *>(
            _session->get_snapshot(SR_CHANNEL_ANALOG));
        if (!analog_snapshot)
            return false;
        total_samples = std::max(total_samples, analog_snapshot->get_sample_count());
    }
    sprintf(meta, "total samples = %" PRIu64 "\n", total_samples); str += meta;

    if (mode != LOGIC && mode != MSO) {
        sprintf(meta, "total probes = %d\n", snapshot->get_channel_num()); str += meta;
        sprintf(meta, "total blocks = %d\n", snapshot->get_block_num()); str += meta;
    }

    data::LogicSnapshot *logic_snapshot = NULL;
    if ((logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot))) {
        uint16_t to_save_probes = 0;
        for (l = _session->get_device()->get_channels(); l; l = l->next) {
            probe = (struct sr_channel *)l->data;
            if (probe->type == SR_CHANNEL_LOGIC && probe->enabled &&
                logic_snapshot->has_data(probe->index))
                to_save_probes++;
        }

        int block_count = logic_snapshot->get_block_num();

        uint64_t start_index = _start_index;
        uint64_t end_index = _end_index;
        uint64_t start_offset = 0;
        uint64_t end_offset = 0;
        int start_block = 0;
        int end_block = 0;

        if (end_index > logic_snapshot->get_ring_sample_count()){
            end_index = 0;
        }
        if (start_index > 0){
            start_block = LogicSnapshot::get_block_with_sample(start_index, &start_offset);
        }
        if (end_index > 0){
            end_block = LogicSnapshot::get_block_with_sample(end_index, &end_offset);
        }

        if (start_index > 0 && end_index > 0){
            block_count = end_block - start_block + 1;
        }
        else if (start_index > 0){
            block_count = block_count - start_block;
        }
        else if (end_index > 0){
            block_count = end_block + 1;
        }

        if (mode == MSO) {
            const auto analog_snapshot = dynamic_cast<data::AnalogSnapshot *>(
                _session->get_snapshot(SR_CHANNEL_ANALOG));
            if (!analog_snapshot)
                return false;
            uint64_t saved_logic_samples = logic_snapshot->get_ring_sample_count();
            if (start_index > 0 || end_index > 0) {
                uint64_t saved_start = start_index - start_index % 64;
                uint64_t saved_end = end_index;
                if (saved_end != 0) {
                    saved_end += (64 - saved_end % 64) % 64;
                    if (saved_end > logic_snapshot->get_ring_sample_count())
                        saved_end = 0;
                }
                saved_logic_samples = (saved_end == 0
                    ? logic_snapshot->get_ring_sample_count() : saved_end) - saved_start;
            }
            sprintf(meta, "mso logic samples = %" PRIu64 "\n",
                    saved_logic_samples); str += meta;
            sprintf(meta, "mso logic probes = %d\n", to_save_probes); str += meta;
            sprintf(meta, "mso logic blocks = %d\n", block_count); str += meta;
            sprintf(meta, "mso analog samples = %" PRIu64 "\n",
                    analog_snapshot->get_sample_count()); str += meta;
            sprintf(meta, "mso analog probes = %u\n",
                    analog_snapshot->get_channel_num()); str += meta;
            sprintf(meta, "mso analog blocks = %d\n",
                    analog_snapshot->get_block_num()); str += meta;
            sprintf(meta, "mso analog bits = %u\n",
                    analog_snapshot->get_unit_bytes() * 8); str += meta;
        }
        else {
            sprintf(meta, "total probes = %d\n", to_save_probes); str += meta;
            sprintf(meta, "total blocks = %d\n", block_count); str += meta;
        }
    }

    s = sr_samplerate_string(_session->cur_snap_samplerate());

    sprintf(meta, "samplerate = %s\n", s); str += meta;

    uint64_t tmp_u64;
    int tmp_u8;
    uint32_t tmp_u32;

    if (mode == DSO) {
        if (_session->get_device()->get_config_uint64(SR_CONF_TIMEBASE, tmp_u64)) {
            sprintf(meta, "hDiv = %" PRIu64 "\n", tmp_u64); str += meta;
        }

        if (_session->get_device()->get_config_uint64(SR_CONF_MAX_TIMEBASE, tmp_u64)) {
            sprintf(meta, "hDiv max = %" PRIu64 "\n", tmp_u64); str += meta;
        }

        if (_session->get_device()->get_config_uint64(SR_CONF_MIN_TIMEBASE, tmp_u64)) {
            sprintf(meta, "hDiv min = %" PRIu64 "\n", tmp_u64); str += meta;
        }
 
        if (_session->get_device()->get_config_byte(SR_CONF_UNIT_BITS, tmp_u8)) {
            sprintf(meta, "bits = %d\n", tmp_u8); str += meta;
        }
 
        if (_session->get_device()->get_config_uint32(SR_CONF_REF_MIN, tmp_u32)) {
            sprintf(meta, "ref min = %d\n", tmp_u32); str += meta;
        }

        if (_session->get_device()->get_config_uint32(SR_CONF_REF_MAX, tmp_u32)) {
            sprintf(meta, "ref max = %d\n", tmp_u32); str += meta;
        }
    }
    else if (mode == LOGIC) {
        sprintf(meta, "trigger time = %lld\n", _session->get_session_time().toMSecsSinceEpoch()); str += meta;
    }
    else if (mode == ANALOG) {
        data::AnalogSnapshot *analog_snapshot = NULL;
        if ((analog_snapshot = dynamic_cast<data::AnalogSnapshot*>(snapshot))) {
            uint8_t tmp_u8 = analog_snapshot->get_unit_bytes();
            sprintf(meta, "bits = %d\n", tmp_u8*8); str += meta;
        }

        if (_session->get_device()->get_config_uint32(SR_CONF_REF_MIN, tmp_u32)) {
            sprintf(meta, "ref min = %d\n", tmp_u32); str += meta;
        }

        if (_session->get_device()->get_config_uint32(SR_CONF_REF_MAX, tmp_u32)) {
            sprintf(meta, "ref max = %d\n", tmp_u32); str += meta;
        }
    }
    sprintf(meta, "trigger pos = %" PRIu64 "\n", _session->get_trigger_pos()); str += meta;

    probecnt = 0; 

    for (l = _session->get_device()->get_channels(); l; l = l->next) {
        
        probe = (struct sr_channel *)l->data;
        
        data::Snapshot *probe_snapshot = snapshot;
        if (mode == MSO)
            probe_snapshot = _session->get_snapshot(probe->type);
        if (!probe_snapshot ||
            (mode != MSO && !probe_snapshot->has_data(probe->index)))
            continue;

        if (mode == LOGIC && !probe->enabled)
            continue;

        if (mode == MSO && probe->name)
        {
            const char *prefix = probe->type == SR_CHANNEL_LOGIC
                ? "logic probe" : "analog probe";
            sprintf(meta, "%s%d = %s\n", prefix, probe->index, probe->name);
            str += meta;
        }
        else if (probe->name)
        {
            int sigdex = (mode == LOGIC) ? probe->index : probecnt;
            sprintf(meta, "probe%d = %s\n", sigdex, probe->name);
            str += meta;
        }

        if (probe->trigger){
            sprintf(meta, " trigger%d = %s\n", probecnt, probe->trigger); 
            str += meta;
        }

        if (mode == DSO)
        {
            sprintf(meta, " enable%d = %d\n", probecnt, probe->enabled);
            str += meta;
            sprintf(meta, " coupling%d = %d\n", probecnt, probe->coupling);
            str += meta;
            sprintf(meta, " vDiv%d = %" PRIu64 "\n", probecnt, probe->vdiv);
            str += meta;
            sprintf(meta, " vFactor%d = %" PRIu64 "\n", probecnt, probe->vfactor);
            str += meta;
            sprintf(meta, " vOffset%d = %d\n", probecnt, probe->hw_offset);
            str += meta;
            sprintf(meta, " vTrig%d = %d\n", probecnt, probe->trig_value);
            str += meta;

            if (_session->dso_status_is_valid())
            {
                sr_status status = _session->get_dso_status();
                
                if (probe->index == 0)
                {
                    sprintf(meta, " period%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_tlen);
                    str += meta;
                    sprintf(meta, " pcnt%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_cnt);
                    str += meta;
                    sprintf(meta, " max%d = %d\n", probecnt, status.ch0_max);
                    str += meta;
                    sprintf(meta, " min%d = %d\n", probecnt, status.ch0_min);
                    str += meta;
                    sprintf(meta, " plen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_plen);
                    str += meta;
                    sprintf(meta, " llen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_llen);
                    str += meta;
                    sprintf(meta, " level%d = %d\n", probecnt, status.ch0_level_valid);
                    str += meta;
                    sprintf(meta, " plevel%d = %d\n", probecnt, status.ch0_plevel);
                    str += meta;
                    sprintf(meta, " low%d = %" PRIu32 "\n", probecnt, status.ch0_low_level);
                    str += meta;
                    sprintf(meta, " high%d = %" PRIu32 "\n", probecnt, status.ch0_high_level);
                    str += meta;
                    sprintf(meta, " rlen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_rlen);
                    str += meta;
                    sprintf(meta, " flen%d = %" PRIu32 "\n", probecnt, status.ch0_cyc_flen);
                    str += meta;
                    sprintf(meta, " rms%d = %" PRIu64 "\n", probecnt, status.ch0_acc_square);
                    str += meta;
                    sprintf(meta, " mean%d = %" PRIu32 "\n", probecnt, status.ch0_acc_mean);
                    str += meta;
                }
                else
                {
                    sprintf(meta, " period%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_tlen);
                    str += meta;
                    sprintf(meta, " pcnt%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_cnt);
                    str += meta;
                    sprintf(meta, " max%d = %d\n", probecnt, status.ch1_max);
                    str += meta;
                    sprintf(meta, " min%d = %d\n", probecnt, status.ch1_min);
                    str += meta;
                    sprintf(meta, " plen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_plen);
                    str += meta;
                    sprintf(meta, " llen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_llen);
                    str += meta;
                    sprintf(meta, " level%d = %d\n", probecnt, status.ch1_level_valid);
                    str += meta;
                    sprintf(meta, " plevel%d = %d\n", probecnt, status.ch1_plevel);
                    str += meta;
                    sprintf(meta, " low%d = %" PRIu32 "\n", probecnt, status.ch1_low_level);
                    str += meta;
                    sprintf(meta, " high%d = %" PRIu32 "\n", probecnt, status.ch1_high_level);
                    str += meta;
                    sprintf(meta, " rlen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_rlen);
                    str += meta;
                    sprintf(meta, " flen%d = %" PRIu32 "\n", probecnt, status.ch1_cyc_flen);
                    str += meta;
                    sprintf(meta, " rms%d = %" PRIu64 "\n", probecnt, status.ch1_acc_square);
                    str += meta;
                    sprintf(meta, " mean%d = %" PRIu32 "\n", probecnt, status.ch1_acc_mean);
                    str += meta;
                }
            }
        }
        else if (mode == ANALOG)
        {
            sprintf(meta, " enable%d = %d\n", probecnt, probe->enabled);
            str += meta;
            sprintf(meta, " coupling%d = %d\n", probecnt, probe->coupling);
            str += meta;
            sprintf(meta, " vDiv%d = %" PRIu64 "\n", probecnt, probe->vdiv);
            str += meta;
            sprintf(meta, " vOffset%d = %d\n", probecnt, probe->hw_offset);
            str += meta;
            sprintf(meta, " mapUnit%d = %s\n", probecnt, probe->map_unit);
            str += meta;
            sprintf(meta, " mapMax%d = %lf\n", probecnt, probe->map_max);
            str += meta;
            sprintf(meta, " mapMin%d = %lf\n", probecnt, probe->map_min);
            str += meta;
        }
        probecnt++;
    } 

    return true;
}

bool StoreSession::validateExportFormat()
{
    _has_error = false;
    _error.clear();

    std::set<int> type_set;
    for (auto signal : _session->get_signals())
        type_set.insert(signal->get_type());

    if (type_set.size() > 1) {
        _error = tr("DSView does not currently support\nfile export for multiple data types.");
        return false;
    }
    if (type_set.empty()) {
        _error = tr("No data to save.");
        return false;
    }

    const auto snapshot = _session->get_snapshot(*type_set.begin());
    if (!snapshot) {
        _error = tr("No data to save.");
        return false;
    }

    const QVector<data::FormatCapability> formats = data::exportFormats();
    const data::FormatCapability *format = data::resolveExportFormatSelection(
        formats, _selectedOutputFormatId, QString(), _suffix);
    if (!format) {
        _error = tr("Invalid export format.");
        return false;
    }

    _outModule = sr_output_find(format->id.toUtf8().data());
    if (_outModule == NULL) {
        _error = tr("Invalid export format.");
        return false;
    }

    data::ExportDataType data_type;
    if (dynamic_cast<data::LogicSnapshot *>(snapshot))
        data_type = data::ExportDataType::Logic;
    else if (dynamic_cast<data::AnalogSnapshot *>(snapshot))
        data_type = data::ExportDataType::Analog;
    else if (dynamic_cast<data::DsoSnapshot *>(snapshot))
        data_type = data::ExportDataType::Dso;
    else {
        _error = tr("The active data type is not supported for export.");
        return false;
    }

    _error = data::exportCompatibilityError(*format, data_type);
    if (!_error.isEmpty())
        return false;

    if (!format_requires_standard_analog(_outModule))
        return true;

    auto *analog_snapshot = dynamic_cast<data::AnalogSnapshot *>(snapshot);
    QString reason;
    uint32_t ref_min = 0;
    uint32_t ref_max = 0;
    int enabled_channels = 0;

    if (!analog_snapshot) {
        reason = tr("the active capture is not analog data");
    } else if (!analog_snapshot->is_float() && analog_snapshot->get_unit_bytes() != 1) {
        reason = tr("only 8-bit analog snapshots can be converted safely");
    } else if (_session->cur_snap_samplerate() == 0) {
        reason = tr("the sample rate is unavailable");
    } else if (!_session->get_device()->get_config_uint32(
                   SR_CONF_REF_MIN, ref_min) ||
               !_session->get_device()->get_config_uint32(
                   SR_CONF_REF_MAX, ref_max) || ref_max <= ref_min) {
        reason = tr("the capture reference range is unavailable");
    } else {
        for (const GSList *item = _session->get_device()->get_channels();
             item; item = item->next) {
            const auto *channel = static_cast<const sr_channel *>(item->data);
            if (!channel || channel->type != SR_CHANNEL_ANALOG ||
                !channel->enabled)
                continue;

            ++enabled_channels;
            if (!channel->name || !channel->map_unit ||
                strcmp(channel->map_unit, "V") ||
                !analog_snapshot->has_data(channel->index) ||
                analog_snapshot->get_ch_order(channel->index) < 0 ||
                !std::isfinite(channel->map_min) ||
                !std::isfinite(channel->map_max) ||
                channel->map_max <= channel->map_min) {
                reason = tr("an enabled channel lacks a valid voltage mapping");
                break;
            }
        }
        if (reason.isEmpty() && enabled_channels == 0)
            reason = tr("no enabled analog channels are available");
        if (reason.isEmpty() && !strcmp(_outModule->id, "wav")) {
            const uint64_t bytes_per_frame =
                static_cast<uint64_t>(enabled_channels) * sizeof(float);
            if (bytes_per_frame > (std::numeric_limits<uint16_t>::max)() ||
                _session->cur_snap_samplerate() >
                    (std::numeric_limits<uint32_t>::max)() / bytes_per_frame) {
                reason = tr("the sample rate or channel count exceeds the WAV format limits");
            }
        }
    }

    if (!reason.isEmpty()) {
        _error = tr("%1 requires standard analog samples: %2.")
            .arg(format->description, reason);
        return false;
    }

    return true;
}

//export as csv file
bool StoreSession::export_start()
{
    if (!validateExportFormat())
        return false;

    std::set<int> type_set;
    for (auto signal : _session->get_signals())
        type_set.insert(signal->get_type());

    const auto snapshot = _session->get_snapshot(*type_set.begin());
    assert(snapshot);
    if (snapshot->empty()) {
        _error = tr("No data to save.");
        return false;
    }

    if (_file_name.isEmpty()) {
        _error = tr("No set file name.");
        return false;
    }

    if (_thread.joinable())
        _thread.join();
    _thread = std::thread(&StoreSession::export_proc, this, snapshot);
    return !_has_error;
}

void StoreSession::export_proc(data::Snapshot *snapshot)
{
    _is_busy = true;

    dsv_info("export task start.");

    export_exec(snapshot);

    dsv_info("export task end.");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    _is_busy = false;
}

void StoreSession::export_exec(data::Snapshot *snapshot)
{
    assert(snapshot);

        //set export all data flag
    AppConfig &app = AppConfig::Instance();
    int origin_flag = app.appOptions.originalData ? 1 : 0;

    data::LogicSnapshot *logic_snapshot = NULL;
    data::AnalogSnapshot *analog_snapshot = NULL;
    data::DsoSnapshot *dso_snapshot = NULL;
    int channel_type;

    if ((logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot))) {
        channel_type = SR_CHANNEL_LOGIC;
    } else if ((dso_snapshot = dynamic_cast<data::DsoSnapshot*>(snapshot))) {
        channel_type = SR_CHANNEL_DSO;
    } else if ((analog_snapshot = dynamic_cast<data::AnalogSnapshot*>(snapshot))) {
        channel_type = SR_CHANNEL_ANALOG;
    } else {
        _has_error = true;
        _error = tr("data type don't support.");
        return;
    }

    const uint64_t snapshot_samples = snapshot->get_sample_count();
    const uint64_t snapshot_ring_samples = snapshot->get_ring_sample_count();
    uint64_t export_limit_samples = snapshot_samples;

    if (logic_snapshot) {
        const uint64_t session_limit_samples = _session->cur_samplelimits();

        if (_start_index > 0 && _end_index > 0 && _end_index > _start_index) {
            export_limit_samples = _end_index - _start_index;
        } else if (_end_index > 0) {
            export_limit_samples = _end_index;
        } else if (_start_index > 0) {
            export_limit_samples =
                snapshot_ring_samples > _start_index
                    ? snapshot_ring_samples - _start_index
                    : 0;
        } else if (session_limit_samples != 0) {
            export_limit_samples = session_limit_samples;
        } else {
            export_limit_samples = snapshot_ring_samples;
        }

        dsv_info("StoreSession::export_exec logic meta: samplerate=%llu session_limit=%llu snapshot_samples=%llu ring_samples=%llu range=[%llu,%llu] metadata_samples=%llu original=%d",
                 (unsigned long long)_session->cur_snap_samplerate(),
                 (unsigned long long)session_limit_samples,
                 (unsigned long long)snapshot_samples,
                 (unsigned long long)snapshot_ring_samples,
                 (unsigned long long)_start_index,
                 (unsigned long long)_end_index,
                 (unsigned long long)export_limit_samples,
                 origin_flag);
    } else {
        dsv_info("StoreSession::export_exec meta: type=%d samplerate=%llu snapshot_samples=%llu ring_samples=%llu metadata_samples=%llu",
                 channel_type,
                 (unsigned long long)_session->cur_snap_samplerate(),
                 (unsigned long long)snapshot_samples,
                 (unsigned long long)snapshot_ring_samples,
                 (unsigned long long)export_limit_samples);
    }

    const sr_option **module_options = sr_output_options_get(_outModule);
    data::IoOptions output_options(module_options);
    if (module_options)
        sr_output_options_free(module_options);

    if (_hasSelectedOutputOptions &&
        _selectedOptionsFormatId == QString::fromUtf8(_outModule->id)) {
        output_options = _selectedOutputOptions;
    }
    _selectedOutputOptions = data::IoOptions(nullptr);
    _selectedOptionsFormatId.clear();
    _hasSelectedOutputOptions = false;

    if (!strcmp(_outModule->id, "csv"))
        output_options.set("type",
            QVariant::fromValue(static_cast<qint16>(channel_type)));
    else if (!strcmp(_outModule->id, "srzip"))
        output_options.set("filename", _file_name);

    GHashTable *params = output_options.toGHashTable();
    const uint64_t start_sample_index =
        channel_type == SR_CHANNEL_LOGIC ? _start_index : 0;
    OutputInstance output(_outModule, params, _session->get_device()->inst(),
                          start_sample_index);
    g_hash_table_destroy(params);

    if (!output.get()) {
        _has_error = true;
        _error = tr("Failed to initialize export module.");
        return;
    }

    const bool module_writes_file = !strcmp(_outModule->id, "srzip");
    QFile file(_file_name);
    if (!module_writes_file &&
        !file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        _has_error = true;
        _error = tr("Failed to open export file for writing.");
        QFile::remove(_file_name);
        return;
    }

    const auto cleanup_partial_export = [&file, module_writes_file, this] {
        file.close();
        if (!module_writes_file)
            QFile::remove(_file_name);
    };

    const auto fail_export = [this, &cleanup_partial_export](const QString &error) {
        _has_error = true;
        _error = error;
        cleanup_partial_export();
    };

    const auto send_packet = [this, &output, &file, module_writes_file, &fail_export](
        const sr_datafeed_packet &packet) {
        GString *chunk = nullptr;
        if (sr_output_send(output.get(), &packet, &chunk) != SR_OK) {
            if (chunk)
                g_string_free(chunk, TRUE);
            fail_export(QObject::tr("Failed to export data."));
            return false;
        }

        if (module_writes_file) {
            if (chunk)
                g_string_free(chunk, TRUE);
            return true;
        }

        if (!append_output(file, chunk)) {
            fail_export(QObject::tr("Failed to write export file."));
            return false;
        }

        return true;
    };

    struct sr_datafeed_packet p;
    struct sr_datafeed_header header;

    memset(&header, 0, sizeof(header));
    header.feed_version = 1;
    gettimeofday(&header.starttime, NULL);

    p.type = SR_DF_HEADER;
    p.status = SR_PKT_OK;
    p.payload = &header;
    p.bExportOriginalData = 0;
    if (!send_packet(p))
        return;

    // Meta
    struct sr_datafeed_meta meta;
    struct sr_config *src;

    src = _session->get_device()->new_config(SR_CONF_SAMPLERATE,
                g_variant_new_uint64(_session->cur_snap_samplerate()));

    meta.config = g_slist_append(NULL, src);

    const bool standard_analog_output =
        format_requires_standard_analog(_outModule);
    if (!standard_analog_output) {
        src = _session->get_device()->new_config(SR_CONF_LIMIT_SAMPLES,
                    g_variant_new_uint64(export_limit_samples));

        meta.config = g_slist_append(meta.config, src);

        GVariant *gvar;
        int bits=0;

        _session->get_device()->get_config_byte(SR_CONF_UNIT_BITS, bits);

        gvar = _session->get_device()->get_config(SR_CONF_REF_MIN);
        if (gvar != NULL) {
            src = _session->get_device()->new_config(SR_CONF_REF_MIN, gvar);
            g_variant_unref(gvar);
        }
        else {
            src = _session->get_device()->new_config(SR_CONF_REF_MIN, g_variant_new_uint32(1));
        }

        meta.config = g_slist_append(meta.config, src);

        gvar = _session->get_device()->get_config(SR_CONF_REF_MAX);
        if (gvar != NULL) {
            src = _session->get_device()->new_config(SR_CONF_REF_MAX, gvar);
            g_variant_unref(gvar);
        }
        else {
            src = _session->get_device()->new_config(SR_CONF_REF_MAX, g_variant_new_uint32((1 << bits) - 1));
        }
        meta.config = g_slist_append(meta.config, src);
    }

    p.type = SR_DF_META;
    p.status = SR_PKT_OK;
    p.payload = &meta;
    p.bExportOriginalData = 0;
    if (!send_packet(p)) {
        for (GSList *l = meta.config; l; l = l->next)
            _session->get_device()->free_config(static_cast<sr_config *>(l->data));
        g_slist_free(meta.config);
        return;
    }
    for (GSList *l = meta.config; l; l = l->next) {
        src = (struct sr_config *)l->data;
        _session->get_device()->free_config(src);
    }
    g_slist_free(meta.config);

    if (channel_type == SR_CHANNEL_LOGIC) {
        _unit_count = logic_snapshot->get_ring_sample_count();
        int blk_num = logic_snapshot->get_block_num();
        bool sample;
        std::vector<uint8_t *> buf_vec;
        std::vector<bool> buf_sample;

        uint64_t start_index = _start_index;
        uint64_t end_index = _end_index;
        uint64_t start_offset = 0;
        uint64_t end_offset = 0;
        int start_block = 0;
        int end_block = 0;

        if (start_index > logic_snapshot->get_ring_sample_count()){
            dsv_err("ERROR:the start curosr is invalid!");
            _units_stored = -1;
            fail_export(tr("Invalid export data range."));
            progress_updated();
            return;
        }
        if (end_index > logic_snapshot->get_ring_sample_count()){
            end_index = 0;
        }

        if (start_index > 0){
            start_block = LogicSnapshot::get_block_with_sample(start_index, &start_offset);
        }
        if (end_index > 0){
            end_block = LogicSnapshot::get_block_with_sample(end_index, &end_offset);
        }

        if (start_index > 0 && end_index > 0){
            _unit_count = (end_index - start_index);
        }
        else if (start_index > 0){
            _unit_count = (logic_snapshot->get_ring_sample_count() - start_index);
        }
        else if (end_index > 0){
            _unit_count = end_index;
        }

        dsv_info("StoreSession::export_exec logic data: blocks=%d ring_samples=%llu start=%llu end=%llu rows_to_send=%llu metadata_samples=%llu",
                 blk_num,
                 (unsigned long long)logic_snapshot->get_ring_sample_count(),
                 (unsigned long long)start_index,
                 (unsigned long long)end_index,
                 (unsigned long long)_unit_count,
                 (unsigned long long)export_limit_samples);

        for (int blk = 0; !_canceled  &&  blk < blk_num; blk++) {
            uint64_t buf_sample_num = logic_snapshot->get_block_size(blk) * 8;
            buf_vec.clear();
            buf_sample.clear();

            if (blk < start_block)
                continue;
            if (blk > end_block && end_block > 0)
                break;

            for(auto s : _session->get_signals()) {
                int ch_type = s->get_type();
                if (ch_type == SR_CHANNEL_LOGIC) {
                    int ch_index = s->get_index();
                    if (!logic_snapshot->has_data(ch_index))
                        continue;
                    uint8_t *buf = logic_snapshot->get_block_buf(blk, ch_index, sample);
                    buf_vec.push_back(buf);
                    buf_sample.push_back(sample);
                }
            }

            uint16_t unitsize = ceil(buf_vec.size() / 8.0);
            unsigned int usize = 8192;
            unsigned int size = usize;
            struct sr_datafeed_logic lp;

            for(uint64_t i = 0; !_canceled && i < buf_sample_num; i+=usize){
                if(buf_sample_num - i < usize)
                    size = buf_sample_num - i;
                uint8_t *xbuf = (uint8_t *)malloc(size * unitsize);
                if (xbuf == NULL) {
                    fail_export(tr("Failed to allocate export buffer."));
                    return;
                }                
                memset(xbuf, 0, size * unitsize);

                for (uint64_t j = 0; j < size; j++) {
                    for (unsigned int k = 0; k < buf_vec.size(); k++) {
                        if (buf_vec[k] == NULL && buf_sample[k])
                            xbuf[j*unitsize+k/8] +=  1 << k%8;
                        else if (buf_vec[k] && (buf_vec[k][(i+j)/8] & (1 << j%8)))
                            xbuf[j*unitsize+k/8] +=  1 << k%8;
                    }
                }

                lp.data = xbuf;
                lp.length = size * unitsize;
                lp.unitsize = unitsize;
                lp.format = LA_CROSS_DATA;
                p.type = SR_DF_LOGIC;
                p.status = SR_PKT_OK;
                p.payload = &lp;
                p.bExportOriginalData = origin_flag;
                if (!send_packet(p)) {
                    free(xbuf);
                    return;
                }

                _units_stored += size;
                if (xbuf)
                    free(xbuf);
                progress_updated();
            }
        }
    }
    else if (channel_type == SR_CHANNEL_DSO) {
        _unit_count = snapshot->get_sample_count(); 
        unsigned int usize = 8192;
        unsigned int size = usize;
        struct sr_datafeed_dso dp; 

        uint8_t *ch_data_buffer = (uint8_t*)malloc(usize * dso_snapshot->get_channel_num() + 1);
        if (ch_data_buffer == NULL){
            dsv_err("StoreSession::export_proc, malloc failed.");
            fail_export(tr("Failed to allocate export buffer."));
            return;
        }

        int ch_num = dso_snapshot->get_channel_num();

        for(uint64_t i = 0; !_canceled && i < _unit_count; i+=usize){
            if(_unit_count - i < usize)
                size = _unit_count - i;

            int ch = 0;
            // Make the cross data buffer.
           for(auto s : _session->get_signals())
            {
                if (s->get_type() != SR_CHANNEL_DSO)
                    continue;

                if (!dso_snapshot->has_data(s->get_index()))
                    continue;
                
                uint8_t *wr = ch_data_buffer + ch;
                ch++;
                const uint8_t *rd = dso_snapshot->get_samples(0,0, s->get_index()) + i;
                const uint8_t *rd_end = rd + size;
                
                while (rd < rd_end)
                {
                    *wr = *rd;                     
                    wr += ch_num;
                    rd++;
                }
            }

            dp.data = ch_data_buffer;
            dp.num_samples = size;
            p.type = SR_DF_DSO;
            p.status = SR_PKT_OK;
            p.payload = &dp;
            p.bExportOriginalData = 0;
            if (!send_packet(p)) {
                free(ch_data_buffer);
                return;
            }

            _units_stored += size;
            progress_updated();
        }

        if (ch_data_buffer){
            free(ch_data_buffer);
            ch_data_buffer = NULL;
        }

    } else if (channel_type == SR_CHANNEL_ANALOG) {
        _unit_count = snapshot->get_sample_count();
        uint64_t unit_count = _unit_count;
        void* data_buffer = analog_snapshot->get_data();
        unsigned int usize = 8192;

        QVector<data::AnalogChannelRef> analog_channels;
        QVector<data::Unsigned8AnalogConversion> conversions;
        uint32_t ref_min = 0;
        uint32_t ref_max = 0;
        if (standard_analog_output) {
            if (!_session->get_device()->get_config_uint32(
                    SR_CONF_REF_MIN, ref_min) ||
                !_session->get_device()->get_config_uint32(
                    SR_CONF_REF_MAX, ref_max) || ref_max <= ref_min) {
                fail_export(tr("The analog reference range became unavailable."));
                return;
            }

            for (GSList *item = _session->get_device()->get_channels();
                 item; item = item->next) {
                auto *channel = static_cast<sr_channel *>(item->data);
                if (!channel || channel->type != SR_CHANNEL_ANALOG ||
                    !channel->enabled)
                    continue;

                const int source_index =
                    analog_snapshot->get_ch_order(channel->index);
                const double units_per_code =
                    (channel->map_max - channel->map_min) /
                    static_cast<double>(ref_max - ref_min);
                analog_channels.append(data::AnalogChannelRef(channel));
                conversions.append({
                    static_cast<uint32_t>(source_index),
                    static_cast<double>(channel->hw_offset),
                    units_per_code,
                });
            }
        }

        struct sr_datafeed_analog ap{};

        const uint64_t ring_start = analog_snapshot->get_ring_start();
 
        int ch_count = snapshot->get_channel_num();  
    
        void *block_buffer[2];
        uint64_t block_samples[2];
        block_buffer[0] =  (unsigned char*)data_buffer + ring_start * ch_count;
        block_samples[0] = unit_count - ring_start;
        block_buffer[1] = data_buffer;
        block_samples[1] = ring_start;

        for (int j=0; j<2; j++)
        {  
            uint64_t sample_count = block_samples[j];

            if (sample_count == 0)
                break;

            //dsv_info("sample_count:%llu,total:%llu", sample_count, unit_count);

            for(uint64_t i = 0; i < sample_count; i += usize){
                
                if (_canceled)
                    break;

                unsigned int size = usize;

                if(sample_count - i < usize){
                    size = sample_count - i;
                }
         
                if (standard_analog_output) {
                    try {
                        std::vector<float> samples;
                        if (analog_snapshot->is_float()) {
                            samples.reserve(static_cast<size_t>(size) * analog_channels.size());
                            const uint64_t base = (j == 0 ? ring_start : 0) + i;
                            for (uint64_t sample = 0; sample < size; ++sample) {
                                for (int channel = 0; channel < analog_channels.size(); ++channel) {
                                    const int order = analog_snapshot->get_ch_order(
                                        analog_channels[channel].index);
                                    samples.push_back(static_cast<float>(
                                        analog_snapshot->sample_as_double(order, base + sample)));
                                }
                            }
                        } else {
                            samples = data::convertUnsigned8AnalogSamples(
                                static_cast<const uint8_t *>(block_buffer[j]) +
                                    i * ch_count,
                                size, ch_count, conversions);
                        }
                        auto packet = data::makeAnalogPacket(
                            analog_channels, samples, size,
                            _session->cur_snap_samplerate(),
                            SR_MQ_VOLTAGE, SR_UNIT_VOLT);
                        packet.packet.bExportOriginalData = 0;
                        if (!send_packet(packet.packet))
                            return;
                    } catch (const std::exception &error) {
                        dsv_err("Failed to create standard analog packet: %s",
                                error.what());
                        fail_export(tr("Failed to convert analog samples."));
                        return;
                    }
                } else {
                    ap.data = (unsigned char*)block_buffer[j] + i * ch_count;
                    ap.num_samples = size;
                    p.type = SR_DF_ANALOG;
                    p.status = SR_PKT_OK;
                    p.payload = &ap;
                    p.bExportOriginalData = 0;
                    if (!send_packet(p))
                        return;
                }

                _units_stored += size;
                progress_updated();

               // dsv_info("size:%llu;_units_stored:%llu", size, _units_stored);
            }
        }
    }

    if (_canceled) {
        cleanup_partial_export();
        return;
    }

    p.type = SR_DF_END;
    p.status = SR_PKT_OK;
    p.payload = nullptr;
    p.bExportOriginalData = 0;
    if (!send_packet(p))
        return;

    file.close();

    progress_updated();
}

 
bool StoreSession::decoders_gen(std::string &str)
{  
    QJsonArray dec_array;
    if (!gen_decoders_json(dec_array))
        return false;
    QJsonDocument sessionDoc(dec_array);
    QString data = QString::fromUtf8(sessionDoc.toJson()); 
    str = std::string(data.toLocal8Bit().data());
    return true;
}

bool StoreSession::gen_decoders_json(QJsonArray &array)
{  
    for(auto s : _session->get_decode_signals()) {
        QJsonObject dec_obj;
        QJsonArray stack_array;
        QJsonObject show_obj;
        const auto &stack = s->decoder();
        const auto &decoderList = stack->stack();

        for(auto dec : decoderList) 
        {
            QJsonArray ch_array;
            const srd_decoder *const d = dec->decoder();;
            const bool have_probes = (d->channels || d->opt_channels) != 0;

            if (have_probes) {
                auto binded_probes = dec->binded_probe_list();
                for(auto probe : binded_probes) {
                    QJsonObject ch_obj;
                    int binded_index = dec->binded_probe_index(probe);
                    ch_obj[probe->id] = QJsonValue::fromVariant(binded_index);
                    ch_array.push_back(ch_obj);
                }
            }

            QJsonObject options_obj;
            auto dec_binding = new prop::binding::DecoderOptions(stack, dec);

            for (GSList *l = d->options; l; l = l->next)
            {
                const srd_decoder_option *const opt =
                    (srd_decoder_option*)l->data;

                if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("d"))) {
                    GVariant *const var = dec_binding->getter(opt->id);
                    if (var != NULL) {
                        options_obj[opt->id] = QJsonValue::fromVariant(g_variant_get_double(var));
                        g_variant_unref(var);
                    }
                } else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("x"))) {
                    GVariant *const var = dec_binding->getter(opt->id);
                    if (var != NULL) {
                        options_obj[opt->id] = QJsonValue::fromVariant(get_integer(var));
                        g_variant_unref(var);
                    }
                } else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("s"))) {
                    GVariant *const var = dec_binding->getter(opt->id);
                    if (var != NULL) {
                        const char *sz = g_variant_get_string(var, NULL);
                        options_obj[opt->id] = QJsonValue::fromVariant(QString(sz));
                        g_variant_unref(var);
                    }
                }else {
                    continue;
                }
            }

            if (have_probes) {
                dec_obj["id"] = QJsonValue::fromVariant(QString(d->id));
                dec_obj["channel"] = ch_array;
                dec_obj["options"] = options_obj;
            } else {
                QJsonObject stack_obj;
                stack_obj["id"] = QJsonValue::fromVariant(QString(d->id));
                stack_obj["options"] = options_obj;
                stack_array.push_back(stack_obj);
            }
            show_obj[d->id] = QJsonValue::fromVariant(dec->shown());
        }
        
        dec_obj["version"] = DEOCDER_CONFIG_VERSION;
        dec_obj["label"] = QString(s->get_name().toUtf8().data());
        dec_obj["stacked decoders"] = stack_array;
        dec_obj["view_index"] = s->get_view_index();
        /* Remember which engine the user picked for this protocol so it
         * round-trips through Save/Open. 1 = force C, 2 = force Python.
         * Only meaningful for protocols that have both implementations. */
        dec_obj["engine_hint"] = stack->use_c_decoder() ? 1 : 2;

        auto rows = stack->get_rows_gshow();
        for (auto i = rows.begin(); i != rows.end(); i++) {
            pv::data::decode::Row _row = (*i).first;
            QString kn = _row.title_id();
            show_obj[kn] = QJsonValue::fromVariant((*i).second);
        }
        dec_obj["show"] = show_obj;

        array.push_back(dec_obj);
    }

    return true;
}

bool StoreSession::load_decoders(dock::ProtocolDock *widget, QJsonArray &dec_array)
{
    if (_session->get_device()->get_work_mode() != LOGIC)
    {
        dsv_info("StoreSession::load_decoders(), is not LOGIC mode.");
        return false;
    }

    if (dec_array.isEmpty()){
        dsv_info("StoreSession::load_decoders(), json object array is empty.");
        return false;
    }

    int dec_index = -1;
    
    for (const QJsonValue &dec_value : dec_array)
    {
        QJsonObject dec_obj = dec_value.toObject(); 
        std::vector<view::DecodeTrace*> &pre_dsigs = _session->get_decode_signals();
        std::list<pv::data::decode::Decoder*> sub_decoders;

        //get sub decoders
        if (dec_obj.contains("stacked decoders")) {
                for(const QJsonValue &value : dec_obj["stacked decoders"].toArray()) {
                    QJsonObject stacked_obj = value.toObject();

                    GSList *dl = g_slist_copy((GSList*)srd_decoder_list());
                    for(; dl; dl = dl->next) {
                        const srd_decoder *const d = (srd_decoder*)dl->data;
                        assert(d);

                        if (QString::fromUtf8(d->id) == stacked_obj["id"].toString()) {
                            sub_decoders.push_back(new data::decode::Decoder(d));
                            break;
                        }
                    }
                    g_slist_free(dl);
                }
        }

        //create protocol
        const int engine_hint = dec_obj.contains("engine_hint")
            ? dec_obj["engine_hint"].toInt(0)
            : 0;
        bool ret = widget->add_protocol_by_id(dec_obj["id"].toString(), true, sub_decoders, engine_hint);
        if (!ret)
        {
            for(auto sub : sub_decoders){
                delete sub;
            }
            sub_decoders.clear();

            continue; //protocol is not exists;
        }

        dec_index++;

        if (dec_obj.contains("label")){
            _session->set_decoder_row_label(dec_index, dec_obj["label"].toString());    
        }

        if (dec_obj.contains("view_index")){
            int chan_view_index = dec_obj["view_index"].toInt();
            _session->get_decoder_trace(dec_index)->set_view_index(chan_view_index);
        }

        std::list<int> bind_indexs;

        std::vector<view::DecodeTrace*> &aft_dsigs = _session->get_decode_signals();

        if (aft_dsigs.size() >= pre_dsigs.size()) {
            const GSList *l;
            
            auto new_dsig = aft_dsigs.back();
            auto stack = new_dsig->decoder();
 
            auto &decoder_list = stack->stack();

            for(auto dec : decoder_list) 
            {
                const srd_decoder *const d = dec->decoder();
                QJsonObject options_obj;

                if (dec == decoder_list.front()) {
                    std::map<const srd_channel*, int> probe_map;
                    // Load the mandatory channels
                    for(l = d->channels; l; l = l->next) {
                        const struct srd_channel *const pdch = (struct srd_channel *)l->data;

                        for (const QJsonValue &value : dec_obj["channel"].toArray()) {
                            QJsonObject ch_obj = value.toObject();
                            if (ch_obj.contains(pdch->id)) {
                                int bind_chan = ch_obj[pdch->id].toInt();
                                probe_map[pdch] = bind_chan;

                                auto fd_it = find(bind_indexs.begin(), bind_indexs.end(), bind_chan);
                                if (fd_it == bind_indexs.end())
                                    bind_indexs.push_back(bind_chan);
                                break;
                            }
                        }
                    }

                    // Load the optional channels
                    for(l = d->opt_channels; l; l = l->next) {
                        const struct srd_channel *const pdch = (struct srd_channel *)l->data;

                        for (const QJsonValue &value : dec_obj["channel"].toArray()) {
                            QJsonObject ch_obj = value.toObject();
                            if (ch_obj.contains(pdch->id)) {
                                int bind_chan = ch_obj[pdch->id].toInt();
                                probe_map[pdch] = bind_chan;

                                auto fd_it = find(bind_indexs.begin(), bind_indexs.end(), bind_chan);
                                if (fd_it == bind_indexs.end())
                                    bind_indexs.push_back(bind_chan);
                                break;
                            }
                        }
                    }
                    dec->set_probes(probe_map);
                    options_obj = dec_obj["options"].toObject();
                }
                else {
                    for(const QJsonValue &value : dec_obj["stacked decoders"].toArray()) {
                        QJsonObject stacked_obj = value.toObject();
                        if (QString::fromUtf8(d->id) == stacked_obj["id"].toString()) {
                            options_obj = stacked_obj["options"].toObject();
                            break;
                        }
                    }
                }

                for (l = d->options; l; l = l->next) {
                    const srd_decoder_option *const opt = (srd_decoder_option*)l->data;

                    if (options_obj.contains(opt->id)) 
                    {
                        GVariant *new_value = NULL;
                        // When the numberic value is a string, it got zero always,
                        // so must convert from string.
                        QString vs = options_obj[opt->id].toString();

                        if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("d"))) 
                        {
                            double vi = options_obj[opt->id].toDouble();
                            if (vs != "") vi = vs.toDouble();
                            new_value = g_variant_new_double(vi);
                        }
                        else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("x"))) {
                            const GVariantType *const type = g_variant_get_type(opt->def);

                            if (g_variant_type_equal(type, G_VARIANT_TYPE_BYTE)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_byte(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT16)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_int16(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT16)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_uint16(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT32)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_int32(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT32)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_uint32(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT64)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_int64(vi);
                            }
                            else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT64)){
                                int vi = options_obj[opt->id].toInt();                               
                                if (vs != "") vi = vs.toInt();
                                new_value = g_variant_new_uint64(vi);
                            }
                        }
                        else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("s"))) {
                            new_value = g_variant_new_string(vs.toUtf8().data());
                        }

                        if (new_value != NULL){
                            dec->set_option(opt->id, new_value);
                        }
                    }
                }
                dec->commit();

                if (dec_obj.contains("show")) {
                    QJsonObject show_obj = dec_obj["show"].toObject();
                    if (show_obj.contains(d->id)) {
                        dec->show(show_obj[d->id].toBool());
                    }
                }
            }

            // Restore the binded channel index
            if (bind_indexs.size() > 0){
                auto dec_trace = _session->get_decoder_trace(dec_index);
                if (dec_trace != NULL)
                    dec_trace->set_index_list(bind_indexs);
            }

            int decoder_cfg_version = -1;

            if (dec_obj.contains("version")){
                decoder_cfg_version = dec_obj["version"].toInt();
            }

            if (dec_obj.contains("show")) {
                QJsonObject show_obj = dec_obj["show"].toObject();
                std::map<const pv::data::decode::Row, bool> rows = stack->get_rows_gshow();

                for (auto i = rows.begin();i != rows.end(); i++) {
                    QString key;

                    if (decoder_cfg_version == -1)
                        key = (*i).first.title();
                    else
                        key = (*i).first.title_id();

                    if (show_obj.contains(key)) {
                        bool bShow = show_obj[key].toBool();
                        const pv::data::decode::Row r = (*i).first;
                        stack->set_rows_gshow(r, bShow);
                    }
                }
            }
        }
    }

    return true;
}
 

double StoreSession::get_integer(GVariant *var)
{
    double val = 0;
    const GVariantType *const type = g_variant_get_type(var);
    assert(type);

    if (g_variant_type_equal(type, G_VARIANT_TYPE_BYTE))
        val = g_variant_get_byte(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT16))
        val = g_variant_get_int16(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT16))
        val = g_variant_get_uint16(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT32))
        val = g_variant_get_int32(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT32))
        val = g_variant_get_uint32(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT64))
        val = g_variant_get_int64(var);
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT64))
        val = g_variant_get_uint64(var);
    else
        assert(false);

    return val;
}

QString StoreSession::MakeSaveFile(bool bDlg)
{
    QString default_name;
    QString device_name = _session->get_device()->name().trimmed();
    device_name.replace(QRegularExpression("\\s+"), "-");

    AppConfig &app = AppConfig::Instance(); 
    if (app.userHistory.saveDir != "")
    {
        default_name = app.userHistory.saveDir + "/"  + device_name + "-";
    } 
    else{
        QDir _dir;
        QString _root = _dir.home().path();                
        default_name =  _root + "/" + device_name + "-";
    } 

    for (const GSList *l = _session->get_device()->get_device_mode_list(); l; l = l->next) 
    {
        const sr_dev_mode *mode = (const sr_dev_mode *)l->data;
        if (_session->get_device()->get_work_mode() == mode->mode) {
            default_name += mode->acronym;
            break;
        }
    }

    default_name += _session->get_session_time().toString("-yyMMdd-hhmmss");

    // Show the dialog
    if (bDlg)
    {
        default_name = QFileDialog::getSaveFileName(
            NULL,
            tr("Save File"),
            default_name,
            //tr
            "DSView Data (*.dsl)");

        if (default_name.isEmpty())
        {
            return ""; //no select file
        }

        QString _dir_path = path::GetDirectoryName(default_name);

        if (_dir_path != app.userHistory.saveDir)
        {
            app.userHistory.saveDir = _dir_path;
            app.SaveHistory();
        }
    }

    QFileInfo f(default_name);
    if (f.suffix().compare("dsl"))
    {
        //Tr
        default_name.append(".dsl");
    }
    _file_name = default_name;
    return default_name;     
}

QString StoreSession::MakeExportFile(bool bDlg)
{
    QString default_name;
    QString device_name = _session->get_device()->name().trimmed();
    device_name.replace(QRegularExpression("\\s+"), "-");
    AppConfig &app = AppConfig::Instance();  
    
    if (app.userHistory.exportDir != "")
    {
        default_name = app.userHistory.exportDir  + "/"  + device_name + "-";
    } 
    else{
        QDir _dir;
        QString _root = _dir.home().path();    
        default_name =  _root + "/" + device_name + "-";
    }  

    for (const GSList *l = _session->get_device()->get_device_mode_list(); l; l = l->next) {
        const sr_dev_mode *mode = (const sr_dev_mode *)l->data;
        if (_session->get_device()->get_work_mode() == mode->mode) {
            default_name += mode->acronym;
            break;
        }
    }
    default_name += _session->get_session_time().toString("-yyMMdd-hhmmss");

    const QVector<data::FormatCapability> formats = data::exportFormats();
    const QString filter = data::saveDialogFilter(formats);

    QString selfilter;
    const data::FormatCapability *selected_format = nullptr;
    if (!_selectedOutputFormatId.isEmpty()) {
        selected_format = data::findFormatById(formats, _selectedOutputFormatId);
        if (selected_format)
            selfilter = selected_format->dialogFilter;
    }

    if (selfilter.isEmpty() && app.userHistory.exportFormat != ""
            && _session->get_device()->get_work_mode() == LOGIC){
        for (const data::FormatCapability &format : formats) {
            if (format.dialogFilter == app.userHistory.exportFormat) {
                selected_format = &format;
                _selectedOutputFormatId = format.id;
                _selectedOutputFormatExplicit = false;
                selfilter = format.dialogFilter;
                break;
            }
        }
    }

    if (selfilter.isEmpty()) {
        selected_format = data::findFormatById(formats, "csv");
        if (selected_format) {
            selfilter = selected_format->dialogFilter;
            _selectedOutputFormatId = selected_format->id;
            _selectedOutputFormatExplicit = false;
        }
    }

    if (bDlg)
    {
        default_name = QFileDialog::getSaveFileName(
            NULL,
            tr("Export Data"),
            default_name,
            filter,
            &selfilter);

        if (default_name == "")
        {
            return "";
        }

        for (const data::FormatCapability &format : formats) {
            if (format.dialogFilter == selfilter) {
                selected_format = &format;
                _selectedOutputFormatId = format.id;
                _selectedOutputFormatExplicit = false;
                break;
            }
        }

        bool bChange = false;
        QString _dir_path = path::GetDirectoryName(default_name);
        if (_dir_path != app.userHistory.exportDir)
        {
            app.userHistory.exportDir = _dir_path;
            bChange = true;
        }
        
        if (selfilter != app.userHistory.exportFormat 
                && _session->get_device()->get_work_mode() == LOGIC){
            app.userHistory.exportFormat = selfilter;
             bChange = true;            
        }

        if (bChange){
            app.SaveHistory();            
        }
    }

    QFileInfo f(default_name);
    if (f.suffix().isEmpty() && selected_format && !selected_format->extensions.isEmpty())
        default_name += "." + selected_format->extensions.first();

    _suffix = QFileInfo(default_name).suffix();

    _file_name = default_name;
    return default_name;    
}

bool StoreSession::IsLogicDataType()
{
    std::set<int> type_set;
    for(auto sig : _session->get_signals()) {
        type_set.insert(sig->get_type());
    }

    if (type_set.size()){
        int type = *(type_set.begin());
        return type == SR_CHANNEL_LOGIC;
    }

    return false;
}

void StoreSession::MakeChunkName(char *chunk_name, int chunk_num, int index, int type, int version)
{ 
    chunk_name[0] = 0;

    if (version >= 2)
    {
        const char *type_name = NULL;
        type_name = (type == SR_CHANNEL_LOGIC) ? "L" : (type == SR_CHANNEL_DSO)  ? "O"
                                                   : (type == SR_CHANNEL_ANALOG) ? "A"
                                                                                 : "U";
        snprintf(chunk_name, 15, "%s-%d/%d", type_name, index, chunk_num);
    }
    else
    {
        snprintf(chunk_name, 15, "data");
    }
}

} // pv
