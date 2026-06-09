/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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
  

#include <stdexcept>
#include <algorithm>
#include <assert.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "decoderstack.h"
#include "logicsnapshot.h"
#include "decode/decoder.h"
#include "decode/annotation.h"
#include "decode/rowdata.h"
#include "../sigsession.h"
#include "../view/logicsignal.h"
#include "../dsvdef.h"
#include "../log.h"
#include <ds_types.h>
#include "c_decoder_registry.h"

using namespace pv::data::decode;
using namespace std;
using namespace boost;

namespace pv {
namespace data {

const double DecoderStack::DecodeMargin = 1.0;
std::mutex DecoderStack::s_srd_session_mutex;
const double DecoderStack::DecodeThreshold = 0.2;
const int64_t DecoderStack::DecodeChunkLength = 4 * 1024; 
const unsigned int DecoderStack::DecodeNotifyPeriod = 1024;
 
DecoderStack::DecoderStack(pv::SigSession *session,
	const srd_decoder *const dec, DecoderStatus *decoder_status) :
	_session(session)
{
    assert(session);
    assert(dec);
    assert(decoder_status); 
    
    _samples_decoded = 0;
    _sample_count = 0; 
    _decode_state = Stopped;
    _options_changed = false;
    _no_memory = false;
    _mark_index = -1;
    _decoder_status = decoder_status;
    _stask_stauts = NULL; 
    _is_capture_end = true;
    _snapshot = NULL;
    _progress = 0;
    _is_decoding = false;
    _result_count = 0;
    
    _use_c_decoder = false;

    _stack.push_back(new decode::Decoder(dec));
 
    build_row();

    const srd_decoder *root = _stack.front()->decoder();
    _use_c_decoder = root &&
        pv::cdecoders::CDecoderRegistry::instance().has_c_decoder_for_id(root->id);
}

void DecoderStack::join_own_thread()
{
    if (_own_thread.joinable()) _own_thread.join();
}

DecoderStack::~DecoderStack()
{
    join_own_thread();

    //release resource talbe
    DESTROY_OBJECT(_decoder_status);

    //release source
    for (auto &kv : _rows)
    {
        kv.second->clear(); //destory all annotations
        delete kv.second;
    }
    _rows.clear();

    //Decoder
    for (auto *p : _stack){
        delete p;
    }
    _stack.clear();
    
    _rows_gshow.clear();
    _rows_lshow.clear();
    _class_rows.clear();
}
 
void DecoderStack::add_sub_decoder(decode::Decoder *decoder)
{
	assert(decoder);
	_stack.push_back(decoder);
    build_row();
    _options_changed = true;
}

void DecoderStack::remove_sub_decoder(Decoder *decoder)
{
	// Find the decoder in the stack
    auto  iter = _stack.begin();
    for(unsigned int i = 0; i < _stack.size(); i++, iter++)
        if ((*iter) == decoder)
            break;

	// Delete the element
    if (iter != _stack.end())
    {
        _stack.erase(iter);
        delete decoder;
    }        

    build_row();
    _options_changed = true;
}

void DecoderStack::remove_decoder_by_handel(const srd_decoder *dec)
{
    Decoder *decoder = NULL;

    for (auto d : _stack){
        if (d->get_dec_handel() == dec){
            decoder = d;
            break;
        }
    }

    if (decoder){
        remove_sub_decoder(decoder);
    }
}

void DecoderStack::build_row()
{
    //release source
    for (auto &kv : _rows)
    {   
        kv.second->clear(); //destory all annotations
        delete kv.second;
    }
    _rows.clear();

    // Add classes
    for (auto dec : _stack)
    { 
        const srd_decoder *const decc = dec->decoder();
        assert(dec->decoder());

        dec->reset_start();

        // Add a row for the decoder if it doesn't have a row list
        if (!decc->annotation_rows) {
            const Row row(decc);
            _rows[row] = new decode::RowData();
            std::map<const decode::Row, bool>::const_iterator iter = _rows_gshow.find(row);
            if (iter == _rows_gshow.end()) {
                _rows_gshow[row] = true;
                if (row.title().contains("bit", Qt::CaseInsensitive) ||
                    row.title().contains("warning", Qt::CaseInsensitive)) {
                    _rows_lshow[row] = false;
                } else {
                    _rows_lshow[row] = true;
                }
            }
        }

        // Add the decoder rows
        int order = 0;
        for (const GSList *l = decc->annotation_rows; l; l = l->next)
        {
            const srd_decoder_annotation_row *const ann_row =
                (srd_decoder_annotation_row *)l->data;
            assert(ann_row);

            const Row row(decc, ann_row, order);

            // Add a new empty row data object
            _rows[row] = new decode::RowData();
            std::map<const decode::Row, bool>::const_iterator iter = _rows_gshow.find(row);
            if (iter == _rows_gshow.end()) {
                _rows_gshow[row] = true;
                if (row.title().contains("bit", Qt::CaseInsensitive) ||
                    row.title().contains("warning", Qt::CaseInsensitive)) {
                    _rows_lshow[row] = false;
                } else {
                    _rows_lshow[row] = true;
                }
            }

            // Map out all the classes
            for (const GSList *ll = ann_row->ann_classes; ll; ll = ll->next){
                _class_rows[make_pair(decc, GPOINTER_TO_INT(ll->data))] = Row(row);
            }

            order++;
        }
    }
}

int64_t DecoderStack::samples_decoded()
{
    std::lock_guard<std::mutex> decode_lock(_output_mutex);
	return _samples_decoded;
}

void DecoderStack::get_annotation_subset(
	std::vector<pv::data::decode::Annotation*> &dest,
	const Row &row, uint64_t start_sample,
	uint64_t end_sample)
{  
    auto iter = _rows.find(row);
    if (iter != _rows.end())
        (*iter).second->get_annotation_subset(dest,
			start_sample, end_sample);
}


uint64_t DecoderStack::get_annotation_index(
    const Row &row, uint64_t start_sample)
{  
    uint64_t index = 0;
    auto iter = _rows.find(row);
    if (iter != _rows.end())
        index = (*iter).second->get_annotation_index(start_sample);

    return index;
}

uint64_t DecoderStack::get_max_annotation(const Row &row)
{ 
    auto iter =  _rows.find(row);
    if (iter != _rows.end())
        return (*iter).second->get_max_annotation();

    return 0;
}

uint64_t DecoderStack::get_min_annotation(const Row &row)
{  
    auto iter = _rows.find(row);
    if (iter != _rows.end())
        return (*iter).second->get_min_annotation();

    return 0;
}

std::map<const decode::Row, bool> DecoderStack::get_rows_gshow()
{
    std::map<const decode::Row, bool> rows_gshow;
    for (std::map<const decode::Row, bool>::const_iterator i = _rows_gshow.begin();
        i != _rows_gshow.end(); i++) {
        rows_gshow[(*i).first] = (*i).second;
    }
    return rows_gshow;
}

std::map<const decode::Row, bool> DecoderStack::get_rows_lshow()
{
    std::map<const decode::Row, bool> rows_lshow;
    for (std::map<const decode::Row, bool>::const_iterator i = _rows_lshow.begin();
        i != _rows_lshow.end(); i++) {
        rows_lshow[(*i).first] = (*i).second;
    }
    return rows_lshow;
}

void DecoderStack::set_rows_gshow(const decode::Row row, bool show)
{
    std::map<const decode::Row, bool>::const_iterator iter = _rows_gshow.find(row);
    if (iter != _rows_gshow.end()) {
        _rows_gshow[row] = show;
    }
}

void DecoderStack::set_rows_lshow(const decode::Row row, bool show)
{
    std::map<const decode::Row, bool>::const_iterator iter = _rows_lshow.find(row);
    if (iter != _rows_lshow.end()) {
        _rows_lshow[row] = show;
    }
}

bool DecoderStack::has_annotations(const Row &row)
{  
    auto iter =
        _rows.find(row);
    if (iter != _rows.end())
        if(0 == (*iter).second->get_max_sample())
            return false;
        else
            return true;
    else
        return false;
}

uint64_t DecoderStack::list_annotation_size()
{
    std::lock_guard<std::mutex> lock(_output_mutex);
    uint64_t max_annotation_size = 0;

    for (auto it = _rows.begin(); it != _rows.end(); it++) {
        auto iter = _rows_lshow.find((*it).first);
        if (iter != _rows_lshow.end() && (*iter).second){
            max_annotation_size = max(max_annotation_size,
                (*it).second->get_annotation_size());
        }
    }

    return max_annotation_size;
}

uint64_t DecoderStack::list_annotation_size(uint16_t row_index)
{ 
    for (auto i = _rows.begin(); i != _rows.end(); i++) {
        auto iter = _rows_lshow.find((*i).first);
        if (iter != _rows_lshow.end() && (*iter).second)
            if (row_index-- == 0) {
                return (*i).second->get_annotation_size();
            }
    }
    return 0;
}

bool DecoderStack::list_annotation(pv::data::decode::Annotation *ann,
                                  uint16_t row_index, uint64_t col_index)
{ 
    for (auto i = _rows.begin(); i != _rows.end(); i++) {
        auto iter = _rows_lshow.find((*i).first);
        if (iter != _rows_lshow.end() && (*iter).second) {
            if (row_index-- == 0) {
                return (*i).second->get_annotation(ann, col_index);
            }
        }
    }

    return false;
}


bool DecoderStack::list_row_title(int row, QString &title)
{ 
    for (auto i = _rows.begin();i != _rows.end(); i++) {
        auto iter = _rows_lshow.find((*i).first);
        if (iter != _rows_lshow.end() && (*iter).second) {
            if (row-- == 0) {
                title = (*i).first.title();
                return 1;
            }
        }
    }
    return 0;
}

void DecoderStack::clear()
{
    init();
}

void DecoderStack::init()
{
    _sample_count = 0; 
    _samples_decoded = 0;
    _error_message = QString();
    _no_memory = false;
    _snapshot = NULL;
    _result_count = 0;

    for (auto i = _rows.begin();i != _rows.end(); i++) { 
        (*i).second->clear();
    }

    set_mark_index(-1);
}
 
void DecoderStack::stop_decode_work()
{
    if (_stask_stauts) {
        _stask_stauts->_bStop = true;
    }
    _decode_state = Stopped;
    join_own_thread();
}

void DecoderStack::begin_decode_work()
{
    // _own_thread may still be joinable() if it exited but wasn't joined yet
    // (it will have set _decode_state = Stopped, so the assert passes)
    assert(_decode_state == Stopped);
    join_own_thread();
    _error_message = "";
    _decode_state = Running;
    _own_thread = std::thread([this](){
        do_decode_work();
        _decode_state = Stopped;
        bool was_cancelled = _stask_stauts && _stask_stauts->_bStop;
        if (!was_cancelled && !_session->is_closed()) {
            emit decode_done();
        }
    });
}

bool DecoderStack::check_required_probes()
{
    for(auto dec : _stack){
		if (!dec->have_required_probes()) {
			return false;
		}
    }

    return true;
}

void DecoderStack::do_decode_work()
{
    //set the flag to exit from task thread 
     if (_stask_stauts){
         _stask_stauts->_bStop = true;
     }
     _stask_stauts = new decode_task_status();
     _stask_stauts->_bStop = false;
     _stask_stauts->_decoder = this;
     _decoder_status->clear(); //clear old items

    if (!_options_changed)
    {  
        dsv_err("ERROR:Decoder options have not changed.");
        return;
    } 
    _options_changed = false;

    init();

    _snapshot = NULL;

	// Check that all decoders have the required channels
    if (!check_required_probes()) {
        _error_message = tr("One or more required channels have not been specified");
        dsv_err("ERROR:%s", _error_message.toStdString().c_str());
        return;
	}

	// We get the logic data of the first channel in the list.
	// This works because we are currently assuming all
	// LogicSignals have the same data/snapshot
    for (auto dec : _stack) {
        if (dec->have_probes()) {
            for(auto s :  _session->get_signals()) {
                if(s->get_index() == dec->first_probe_index() && s->signal_type() == SR_CHANNEL_LOGIC)
                { 
                    _snapshot = ((pv::view::LogicSignal*)s)->data();
                    if (_snapshot != NULL)
                        break;
                }
            }
            if (_snapshot != NULL)
                break;
        }
    }

	if (_snapshot == NULL)
    {   
        _error_message = tr("One or more required channels have not been specified");
        dsv_err("ERROR:%s", _error_message.toStdString().c_str());
        return;
    }		

    if (_session->is_realtime_refresh() == false && _snapshot->empty())
    { 
        dsv_err("ERROR:Decode data is empty.");
        return;
    }

    // Get the samplerate
	_samplerate = _snapshot->samplerate();
    if (_samplerate == 0.0)
    {
        dsv_err("ERROR:Decode data got an invalid sample rate.");
        return;
    }
     
    execute_decode_stack();   
}

uint64_t DecoderStack::get_max_sample_count()
{
	uint64_t max_sample_count = 0;

    for (auto i = _rows.begin(); i != _rows.end(); i++){
        max_sample_count = max(max_sample_count, (*i).second->get_max_sample());
    } 	

	return max_sample_count;
}

void DecoderStack::decode_data(const uint64_t decode_start, const uint64_t decode_end, srd_session *const session)
{
    decode_task_status *status = _stask_stauts;

    //uint8_t *chunk = NULL;
    uint64_t last_cnt = 0;
    uint64_t notify_cnt = (decode_end - decode_start + 1)/100;
    srd_decoder_inst *logic_di = NULL;

    // find the first level decoder instant
    for (GSList *d = session->di_list; d; d = d->next) {
        srd_decoder_inst *di = (srd_decoder_inst *)d->data;
        srd_decoder *decoder = di->decoder;
        const bool have_probes = (decoder->channels || decoder->opt_channels) != 0;
        if (have_probes) {
            logic_di = di;
            break;
        }
    }

    assert(logic_di);

    uint64_t entry_cnt = 0;
    uint64_t i = decode_start;
    char *error = NULL; 
    bool bError = false;
    bool bEndTime = false;
    //struct srd_push_param push_param;

    if( i >= decode_end){
        dsv_info("decode data index have been to end");
    }

    std::vector<const uint8_t *> chunk;
    std::vector<uint8_t> chunk_const;

    bool bCheckEnd = false;
    uint64_t end_index = decode_end;

    _progress = 0;
    uint64_t sended_len  = 0;
    _is_decoding = true;

    void* lbp_array[35];

    for (int j =0 ; j < logic_di->dec_num_channels; j++){
        lbp_array[j] = NULL;
    }
  
    while(i < end_index && !_no_memory && !status->_bStop)
    {
        chunk.clear();
        chunk_const.clear();

        if (_is_capture_end)
        {
            if (!bCheckEnd){
                bCheckEnd = true;

                uint64_t align_sample_count = _snapshot->get_ring_sample_count();

                if (end_index >= align_sample_count){
                    end_index = align_sample_count - 1;
                    dsv_info("Reset the decode end sample, new:%llu, old:%llu", 
                        (u64_t)end_index, (u64_t)decode_end);
                }

                if (i >= align_sample_count){
                    dsv_info("ERROR: the decoding sample index is out of range.");
                    break;
                }
            }
        }
        else if (i >= _snapshot->get_ring_sample_count())
        {   
            // Wait the data is ready.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (_is_capture_end && i == _snapshot->get_ring_sample_count()){
            break;
        }

        uint64_t chunk_end = end_index;

        for (int j =0 ; j < logic_di->dec_num_channels; j++) {
            int sig_index = logic_di->dec_channelmap[j];
            void *lbp = NULL;

            if (sig_index == -1) {
                chunk.push_back(NULL);
                chunk_const.push_back(0);
            }
            else {
                if (_snapshot->has_data(sig_index)) {
                    const uint8_t *data_ptr = _snapshot->get_samples(i, chunk_end, sig_index, &lbp);
                    chunk.push_back(data_ptr);
                    chunk_const.push_back(_snapshot->get_sample(i, sig_index));

                    if (_snapshot->is_able_free() == false)
                    {
                        if (lbp_array[j] != lbp){
                            if (lbp_array[j] != NULL)
                                _snapshot->free_decode_lpb(lbp_array[j]);
                            lbp_array[j] = lbp;
                        }
                    }
                }
                else {
                    _error_message = tr("At least one of selected channels are not enabled.");
                    return;
                }
            }
        }

        if (chunk_end > end_index)
            chunk_end = end_index;
        if (chunk_end - i > MaxChunkSize)
            chunk_end = i + MaxChunkSize;

        bEndTime = (chunk_end == end_index);

        if (srd_session_send(
                session,
                i,
                chunk_end,
                chunk.data(),
                chunk_const.data(),
                chunk_end - i,
                &error) != SRD_OK){

            if (error){
                _error_message = QString::fromLocal8Bit(error);
                dsv_err("Failed to call srd_session_send:%s", error);
                g_free(error);
                error = NULL;
            }

            bError = true;
            break;
        }

        sended_len += chunk_end - i; 
        _progress = (int)(sended_len * 100 / end_index);

        i = chunk_end;       

        //use mutex
        {
            std::lock_guard<std::mutex> lock(_output_mutex);
            _samples_decoded = i - decode_start + 1;
        }

        if ((i - last_cnt) > notify_cnt) {
            last_cnt = i;
            new_decode_data();
        }

        entry_cnt++;
    }

    _progress = 100;
    _is_decoding = false;
    
    new_decode_data();

    // the task is normal ends,so all samples was processed;
    if (!bError && bEndTime){
       srd_session_end(session, &error);

        if (error != NULL){
            _error_message = QString::fromLocal8Bit(error);
            dsv_err("Failed to call srd_session_end:%s", error);
        }
    }
 
    dsv_info("%s%llu", "send to decoder times: ", (u64_t)entry_cnt);

    if (error != NULL)
        g_free(error);
  
}

void DecoderStack::execute_decode_stack()
{
    if (_use_c_decoder) {
        execute_c_decode_stack();
        return;
    }

    // Acquire the global srd mutex before touching any srd_session.
    // libsigrokdecode's global `sessions` list is unprotected; concurrent
    // srd_session_new / srd_session_destroy calls race with the Python
    // decoder glib threads that call srd_inst_find_by_obj(), leading to
    // SIGSEGV (far=0x10, NULL+offsetof(srd_pd_output,di)).
    // C-decoder sessions bypass libsigrokdecode entirely and skip this lock.
    std::lock_guard<std::mutex> _srd_lock(s_srd_session_mutex);

	srd_session *session = NULL;
	srd_decoder_inst *prev_di = NULL;
    uint64_t decode_start = 0;
    uint64_t decode_end = 0;

	assert(_snapshot);

	// Create the session
    // one decoderstatck onwer one session
    // all decoderstatck execute in sequence
	srd_session_new(&session);

    if (session == NULL){
        dsv_err("Failed to call srd_session_new()");
        assert(false);
    }
    
    // Get the intial sample count
    _sample_count = _snapshot->get_ring_sample_count();
 
    // Create the decoders
    for(auto dec : _stack)
	{
        srd_decoder_inst *const di = dec->create_decoder_inst(session);

		if (!di)
		{
			_error_message =tr("Failed to create decoder instance");
			srd_session_destroy(session);
			return;
		}

		if (prev_di)
			srd_inst_stack (session, prev_di, di);

		prev_di = di;
        decode_start = dec->decode_start();

        if (_session->is_realtime_refresh() == false)
            decode_end = min(dec->decode_end(), _sample_count-1);
        else
            decode_end = max(dec->decode_end(), decode_end);
	}

    dsv_info("decoder start sample:%llu, end sample:%llu, count:%llu", 
            (u64_t)decode_start, (u64_t)decode_end, (u64_t)(decode_end - decode_start + 1));

	// Start the session
	srd_session_metadata_set(session, SRD_CONF_SAMPLERATE,
		g_variant_new_uint64((uint64_t)_samplerate));

	srd_pd_output_callback_add(
                    session, 
                    SRD_OUTPUT_ANN,
		            DecoderStack::annotation_callback,
                    _stask_stauts);

    char *error = NULL;
    if (srd_session_start(session, &error) == SRD_OK){
       //need a lot time
        decode_data(decode_start, decode_end, session);
    }
    else if (error != NULL){
        _error_message = QString::fromLocal8Bit(error);
    }

	// Destroy the session
    if (error != NULL) {
        g_free(error);
    }

	srd_session_destroy(session); 
}

uint64_t DecoderStack::sample_count()
{
    if (_snapshot)
        return _snapshot->get_sample_count();
    else
        return 0;
}

uint64_t DecoderStack::sample_rate()
{
    return _samplerate;
}

//the decode callback, annotation object will be create
void DecoderStack::annotation_callback(srd_proto_data *pdata, void *self)
{
	assert(pdata);
	assert(self);

    struct decode_task_status *st = (decode_task_status*)self;

	DecoderStack *const d = st->_decoder;
	assert(d);

    if (st->_bStop){ 
        return;
    }
    if (d->_decoder_status == NULL){ 
        dsv_err("decode task was deleted.");
        assert(false);
    }
  
    if (d->_no_memory) {
        return;
    }

    Annotation *a = new Annotation(pdata, d->_decoder_status);
    if (a == NULL){
        d->_no_memory = true;
        return;     
    }
    d->_result_count++;

    /* -------- [DEBUG] dump every Python-decoder annotation --------------
     * Mirror of the ANN-C dump in c_put_annotation_cb. Guarded by:
     *   (a) DSV_DUMP_ANN env var (same switch),
     *   (b) !_use_c_decoder so the C path (which also routes through this
     *       callback after c_put_annotation_cb) does not produce ANN-PY
     *       duplicates of its own ANN-C output.
     * Format: ANN-PY,<ss>,<es>,<class>,-,-,"<text>"
     * The empty row_c / class_local_c slots keep the column count identical
     * to ANN-C so awk/cut field indexing matches on both streams.
     * -------------------------------------------------------------------- */
    static const bool s_dump_ann_py = (std::getenv("DSV_DUMP_ANN") != nullptr);
    if (s_dump_ann_py && !d->_use_c_decoder) {
        srd_proto_data_annotation *ad =
            static_cast<srd_proto_data_annotation*>(pdata->data);
        const char *txt = (ad && ad->ann_text && ad->ann_text[0])
                        ? ad->ann_text[0] : "";
        std::fprintf(stderr,
                     "ANN-PY,%llu,%llu,%d,t=%d,-,\"%s\"\n",
                     (unsigned long long)pdata->start_sample,
                     (unsigned long long)pdata->end_sample,
                     ad ? ad->ann_class : -1,
                     ad ? ad->ann_type : -1,
                     txt);
        std::fflush(stderr);
    }

	// Find the row
	// These are asserted in debug; add explicit guards for release builds
	// to prevent a NULL-dereference crash (far=0x10 = NULL+offsetof(pdo,di)).
	if (!pdata->pdo || !pdata->pdo->di || !pdata->pdo->di->decoder) {
	    delete a;
	    return;
	}
	assert(pdata->pdo);
	assert(pdata->pdo->di);
	const srd_decoder *const decc = pdata->pdo->di->decoder;
	assert(decc);

    auto row_iter = d->_rows.end();
	
	// Try looking up the sub-row of this class
	const map<pair<const srd_decoder*, int>, Row>::const_iterator r =
        d->_class_rows.find(make_pair(decc, a->format()));
	if (r != d->_class_rows.end())
        row_iter = d->_rows.find((*r).second);
	else
	{
		// Failing that, use the decoder as a key
        row_iter = d->_rows.find(Row(decc));
	}

    assert(row_iter != d->_rows.end());
    if (row_iter == d->_rows.end()) {
        dsv_err("Unexpected annotation: decoder = 0x%x, format = %d", (void*)decc, a->format());
        assert(0);
        return;
    }

	// Add the annotation 
    if (!(*row_iter).second->push_annotation(a))
        d->_no_memory = true; 
}
 
void DecoderStack::frame_ended()
{ 
    _options_changed = true; 
}

int DecoderStack::list_rows_size()
{ 
    int rows_size = 0;
    for (auto i = _rows.begin(); i != _rows.end(); i++) {
        auto iter = _rows_lshow.find((*i).first);
        if (iter != _rows_lshow.end() && (*iter).second)
            rows_size++;
    }
    return rows_size;
}

bool DecoderStack::options_changed()
{
    return _options_changed;
}

void DecoderStack::set_options_changed(bool changed)
{
    _options_changed = changed;
}

bool DecoderStack::out_of_memory()
{
    return _no_memory;
}

void DecoderStack::set_mark_index(int64_t index)
{
    _mark_index = index;
}

int64_t DecoderStack::get_mark_index()
{
    return _mark_index;
}

const char* DecoderStack::get_root_decoder_id()
{
    if (_stack.size() > 0){
        decode::Decoder *dec = _stack.front();
        return dec->decoder()->id;
    }
    return NULL;
}

/* Unpack `chunk_len` consecutive logic samples starting at absolute sample
 * index `start_sample` from a LogicSnapshot packed-bit byte view (as returned
 * by LogicSnapshot::get_samples) into an output buffer where each byte is
 * either 0 or 1.
 *
 * LogicSnapshot stores each channel as a packed-bit stream: 64 consecutive
 * samples per uint64_t, sample `s` lives in bit `s & 63` of word
 * `(s & LeafMask) >> 6` -- equivalently bit `s & 7` of byte
 * `(s & LeafMask) >> 3`. See LogicSnapshot::get_sample_self for the
 * canonical reader of this layout. `get_samples()` already byte-aligned the
 * returned pointer to `(start_sample & ~7)`, so the in-byte offset of
 * `start_sample` from `packed[0]` is `start_sample & 7`.
 *
 * Without this unpack the host would hand the C decoder the raw packed
 * stream, which the decoder must interpret as 0/1-per-byte per the
 * c_decoder_api.h contract, causing a 1:8 sample-index distortion in every
 * downstream annotation.
 *
 * Cost: ~chunk_len simple integer ops per channel per chunk, completely
 * dominated by the surrounding decode work.
 */
static inline void c_unpack_packed_bits(const uint8_t *packed,
                                        uint64_t start_sample,
                                        uint64_t chunk_len,
                                        uint8_t *out_unpacked)
{
    const uint64_t bit_off = start_sample & 7u;
    for (uint64_t k = 0; k < chunk_len; k++) {
        const uint64_t bit_pos = bit_off + k;
        out_unpacked[k] =
            static_cast<uint8_t>((packed[bit_pos >> 3] >> (bit_pos & 7u)) & 1u);
    }
}

// Helper context for the C put_annotation callback
struct CPutFnCtx {
    DecoderStack *stack;
    decode_task_status *status;
    srd_pd_output *fake_pdo;
    const std::vector<std::vector<int>> *row_class_map;
    volatile int *stop_flag;
};

static void c_put_annotation_cb(void *ctx, uint64_t ss, uint64_t es,
                                 unsigned int ann_row, unsigned int ann_class,
                                 const char *text)
{
    CPutFnCtx *pctx = static_cast<CPutFnCtx*>(ctx);
    if (pctx->status->_bStop || pctx->stack->out_of_memory()) {
        *(pctx->stop_flag) = 1;
        return;
    }

    int global_class = static_cast<int>(ann_class);
    const auto &rcm = *(pctx->row_class_map);
    if (ann_row < rcm.size() && ann_class < rcm[ann_row].size()) {
        global_class = rcm[ann_row][ann_class];
    }

    /* -------- [DEBUG] dump every C-decoder annotation -----------------
     * Enable at runtime with: export DSV_DUMP_ANN=1
     * Format: ANN-C,<ss>,<es>,<class_py>,<row_c>,<class_local_c>,"<text>"
     * - class_py is the Python annotation tuple index after row_class_map
     *   translation; identical numbering as ANN-PY rows so the two streams
     *   can be diff'd field-for-field.
     * Output goes to stderr (NOT dsv_info) to avoid flooding the UI log
     * panel with thousands of bit-level annotations.
     * ------------------------------------------------------------------ */
    static const bool s_dump_ann = (std::getenv("DSV_DUMP_ANN") != nullptr);
    if (s_dump_ann) {
        std::fprintf(stderr,
                     "ANN-C,%llu,%llu,%d,%u,%u,\"%s\"\n",
                     (unsigned long long)ss,
                     (unsigned long long)es,
                     global_class,
                     ann_row,
                     ann_class,
                     text ? text : "");
        std::fflush(stderr);
    }

    const char *texts[2] = { text ? text : "", nullptr };

    /* libsigrokdecode assigns ann_type starting at 7 for the first 2-tuple
     * annotation class, incrementing by 1 per class (decoder.c:465-513).
     * Mirror that here so C decoder annotations land on the same Colours[]
     * slots as the Python decoder — matching colours row-for-row. */
    srd_proto_data_annotation ann_data = {};
    ann_data.ann_class = global_class;
    ann_data.ann_type  = 7 + global_class;
    ann_data.ann_text  = const_cast<char**>(texts);

    srd_proto_data pdata = {};
    pdata.start_sample = ss;
    pdata.end_sample   = es;
    pdata.pdo          = pctx->fake_pdo;
    pdata.data         = &ann_data;

    DecoderStack::annotation_callback(&pdata, pctx->status);
}

/* Normalize a label for fuzzy matching: lowercase, strip non-alphanumeric.
 * So "MOSI-bit", "mosi_bit", "mosi bit" and "MosiBit" all collapse to
 * "mosibit". Writes up to dst_cap-1 chars and NUL-terminates dst. */
static void normalize_label(const char *src, char *dst, size_t dst_cap)
{
    size_t w = 0;
    if (!dst || dst_cap == 0) return;
    if (!src) { dst[0] = 0; return; }
    for (const char *p = src; *p && w + 1 < dst_cap; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            dst[w++] = static_cast<char>(c);
    }
    dst[w] = 0;
}

static bool env_flag_enabled(const char *name)
{
    const char *v = std::getenv(name);
    return v && v[0] && !(v[0] == '0' && v[1] == '\0');
}

static QString signal_label_for_index(pv::SigSession *session, int sig_idx)
{
    if (!session || sig_idx < 0)
        return QStringLiteral("-");
    for (auto s : session->get_signals()) {
        if (s->get_index() == sig_idx)
            return s->get_name();
    }
    return QStringLiteral("?");
}

/* SPI[C] vs SPI[Py]: log how each decoder channel maps to LA probes. An
 * unbound MISO on the C trace explains all-zero MISO bytes while Py shows
 * real data. Set DSV_SPI_DBG=1 for per-byte mosi/miso lines from both decoders. */
static void log_spi_decoder_channel_bindings(pv::SigSession *session,
                                             const srd_decoder *root_srd,
                                             decode::Decoder *front_dec,
                                             CDecoderDef *c_def,
                                             const std::vector<int> &sig_indices,
                                             int num_channels)
{
    if (!root_srd || !c_def || std::strcmp(root_srd->id, "spi") != 0)
        return;

    for (int ch = 0; ch < num_channels; ch++) {
        const char *cid = c_def->channel_ids[ch];
        const int idx = sig_indices[ch];
        if (idx >= 0) {
            dsv_info("SPI[C] %s -> LA ch %d (%s)",
                     cid ? cid : "?",
                     idx,
                     signal_label_for_index(session, idx).toUtf8().constData());
        } else {
            dsv_warn("SPI[C] %s -> UNBOUND (logic samples zero-filled for this slot)",
                     cid ? cid : "?");
        }
    }

    if (front_dec) {
        const GSList *lists[2] = { root_srd->channels, root_srd->opt_channels };
        const char *list_names[2] = { "req", "opt" };
        for (int li = 0; li < 2; li++) {
            for (const GSList *l = lists[li]; l; l = l->next) {
                const srd_channel *pdch = static_cast<const srd_channel*>(l->data);
                if (!pdch) continue;
                const int idx = front_dec->binded_probe_index(pdch);
                dsv_info("SPI[Py] %s:%s (%s) -> LA ch %d (%s)",
                         list_names[li],
                         pdch->id ? pdch->id : "?",
                         pdch->name ? pdch->name : "?",
                         idx,
                         signal_label_for_index(session, idx).toUtf8().constData());
            }
        }
    }

    for (int ch = 0; ch < num_channels; ch++) {
        char norm[64];
        normalize_label(c_def->channel_ids[ch], norm, sizeof(norm));
        if (std::strcmp(norm, "miso") == 0 && sig_indices[ch] < 0) {
            dsv_warn("SPI[C] MISO is not assigned to a logic channel — MISO decode "
                     "will be all 00. Bind the same LA pin as on the SPI[Py] trace "
                     "(decoder options). Use DSV_SPI_DBG=1 to compare per-byte output.");
        }
    }

    if (env_flag_enabled("DSV_SPI_DBG")) {
        dsv_info("DSV_SPI_DBG=1: SPI[C] logs on stderr as SPI[C] DATA/TRANSFER; "
                 "SPI[Py] logs as SPI[Py] DATA/TRANSFER (libsigrokdecode stderr).");
    }
}

/* Map a single C decoder channel id (from CDecoderDef::channel_ids[]) to the
 * signal index bound to the matching srd_channel in front_dec. We match by
 * name in a case-insensitive way against srd_channel::id then srd_channel::name
 * (some Python decoders use lowercase ids like "clk" while C decoders tend to
 * use uppercase). Returns -1 when nothing matches or the slot is unbound. */
static int lookup_c_channel_binding(const srd_decoder *root_srd,
                                    decode::Decoder *front_dec,
                                    const char *want)
{
    if (!root_srd || !front_dec || !want)
        return -1;

    char want_norm[64];
    normalize_label(want, want_norm, sizeof(want_norm));
    if (!want_norm[0]) return -1;

    auto matches = [&](const char *cand) {
        char buf[64];
        normalize_label(cand, buf, sizeof(buf));
        return buf[0] && std::strcmp(buf, want_norm) == 0;
    };

    const GSList *lists[2] = { root_srd->channels, root_srd->opt_channels };
    for (const GSList *list : lists) {
        for (const GSList *l = list; l; l = l->next) {
            const srd_channel *pdch = static_cast<const srd_channel*>(l->data);
            if (!pdch) continue;
            if (matches(pdch->id) || matches(pdch->name))
                return front_dec->binded_probe_index(pdch);
        }
    }
    return -1;
}

/* Translate a C decoder ann_class label ("mosi-bit", "miso_byte", ...) to the
 * matching Python decoder's `annotations` tuple index, so the host's
 * _class_rows map can route the annotation into the correct Python row.
 *
 * Why: the host UI is built from the Python decoder's annotations +
 * annotation_rows. annotation_callback looks up _class_rows[(decoder, k)]
 * where k must be the Python tuple index of the class. If the C decoder
 * emits its own flat ann_classes index instead, every annotation either
 * lands in the wrong row or, when the Python row is hidden by default
 * (rows whose title contains "bit" / "warning"), disappears entirely —
 * which is exactly the "no annotations visible" symptom reported.
 *
 * Returns the Python class index on success, -1 if no match. Note that the
 * srd_decoder->annotations GSList is reversed relative to the Python tuple
 * because libsigrokdecode builds it with g_slist_prepend; convert via
 * (g_slist_length - 1 - position) to recover the original tuple index. */
static int find_py_class_index(const srd_decoder *root_srd, const char *want_label)
{
    if (!root_srd || !root_srd->annotations || !want_label) return -1;

    char want_norm[64];
    normalize_label(want_label, want_norm, sizeof(want_norm));
    if (!want_norm[0]) return -1;

    const guint total = g_slist_length(root_srd->annotations);
    guint pos = 0;
    for (const GSList *l = root_srd->annotations; l; l = l->next, ++pos) {
        char **annpair = static_cast<char**>(l->data);
        if (!annpair || !annpair[0]) continue;
        char have_norm[64];
        normalize_label(annpair[0], have_norm, sizeof(have_norm));
        if (have_norm[0] && std::strcmp(have_norm, want_norm) == 0)
            return static_cast<int>(total - 1 - pos);
    }
    return -1;
}

/* Build row_class_map[ann_row][ann_class_within_row] = python_class_index.
 *
 * c_def->ann_row_ids[r] is the C decoder's row id (e.g. "bits"); the entries
 * in c_def->ann_classes are "row_id:label" tokens (e.g. "bits:mosi-bit").
 * For each (row, in-row-position) we find the matching Python class index by
 * fuzzy name match against root_srd->annotations. If no Python class matches
 * we fall back to the flat C index `k` and log a warning so the decoder
 * author can fix their labels. */
static void build_c_decoder_row_class_map(
    const srd_decoder *root_srd, const CDecoderDef *c_def,
    std::vector<std::vector<int>> &row_class_map)
{
    row_class_map.clear();
    if (!c_def || !c_def->ann_row_ids || !c_def->ann_classes) return;

    int nrows = 0;    while (c_def->ann_row_ids[nrows]) nrows++;
    int nclasses = 0; while (c_def->ann_classes[nclasses]) nclasses++;

    std::vector<int> row_count(nrows, 0);
    for (int k = 0; k < nclasses; k++) {
        const char *entry = c_def->ann_classes[k];
        const char *colon = std::strchr(entry, ':');
        if (!colon) continue;
        size_t plen = static_cast<size_t>(colon - entry);
        for (int r = 0; r < nrows; r++) {
            if (std::strlen(c_def->ann_row_ids[r]) == plen &&
                std::strncmp(c_def->ann_row_ids[r], entry, plen) == 0) {
                row_count[r]++;
                break;
            }
        }
    }

    row_class_map.resize(nrows);
    for (int r = 0; r < nrows; r++)
        row_class_map[r].assign(row_count[r], -1);

    std::vector<int> row_idx(nrows, 0);
    for (int k = 0; k < nclasses; k++) {
        const char *entry = c_def->ann_classes[k];
        const char *colon = std::strchr(entry, ':');
        if (!colon) continue;
        size_t plen = static_cast<size_t>(colon - entry);
        for (int r = 0; r < nrows; r++) {
            if (std::strlen(c_def->ann_row_ids[r]) != plen ||
                std::strncmp(c_def->ann_row_ids[r], entry, plen) != 0)
                continue;
            const char *label = colon + 1;
            int py_idx = find_py_class_index(root_srd, label);
            if (py_idx < 0) {
                dsv_warn("C decoder '%s' ann_class label '%s' (row '%s') "
                         "doesn't map to any Python annotation id; falling "
                         "back to flat index %d. Update the C decoder's "
                         "ann_classes to match the Python decoder's "
                         "annotations[] ids exactly.",
                         root_srd ? root_srd->id : "?", label,
                         c_def->ann_row_ids[r], k);
                py_idx = k;
            }
            row_class_map[r][row_idx[r]++] = py_idx;
            break;
        }
    }
}

void DecoderStack::execute_c_decode_stack()
{
    assert(_snapshot);
    const srd_decoder *root_srd = _stack.front()->decoder();
    CDecoderDef *c_def =
        pv::cdecoders::CDecoderRegistry::instance().get_c_decoder_def_by_id(root_srd->id);
    if (!c_def) {
        _error_message = "C decoder definition not found";
        dsv_err("C decoder '%s' not in registry", root_srd->id);
        return;
    }

    /* Dispatch policy (spec Resolved Decision #1):
     * If the decoder is v2 streaming-capable, ALWAYS go through the streaming
     * path, regardless of whether the capture is live or already finished —
     * this matches the Python decoder path (decode_data()).
     * Fall back to the existing batch body only for v1 decoders or v2
     * decoders that only expose `decode`. */
    const bool streaming_capable =
        c_def->api_version >= 2 &&
        c_def->create       != nullptr &&
        c_def->decode_chunk != nullptr &&
        c_def->destroy      != nullptr;
    const bool batch_capable = (c_def->decode != nullptr);

    dsv_info("C decoder dispatch '%s': api=v%u, streaming_capable=%d, batch_capable=%d -> %s",
             root_srd->id, c_def->api_version,
             streaming_capable ? 1 : 0, batch_capable ? 1 : 0,
             streaming_capable ? "STREAMING" : (batch_capable ? "BATCH" : "NONE"));

    if (streaming_capable) {
        execute_c_decode_stack_streaming(c_def);
    } else if (batch_capable) {
        execute_c_decode_stack_batch(c_def);
    } else {
        _error_message = "C decoder exposes neither streaming nor batch entry";
        dsv_err("C decoder '%s' has no usable decode entry", root_srd->id);
    }
}

void DecoderStack::execute_c_decode_stack_streaming(CDecoderDef *c_def)
{
    assert(_snapshot);
    assert(c_def);
    assert(c_def->create && c_def->decode_chunk && c_def->destroy);

    const srd_decoder *root_srd = _stack.front()->decoder();
    decode::Decoder   *front_dec = _stack.front();

    /* ---------- channel layout (same as batch path) ----------
     * c_def->channel_ids[] declares the order in which the C decoder wants
     * its `samples[][i]` rows. We must map each of those slots to a bound
     * LA signal by NAME against the underlying Python decoder's
     * `channels` + `opt_channels`. Positional mapping breaks the moment the
     * C decoder author chooses any order different from the Python
     * decoder's (e.g. Python SPI orders opt_channels as MISO/MOSI/CS while
     * the C example uses MOSI/MISO/CS — positional binding silently swaps
     * MOSI and MISO). */
    int num_channels = 0;
    if (c_def->channel_ids)
        while (c_def->channel_ids[num_channels]) num_channels++;

    std::vector<int> sig_indices(num_channels, -1);
    for (int ch_idx = 0; ch_idx < num_channels; ch_idx++) {
        sig_indices[ch_idx] = lookup_c_channel_binding(
            root_srd, front_dec, c_def->channel_ids[ch_idx]);
    }

    /* ---------- per-run decoder state ---------- */
    void *inst = c_def->create(static_cast<uint64_t>(_samplerate), num_channels);
    if (!inst) {
        _error_message = "C decoder create() returned NULL";
        dsv_err("C decoder '%s' create() returned NULL", root_srd->id);
        return;
    }

    /* ---------- annotation glue (identical to batch path) ---------- */
    std::vector<std::vector<int>> row_class_map;
    build_c_decoder_row_class_map(root_srd, c_def, row_class_map);

    srd_decoder_inst fake_di = {};
    fake_di.decoder = const_cast<srd_decoder*>(root_srd);
    srd_pd_output fake_pdo = {};
    fake_pdo.output_type = SRD_OUTPUT_ANN;
    fake_pdo.di          = &fake_di;
    volatile int stop_flag = 0;
    CPutFnCtx put_ctx = { this, _stask_stauts, &fake_pdo, &row_class_map, &stop_flag };

    /* ---------- streaming loop ---------- */
    uint64_t i         = front_dec->decode_start();
    uint64_t end_index = front_dec->decode_end();
    std::vector<void*>          lbp_per_ch(num_channels, nullptr);
    std::vector<const uint8_t*> ch_ptrs(num_channels, nullptr);
    std::vector<std::vector<uint8_t>> unpacked_per_ch(num_channels);
    std::vector<uint8_t>        zero_buf;   /* on-demand zero-fill for unbound channels */
    bool bCheckEnd = false;
    int  rc = 0;
    bool dumped_unpack_preview = false;
    /* Allow opt-in extra diagnostics (env var matches the ANN dump switch). */
    const bool s_unpack_dbg = (std::getenv("DSV_DUMP_ANN") != nullptr);

    /* Classify channels: "bound" = the user assigned a signal in the decoder
     * options (sig_idx >= 0). We must NOT consult _snapshot->has_data() here
     * because at streaming start the ring is typically empty -> every channel
     * would be misclassified as "unbound" and the loop would never call
     * decode_chunk(). The batch path makes the same choice: it dispatches by
     * sig_idx >= 0 and lets get_samples() decide what to return at fetch
     * time. */
    std::vector<bool> ch_is_bound(num_channels, false);
    for (int ch = 0; ch < num_channels; ch++)
        ch_is_bound[ch] = (sig_indices[ch] >= 0);

    dsv_info("C decoder '%s' streaming START: samplerate=%llu, channels=%d, "
             "decode_start=%llu, decode_end=%llu, is_capture_end=%d, ring=%llu",
             root_srd->id, (unsigned long long)_samplerate, num_channels,
             (unsigned long long)i, (unsigned long long)end_index,
             _is_capture_end ? 1 : 0,
             (unsigned long long)_snapshot->get_ring_sample_count());
    {
        std::string bound_str;
        for (int ch = 0; ch < num_channels; ch++) {
            bound_str += "[";
            bound_str += c_def->channel_ids[ch];
            bound_str += "=sig";
            bound_str += std::to_string(sig_indices[ch]);
            bound_str += ch_is_bound[ch] ? ":bound]" : ":unbound]";
        }
        dsv_info("C decoder '%s' channel map: %s", root_srd->id, bound_str.c_str());
    }
    log_spi_decoder_channel_bindings(_session, root_srd, front_dec, c_def,
                                     sig_indices, num_channels);

    _progress    = 0;
    _is_decoding = true;

    uint64_t loop_iters = 0;
    uint64_t loop_sleeps = 0;
    while (i <= end_index && !_no_memory && !_stask_stauts->_bStop && !stop_flag)
    {
        loop_iters++;
        /* Wait-for-data / end-of-capture alignment — same shape as decode_data() */
        if (_is_capture_end) {
            if (!bCheckEnd) {
                bCheckEnd = true;
                uint64_t avail = _snapshot->get_ring_sample_count();
                if (avail == 0) {
                    dsv_info("C streaming: capture ended with empty ring, exiting");
                    break;
                }
                if (end_index >= avail) {
                    end_index = avail - 1;
                    dsv_info("C streaming: capped end_index to %llu (avail=%llu)",
                             (unsigned long long)end_index,
                             (unsigned long long)avail);
                }
                if (i > end_index) {
                    dsv_info("C streaming: i (%llu) > end_index (%llu), exiting",
                             (unsigned long long)i, (unsigned long long)end_index);
                    break;
                }
            }
        } else if (i >= _snapshot->get_ring_sample_count()) {
            loop_sleeps++;
            if (loop_sleeps == 1 || (loop_sleeps % 50) == 0) {
                dsv_info("C streaming: waiting for data, i=%llu, ring=%llu, sleeps=%llu",
                         (unsigned long long)i,
                         (unsigned long long)_snapshot->get_ring_sample_count(),
                         (unsigned long long)loop_sleeps);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        /* Pass 1: fetch BOUND channels and shrink chunk_end to the minimum end.
         * Two policies depending on whether the capture is still live:
         *   - Live (!_is_capture_end): if a bound channel returns NULL the
         *     ring isn't filled past `i` yet — wait and retry on the next
         *     iteration.
         *   - Ended (_is_capture_end && bCheckEnd): never wait. A NULL here
         *     means the assigned sig_idx is bogus or the snapshot dropped the
         *     range. Match the batch path: leave the channel zero-filled and
         *     keep going so the loop can finish. */
        uint64_t chunk_end       = end_index;
        bool     needs_wait      = false;
        bool     any_bound_ready = false;
        for (int ch = 0; ch < num_channels; ch++) {
            ch_ptrs[ch] = nullptr;
            if (!ch_is_bound[ch]) continue;
            uint64_t ce = end_index;
            void *lbp = nullptr;
            const uint8_t *data = _snapshot->get_samples(i, ce, sig_indices[ch], &lbp);
            if (!data) {
                if (!_is_capture_end) { needs_wait = true; break; }
                /* capture ended, sig_idx unusable -> treat as unbound, zero-fill */
                continue;
            }
            if (ce < chunk_end) chunk_end = ce;
            ch_ptrs[ch] = data;
            any_bound_ready = true;
            if (!_snapshot->is_able_free() && lbp) {
                if (lbp_per_ch[ch] && lbp_per_ch[ch] != lbp)
                    _snapshot->free_decode_lpb(lbp_per_ch[ch]);
                lbp_per_ch[ch] = lbp;
            }
        }

        /* Wait only while capture is still live; once it has ended we must
         * make forward progress (any_bound_ready may be false if every
         * bound channel returned NULL, which we handle by zero-filling). */
        if (!_is_capture_end && (needs_wait || (!any_bound_ready && num_channels > 0))) {
            loop_sleeps++;
            if (loop_sleeps == 1 || (loop_sleeps % 50) == 0) {
                dsv_info("C streaming: bound channel not ready, i=%llu, ring=%llu, sleeps=%llu",
                         (unsigned long long)i,
                         (unsigned long long)_snapshot->get_ring_sample_count(),
                         (unsigned long long)loop_sleeps);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        /* Pass 2: zero-fill UNBOUND channels for the chunk's length so the
         * decoder always sees non-NULL ch[k]. */
        uint64_t chunk_len = chunk_end - i + 1;
        if (zero_buf.size() < chunk_len) zero_buf.assign(chunk_len, 0);
        for (int ch = 0; ch < num_channels; ch++) {
            if (ch_ptrs[ch] == nullptr) ch_ptrs[ch] = zero_buf.data();
        }

        /* Pass 3: unpack packed-bit storage to 0/1 bytes for BOUND channels.
         * LogicSnapshot::get_samples returns a uint8_t view over packed-bit
         * memory (8 samples / byte). The C decoder API contract
         * (c_decoder_api.h) is "channel_samples[ch][k] is 0 or 1", so we
         * must convert before invoking decode_chunk(). UNBOUND channels
         * already point at zero_buf so we leave them alone.
         *
         * This is the fix for the host-side bug that caused C decoders to
         * see a 1:8 sample-index distortion (validated against the SPI
         * Python decoder using DSV_DUMP_ANN=1 logs). */
        for (int ch = 0; ch < num_channels; ch++) {
            if (!ch_is_bound[ch]) continue;
            if (ch_ptrs[ch] == zero_buf.data()) continue;   /* fell back to zero-fill */
            const uint8_t *packed = ch_ptrs[ch];
            auto &unpacked = unpacked_per_ch[ch];
            if (unpacked.size() < chunk_len) unpacked.resize(chunk_len);
            c_unpack_packed_bits(packed, i, chunk_len, unpacked.data());
            ch_ptrs[ch] = unpacked.data();
        }

        /* Optional diagnostics: dump the first chunk's packed -> unpacked
         * mapping for each bound channel so log readers can sanity-check
         * the unpacking against the raw bytes. Cap at the first call and a
         * small prefix to avoid log spam. */
        if (s_unpack_dbg && !dumped_unpack_preview) {
            uint64_t preview_n = chunk_len < 16 ? chunk_len : 16;
            for (int ch = 0; ch < num_channels; ch++) {
                if (!ch_is_bound[ch]) continue;
                if (ch_ptrs[ch] == zero_buf.data()) continue;
                std::string packed_hex, unpacked_str;
                /* We can't recover the original packed pointer here (it's
                 * been replaced with unpacked.data()), so re-derive packed
                 * bytes by re-fetching for the preview only. */
                void *lbp_dbg = nullptr;
                uint64_t ce_dbg = chunk_end;
                const uint8_t *pkd =
                    _snapshot->get_samples(i, ce_dbg, sig_indices[ch], &lbp_dbg);
                if (pkd) {
                    /* Need enough packed bytes to cover preview_n samples,
                     * starting at the in-byte offset (i & 7). */
                    uint64_t need_bytes = ((i & 7u) + preview_n + 7u) / 8u;
                    char hexbuf[8];
                    for (uint64_t b = 0; b < need_bytes; b++) {
                        std::snprintf(hexbuf, sizeof(hexbuf),
                                      b ? " %02X" : "%02X", pkd[b]);
                        packed_hex += hexbuf;
                    }
                    for (uint64_t k = 0; k < preview_n; k++) {
                        unpacked_str += (unpacked_per_ch[ch][k] ? '1' : '0');
                    }
                    dsv_info("C decoder unpack preview [ch=%d %s] "
                             "chunk_start=%llu, bit_off=%llu: "
                             "packed[%llu]=%s -> unpacked[%llu]=%s",
                             ch, c_def->channel_ids[ch],
                             (unsigned long long)i,
                             (unsigned long long)(i & 7u),
                             (unsigned long long)need_bytes,
                             packed_hex.c_str(),
                             (unsigned long long)preview_n,
                             unpacked_str.c_str());
                }
            }
            dumped_unpack_preview = true;
        }

        /* Feed this chunk to the decoder */
        rc = c_def->decode_chunk(inst, i, chunk_end,
                                  num_channels, ch_ptrs.data(),
                                  c_put_annotation_cb, &put_ctx, &stop_flag);
        if (rc != 0) {
            _error_message =
                QString("C decoder decode_chunk returned %1").arg(rc);
            dsv_err("C decoder '%s' decode_chunk returned %d",
                    root_srd->id, rc);
            break;
        }

        /* Progress + UI notify per chunk */
        i = chunk_end + 1;
        uint64_t total = end_index - front_dec->decode_start() + 1;
        _progress = total ? (int)((i - front_dec->decode_start()) * 100 / total) : 100;
        {
            std::lock_guard<std::mutex> lock(_output_mutex);
            _samples_decoded += static_cast<int64_t>(chunk_len);
        }
        new_decode_data();

        if (_is_capture_end && i > end_index) break;
    }

    /* ---------- cleanup ---------- */
    for (int ch = 0; ch < num_channels; ch++) {
        if (lbp_per_ch[ch]) _snapshot->free_decode_lpb(lbp_per_ch[ch]);
        lbp_per_ch[ch] = nullptr;
    }

    c_def->destroy(inst);

    _progress    = 100;
    _is_decoding = false;

    dsv_info("C decoder '%s' streaming EXIT: decoded %llu samples, %llu annotations "
             "(iters=%llu, sleeps=%llu, final i=%llu/end=%llu, capture_end=%d, "
             "bStop=%d, stop_flag=%d, no_memory=%d)",
             root_srd->id,
             (unsigned long long)(i - front_dec->decode_start()),
             (unsigned long long)_result_count,
             (unsigned long long)loop_iters,
             (unsigned long long)loop_sleeps,
             (unsigned long long)i,
             (unsigned long long)end_index,
             _is_capture_end ? 1 : 0,
             _stask_stauts ? (_stask_stauts->_bStop ? 1 : 0) : -1,
             stop_flag,
             _no_memory ? 1 : 0);

    new_decode_data();
}

void DecoderStack::execute_c_decode_stack_batch(CDecoderDef *c_def)
{
    assert(_snapshot);
    assert(c_def);
    assert(c_def->decode);

    const srd_decoder *root_srd = _stack.front()->decoder();
    decode::Decoder *front_dec = _stack.front();
    _sample_count = _snapshot->get_ring_sample_count();
    if (_sample_count == 0) {
        dsv_err("C decoder: snapshot has no samples");
        return;
    }
    uint64_t decode_start = front_dec->decode_start();
    uint64_t decode_end   = front_dec->decode_end();
    if (decode_end >= _sample_count)
        decode_end = _sample_count - 1;
    if (decode_start > decode_end) {
        dsv_err("C decoder: decode_start (%llu) > decode_end (%llu)",
                (unsigned long long)decode_start, (unsigned long long)decode_end);
        return;
    }

    int num_channels = 0;
    if (c_def->channel_ids)
        while (c_def->channel_ids[num_channels]) num_channels++;

    /* Map each c_def->channel_ids[] slot to the bound LA signal by name;
     * see the streaming path for the full rationale. */
    std::vector<int> sig_indices(num_channels, -1);
    for (int ch_idx = 0; ch_idx < num_channels; ch_idx++) {
        sig_indices[ch_idx] = lookup_c_channel_binding(
            root_srd, front_dec, c_def->channel_ids[ch_idx]);
    }
    log_spi_decoder_channel_bindings(_session, root_srd, front_dec, c_def,
                                     sig_indices, num_channels);

    uint64_t num_samples = decode_end - decode_start + 1;
    std::vector<std::vector<uint8_t>> sample_bufs;
    try {
        sample_bufs.resize(num_channels);
        for (int ch = 0; ch < num_channels; ch++)
            sample_bufs[ch].assign(num_samples, 0);
    } catch (const std::bad_alloc &) {
        _no_memory = true;
        _error_message = "Out of memory allocating sample buffers for C decoder";
        dsv_err("C decoder: out of memory allocating %llu x %d bytes",
                (unsigned long long)num_samples, num_channels);
        return;
    }

    std::vector<void*> lbp_per_ch(num_channels, nullptr);

    uint64_t pos = decode_start;
    while (pos <= decode_end && !_stask_stauts->_bStop) {
        uint64_t chunk_end = decode_end;
        for (int ch = 0; ch < num_channels && chunk_end == decode_end; ch++) {
            int sig_idx = sig_indices[ch];
            if (sig_idx < 0 || !_snapshot->has_data(sig_idx)) continue;
            uint64_t ce = decode_end;
            void *lbp = nullptr;
            const uint8_t *data = _snapshot->get_samples(pos, ce, sig_idx, &lbp);
            if (!data) continue;
            chunk_end = (ce < decode_end) ? ce : decode_end;
            // Unpack this channel's packed-bit stream into sample_bufs[ch].
            // Same fix as the streaming path: LogicSnapshot stores 8
            // samples/byte but the C decoder ABI requires 1 byte == 1
            // sample. memcpy'ing the packed view directly would silently
            // cause a 1:8 sample-index distortion in every annotation.
            uint64_t copy_len = chunk_end - pos + 1;
            uint64_t buf_offset = pos - decode_start;
            if (buf_offset + copy_len > num_samples) copy_len = num_samples - buf_offset;
            c_unpack_packed_bits(data, pos, copy_len, &sample_bufs[ch][buf_offset]);
            if (!_snapshot->is_able_free() && lbp) {
                if (lbp_per_ch[ch] && lbp_per_ch[ch] != lbp)
                    _snapshot->free_decode_lpb(lbp_per_ch[ch]);
                lbp_per_ch[ch] = lbp;
            }
            // Fill remaining channels (same unpack treatment)
            for (int ch2 = ch + 1; ch2 < num_channels; ch2++) {
                int sig_idx2 = sig_indices[ch2];
                if (sig_idx2 < 0 || !_snapshot->has_data(sig_idx2)) continue;
                uint64_t ce2 = chunk_end;
                void *lbp2 = nullptr;
                const uint8_t *data2 = _snapshot->get_samples(pos, ce2, sig_idx2, &lbp2);
                if (!data2) continue;
                uint64_t clen2 = (ce2 < chunk_end ? ce2 : chunk_end) - pos + 1;
                if (pos - decode_start + clen2 > num_samples) clen2 = num_samples - (pos - decode_start);
                c_unpack_packed_bits(data2, pos, clen2,
                                     &sample_bufs[ch2][pos - decode_start]);
                if (!_snapshot->is_able_free() && lbp2) {
                    if (lbp_per_ch[ch2] && lbp_per_ch[ch2] != lbp2)
                        _snapshot->free_decode_lpb(lbp_per_ch[ch2]);
                    lbp_per_ch[ch2] = lbp2;
                }
            }
            break;
        }

        // Update progress during extraction (0-50%)
        {
            uint64_t extracted = pos - decode_start + (chunk_end - pos + 1);
            _progress = (int)(extracted * 50 / (num_samples > 0 ? num_samples : 1));
        }

        pos = chunk_end + 1;
    }

    // Release any still-held lock buffer pointers
    for (int ch = 0; ch < num_channels; ch++) {
        if (lbp_per_ch[ch])
            _snapshot->free_decode_lpb(lbp_per_ch[ch]);
        lbp_per_ch[ch] = nullptr;
    }

    if (_stask_stauts->_bStop) return;

    std::vector<std::vector<int>> row_class_map;
    build_c_decoder_row_class_map(root_srd, c_def, row_class_map);

    srd_decoder_inst fake_di = {};
    fake_di.decoder = const_cast<srd_decoder*>(root_srd);

    srd_pd_output fake_pdo = {};
    fake_pdo.output_type = SRD_OUTPUT_ANN;
    fake_pdo.di          = &fake_di;

    volatile int stop_flag = 0;

    CPutFnCtx put_ctx = {
        this, _stask_stauts, &fake_pdo, &row_class_map, &stop_flag
    };

    std::vector<const uint8_t*> ch_ptrs(num_channels);
    for (int ch = 0; ch < num_channels; ch++)
        ch_ptrs[ch] = sample_bufs[ch].data();

    _progress = 0;
    _is_decoding = true;

    int rc = c_def->decode(
        static_cast<uint64_t>(_samplerate),
        decode_start,
        decode_end,
        num_channels,
        ch_ptrs.data(),
        c_put_annotation_cb,
        &put_ctx,
        &stop_flag
    );

    // Update samples_decoded
    {
        std::lock_guard<std::mutex> lock(_output_mutex);
        _samples_decoded = static_cast<int64_t>(num_samples);
    }
    _progress   = 100;
    _is_decoding = false;

    if (rc != 0 && !_stask_stauts->_bStop) {
        _error_message = QString("C decoder returned error: %1").arg(rc);
        dsv_err("C decoder '%s' decode() returned %d", root_srd->id, rc);
    }

    dsv_info("C decoder '%s' decoded %llu samples, %llu annotations",
             root_srd->id, (unsigned long long)num_samples, (unsigned long long)_result_count);
    new_decode_data();
}

} // namespace data
} // namespace pv
