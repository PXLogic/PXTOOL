/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 * 
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

#include "deviceagent.h"
#include <assert.h>
#include <glib.h>
#include "log.h"
#include "deviceagentcustomconfig.h"

extern "C" {
#include "libsigrok-internal.h"
}

namespace {

void free_custom_mode_list(GSList *list)
{
    if (list)
        g_slist_free(list);
}

}

DeviceAgent::DeviceAgent()
{
    _dev_handle = NULL_HANDLE;
    _di = NULL;
    _dev_type = 0;
    _callback = NULL;
    _is_new_device = false;
    _custom_device = false;
    _custom_work_mode = LOGIC;
    _custom_sample_rate = 0;
    _custom_sample_limit = 0;
    _custom_mode_list = nullptr;
}

void DeviceAgent::update()
{
    _dev_handle = NULL_HANDLE;
    _dev_name = "";
    _path = "";
    _di = NULL;
    _dev_type = 0;
    _is_new_device = false;
    _custom_device = false;
    _custom_work_mode = LOGIC;
    _custom_sample_rate = 0;
    _custom_sample_limit = 0;
    free_custom_mode_list(_custom_mode_list);
    _custom_mode_list = nullptr;

    struct ds_device_full_info info;

    if (ds_get_actived_device_info(&info) == SR_OK)
    {
        _dev_handle = info.handle;
        _dev_type = info.dev_type;
        _di = info.di;
        _is_new_device = info.actived_times == 1;

        _dev_name = QString::fromLocal8Bit(info.name);
        _driver_name = QString::fromLocal8Bit(info.driver_name);

        if (info.path[0] != '\0'){
            _path = QString::fromLocal8Bit(info.path);
        } 
    }
}

 sr_dev_inst* DeviceAgent::inst()
 {
    assert(_dev_handle);
    return _di;
 }

bool DeviceAgent::enable_probe(const sr_channel *probe, bool enable)
{
    assert(_dev_handle);

    if (_custom_device) {
        sr_channel *editable = const_cast<sr_channel *>(probe);
        if (!editable)
            return false;
        editable->enabled = enable;
        config_changed();
        return true;
    }

    if (ds_enable_device_channel(probe, enable) == SR_OK){
        config_changed();
        return true;
    }
    return false;
}

bool DeviceAgent::enable_probe(int probe_index, bool enable)
{
     assert(_dev_handle);

     if (_custom_device) {
        for (GSList *l = get_channels(); l; l = l->next) {
            sr_channel *probe = (sr_channel *)l->data;
            if (probe->index == probe_index) {
                probe->enabled = enable;
                config_changed();
                return true;
            }
        }
        return false;
     }

     if (ds_enable_device_channel_index(probe_index, enable) == SR_OK){
        config_changed();
        return true;
    }
    return false;
}

bool DeviceAgent::set_channel_name(int ch_index, const char *name)
{
    assert(_dev_handle);

    if (_custom_device) {
        if (!name)
            return false;
        for (GSList *l = get_channels(); l; l = l->next) {
            sr_channel *probe = (sr_channel *)l->data;
            if (probe->index == ch_index) {
                g_free(probe->name);
                probe->name = g_strdup(name);
                config_changed();
                return true;
            }
        }
        return false;
    }
    
    if (ds_set_device_channel_name(ch_index, name) == SR_OK){
        config_changed();
        return true;
    }
    return false;
}

uint64_t DeviceAgent::get_sample_limit()
{
    assert(_dev_handle);

    if (_custom_device)
        return _custom_sample_limit;

    uint64_t v;
    GVariant* gvar = NULL;

    ds_get_actived_device_config(NULL, NULL, SR_CONF_LIMIT_SAMPLES, &gvar);

	if (gvar != NULL) {
        v = g_variant_get_uint64(gvar);
		g_variant_unref(gvar);
	}
    else {
		v = 0U;
	}

	return v;
}

uint64_t DeviceAgent::get_sample_rate()
{
    assert(_dev_handle);

    if (_custom_device)
        return _custom_sample_rate;

    uint64_t v;
    GVariant* gvar = NULL;

    ds_get_actived_device_config(NULL, NULL, SR_CONF_SAMPLERATE, &gvar);

	if (gvar != NULL) {
        v = g_variant_get_uint64(gvar);
		g_variant_unref(gvar);
	}
    else {
		v = 0U;
	}

	return v;
}

uint64_t DeviceAgent::get_time_base()
{
    assert(_dev_handle);

    if (_custom_device)
        return 0;

    uint64_t v;
    GVariant* gvar = NULL;

    ds_get_actived_device_config(NULL, NULL, SR_CONF_TIMEBASE, &gvar);

	if (gvar != NULL) {
        v = g_variant_get_uint64(gvar);
		g_variant_unref(gvar);
	}
    else {
		v = 0U;
	}

	return v;
}

double DeviceAgent::get_sample_time()
{
    assert(_dev_handle);

    uint64_t sample_rate = get_sample_rate();
    uint64_t sample_limit = get_sample_limit();
    double sample_time;

    if (sample_rate == 0)
        sample_time = 0;
    else
        sample_time = sample_limit * 1.0 / sample_rate;

    return sample_time;
}

const GSList* DeviceAgent::get_device_mode_list()
{
    assert(_dev_handle);
    if (_custom_device)
        return _custom_mode_list;
    return ds_get_actived_device_mode_list();
}

bool DeviceAgent::is_trigger_enabled()
{
    assert(_dev_handle);
    if (_custom_device)
        return false;
    if (ds_trigger_is_enabled() > 0){
        return true;
    }
    return false;
}

bool DeviceAgent::start()
{
    assert(_dev_handle);

    if (_custom_device)
        return false;

    if (ds_start_collect() == SR_OK){
        return true;
    }
    return false;
}

bool DeviceAgent::stop()
{
    assert(_dev_handle);

    if (_custom_device)
        return false;

    if (ds_stop_collect() == SR_OK){
        return true;
    }
    return false;
}

void DeviceAgent::release()
{
    if (_custom_device) {
        if (_di)
            sr_dev_inst_free(_di);
        _dev_handle = NULL_HANDLE;
        _di = NULL;
        _dev_name.clear();
        _driver_name.clear();
        _path.clear();
        _dev_type = 0;
        _is_new_device = false;
        _custom_device = false;
        _custom_work_mode = LOGIC;
        _custom_sample_rate = 0;
        _custom_sample_limit = 0;
        free_custom_mode_list(_custom_mode_list);
        _custom_mode_list = nullptr;
        return;
    }

    if (_dev_handle == NULL_HANDLE)
        return;

    struct ds_device_full_info active_info;
    if (ds_get_actived_device_info(&active_info) == SR_OK &&
        active_info.handle == _dev_handle) {
        ds_release_actived_device();
    }

    _dev_handle = NULL_HANDLE;
    _di = NULL;
    _dev_name.clear();
    _driver_name.clear();
    _path.clear();
    _dev_type = 0;
    _is_new_device = false;
    _custom_sample_rate = 0;
    _custom_sample_limit = 0;
}

void DeviceAgent::bind_custom_device(struct sr_dev_inst *di,
                                     int dev_type,
                                     int work_mode,
                                     const QString &name,
                                     const QString &path,
                                     const QString &driver_name,
                                     uint64_t sample_rate,
                                     uint64_t sample_limit)
{
    if (_custom_device && _di && _di != di)
        sr_dev_inst_free(_di);

    _dev_handle = di ? (ds_device_handle)di : NULL_HANDLE;
    _di = di;
    _dev_type = dev_type;
    _dev_name = name;
    _path = path;
    _driver_name = driver_name;
    _is_new_device = false;
    _custom_device = (di != nullptr);
    _custom_work_mode = work_mode;
    _custom_sample_rate = sample_rate;
    _custom_sample_limit = sample_limit;
    free_custom_mode_list(_custom_mode_list);
    _custom_mode_list = nullptr;
    _custom_mode_entry.mode = work_mode;
    _custom_mode_entry.name = "Imported Data";
    _custom_mode_entry.acronym = "import";
    _custom_mode_list = g_slist_append(nullptr, &_custom_mode_entry);
}

bool DeviceAgent::have_enabled_channel()
{
    assert(_dev_handle);
    if (_custom_device) {
        for (GSList *l = get_channels(); l; l = l->next) {
            const sr_channel *probe = (const sr_channel *)l->data;
            if (probe->enabled)
                return true;
        }
        return false;
    }
    return ds_channel_is_enabled() > 0;
}

void DeviceAgent::config_changed()
{
    if (_callback != NULL){
        _callback->DeviceConfigChanged();
    }
}

bool DeviceAgent::channel_is_enable(int index)
{  
    for (const GSList *l = get_channels(); l; l = l->next)
    {
        const sr_channel *const probe = (const sr_channel *)l->data;
        if (probe->index == index)
            return probe->enabled;          
    }

    return false;
}
 
//---------------device config-----------/

int DeviceAgent::get_work_mode()
{
    if (_custom_device)
        return _custom_work_mode;
    return ds_get_actived_device_mode();
}

const struct sr_config_info *DeviceAgent::get_config_info(int key)
{
    if (_custom_device)
        return sr_config_info_get(key);
    return ds_get_actived_device_config_info(key);
}

bool DeviceAgent::get_device_status(struct sr_status &status, gboolean prg)
{   
    assert(_dev_handle);

    if (_custom_device)
        return false;

    if (ds_get_actived_device_status(&status, prg) == SR_OK)
    {
        return true;
    }
    return false;
}

struct sr_config *DeviceAgent::new_config(int key, GVariant *data)
{
    return ds_new_config(key, data);
}

void DeviceAgent::free_config(struct sr_config *src)
{
    ds_free_config(src);
}

bool DeviceAgent::is_collecting()
{
    if (_custom_device)
        return false;
    return ds_is_collecting() > 0;
}

GSList *DeviceAgent::get_channels()
{
    assert(_dev_handle);
    if (_custom_device)
        return _di ? _di->channels : nullptr;
    return ds_get_actived_device_channels();
}

 int DeviceAgent::get_hardware_operation_mode()
 {
    assert(_dev_handle);

    int mode_val = 0;
    if (get_config_int16(SR_CONF_OPERATION_MODE, mode_val)){                  
        return mode_val;
    }
    return -1;
 }

 bool DeviceAgent::supports_config(int key)
 {
    if (!have_instance())
        return false;

    if (_custom_device) {
        switch (key) {
        case SR_CONF_SAMPLERATE:
        case SR_CONF_LIMIT_SAMPLES:
        case SR_CONF_OPERATION_MODE:
        case SR_CONF_STREAM:
            return true;
        default:
            return false;
        }
    }

    return ds_actived_device_supports_config_key(key) > 0;
 }

 bool DeviceAgent::supports_capability(int capability)
 {
    if (!have_instance())
        return false;

    if (_custom_device)
        return capability == DS_DEVICE_CAP_WAVEFORM;

    return ds_actived_device_supports_capability(capability) > 0;
 }

 bool DeviceAgent::supports_waveform()
 {
    return supports_capability(DS_DEVICE_CAP_WAVEFORM);
 }

 bool DeviceAgent::supports_stream()
 {
    return supports_capability(DS_DEVICE_CAP_STREAM);
 }

 bool DeviceAgent::supports_advanced_trigger()
 {
    return supports_capability(DS_DEVICE_CAP_ADVANCED_TRIGGER);
 }

 bool DeviceAgent::is_stream_mode()
 { 
    return get_hardware_operation_mode() == LO_OP_STREAM;
 }

 bool DeviceAgent::check_firmware_version()
 {
    assert(_dev_handle);

    int st = -1;
    if (ds_get_actived_device_init_status(&st) == SR_OK){
        if (st == SR_ST_INCOMPATIBLE){
            return false;
        }
    }
    return true;
 }

 QString DeviceAgent::get_demo_operation_mode()
 {
    assert(_dev_handle);

    if (is_demo() == false){
        assert(false);
    }        
    
    QString pattern_mode;
    if(!supports_config(SR_CONF_PATTERN_MODE) ||
       get_config_string(SR_CONF_PATTERN_MODE, pattern_mode) == false ||
       pattern_mode.isEmpty())
        return QStringLiteral("random");

    return pattern_mode;
 }

GVariant* DeviceAgent::get_config_list(const sr_channel_group *group, int key)
{
    assert(_dev_handle);

    if (_custom_device) {
        (void)group;
        if (key == SR_CONF_SAMPLERATE) {
            dsv_info("DeviceAgent::get_config_list custom samplerate=%llu",
                     (unsigned long long)_custom_sample_rate);
            return custom_samplerate_list_variant(_custom_sample_rate);
        }
        return NULL;
    }

    GVariant *data = NULL;

    int ret = ds_get_actived_device_config_list(group, key, &data);
    if (ret != SR_OK){
        if (ret != SR_ERR_NA)
            dsv_detail("%s%d", "WARNING: Failed to get config list, key:", key); 
        
        if (data != NULL){
            dsv_warn("%s%d", "WARNING: Failed to get config list, but data is not null. key:", key); 
        }
        data = NULL;
    }

    return data;
}

GVariant* DeviceAgent::get_config(int key, const sr_channel *ch, const sr_channel_group *cg)
{
    if (_dev_handle == NULL_HANDLE)
        return NULL;

    if (_custom_device) {
        (void)ch;
        (void)cg;
        return custom_config_variant(key);
    }

    struct ds_device_full_info active_info;
    if (ds_get_actived_device_info(&active_info) != SR_OK ||
        active_info.handle != _dev_handle) {
        dsv_warn("DeviceAgent::get_config ignored inactive device handle=%p key=%d",
                 (void*)_dev_handle, key);
        return NULL;
    }

    GVariant *data = NULL;
    
    int ret = ds_get_actived_device_config(ch, cg, key, &data);
    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR:DeviceAgent::get_config, Failed to get value of config id:", key);
    }
    return data;
}

bool DeviceAgent::have_config(int key, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);

    if (gvar != NULL){
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config(int key, GVariant *data, const sr_channel *ch, const sr_channel_group *cg)
 {
    assert(_dev_handle);

    if (_custom_device) {
        (void)ch;
        (void)cg;
        if (!set_custom_config_variant(key, data))
            return false;
        config_changed();
        return true;
    }

    int ret = ds_set_actived_device_config(ch, cg, key, data);
    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR:DeviceAgent::set_config, Failed to set value of config id:", key);
        return false;
    }

    config_changed();
    return true;
 }

 bool DeviceAgent::get_config_int32(int key, int &value, const sr_channel *ch, const sr_channel_group *cg)
 {  
    GVariant* gvar = get_config(key, ch, cg);
    
    if (gvar != NULL) {
        value = g_variant_get_int32(gvar);
        g_variant_unref(gvar);
        return true;
    }

    return false;
 }

bool DeviceAgent::set_config_int32(int key, int value, const sr_channel *ch, const sr_channel_group *cg)
 {
    assert(_dev_handle);

    if (_custom_device)
        return set_config(key, g_variant_new_int32(value), ch, cg);

    GVariant *gvar = g_variant_new_int32(value);
    int ret = ds_set_actived_device_config(ch, cg, key, gvar);

    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR: DeviceAgent::set_config_int32, Failed to set value of config id:", key);
        return false;
    }
    return true;
 }

 bool DeviceAgent::get_config_string(int key, QString &value, const sr_channel *ch, const sr_channel_group *cg)
 {
    GVariant* gvar = get_config(key, ch, cg);
    
    if (gvar != NULL) {
        const gchar *s = g_variant_get_string(gvar, NULL);
        value = QString(s);
        g_variant_unref(gvar);
        return true;
    }

    return false;
 }

 bool DeviceAgent::set_config_string(int key, const char *value, const sr_channel *ch, const sr_channel_group *cg)
 {
    assert(value);
    assert(_dev_handle);

    if (_custom_device)
        return set_config(key, g_variant_new_string(value), ch, cg);

    GVariant *gvar = g_variant_new_string(value);
    int ret = ds_set_actived_device_config(ch, cg, key, gvar);

    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR: DeviceAgent::set_config_string, Failed to set value of config id:", key);
        return false;
    }
    return true;
 }

bool DeviceAgent::get_config_bool(int key, bool &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant* gvar = get_config(key, ch, cg);
    
    if (gvar != NULL) {
        gboolean v = g_variant_get_boolean(gvar);
        value = v > 0;
        g_variant_unref(gvar);
        return true;
    }

    return false;
}

bool DeviceAgent::set_config_bool(int key, bool value, const sr_channel *ch, const sr_channel_group *cg)
{
    assert(_dev_handle);

    if (_custom_device)
        return set_config(key, g_variant_new_boolean(value), ch, cg);

    GVariant *gvar = g_variant_new_boolean(value);
    int ret = ds_set_actived_device_config(ch, cg, key, gvar);

    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR: DeviceAgent::set_config_bool, Failed to set value of config id:", key);
        return false;
    }
    return true;
}

bool DeviceAgent::get_config_uint64(int key, uint64_t &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant* gvar = get_config(key, ch, cg);
    
    if (gvar != NULL) {
        value = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
        return true;
    }

    return false;
}

bool DeviceAgent::set_config_uint64(int key, uint64_t value, const sr_channel *ch, const sr_channel_group *cg)
{
    assert(_dev_handle);

    if (_custom_device)
        return set_config(key, g_variant_new_uint64(value), ch, cg);

    GVariant *gvar = g_variant_new_uint64(value);
    int ret = ds_set_actived_device_config(ch, cg, key, gvar);

    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR: DeviceAgent::set_config_uint64, Failed to set value of config id:", key);
        return false;
    }
    return true;
}

bool DeviceAgent::get_config_uint16(int key, int &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant* gvar = get_config(key, ch, cg);
    
    if (gvar != NULL) {
        value = g_variant_get_uint16(gvar);
        g_variant_unref(gvar);
        return true;
    }

    return false;
}

bool DeviceAgent::set_config_uint16(int key, int value, const sr_channel *ch, const sr_channel_group *cg)
{
    assert(_dev_handle);

    if (_custom_device)
        return set_config(key, g_variant_new_uint16(value), ch, cg);

    GVariant *gvar = g_variant_new_uint16(value);
    int ret = ds_set_actived_device_config(ch, cg, key, gvar);

    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR: DeviceAgent::set_config_uint16, Failed to set value of config id:", key);
        return false;
    }
    return true;
}

bool DeviceAgent::get_config_uint32(int key, uint32_t &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant* gvar = get_config(key, ch, cg);
    
    if (gvar != NULL) {
        value = g_variant_get_uint32(gvar);
        g_variant_unref(gvar);
        return true;
    }

    return false;
}

bool DeviceAgent::set_config_uint32(int key, uint32_t value, const sr_channel *ch, const sr_channel_group *cg)
{
    assert(_dev_handle);

    if (_custom_device)
        return set_config(key, g_variant_new_uint32(value), ch, cg);

    GVariant *gvar = g_variant_new_uint32(value);
    int ret = ds_set_actived_device_config(ch, cg, key, gvar);

    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR: DeviceAgent::set_config_uint32, Failed to set value of config id:", key);
        return false;
    }
    return true;
}

bool DeviceAgent::get_config_int16(int key, int &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant* gvar = get_config(key, ch, cg);
    
    if (gvar != NULL) {
        value = g_variant_get_int16(gvar);
        g_variant_unref(gvar);
        return true;
    }

    return false;
}

bool DeviceAgent::set_config_int16(int key, int value, const sr_channel *ch, const sr_channel_group *cg)
{
    assert(_dev_handle);

    if (_custom_device)
        return set_config(key, g_variant_new_int16(value), ch, cg);

    GVariant *gvar = g_variant_new_int16(value);
    int ret = ds_set_actived_device_config(ch, cg, key, gvar);

    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR: DeviceAgent::set_config_int16, Failed to set value of config id:", key);
        return false;
    }
    return true;
}

bool DeviceAgent::get_config_byte(int key, int &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant* gvar = get_config(key, ch, cg);
    
    if (gvar != NULL) {
        value = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        return true;
    }

    return false;
}

bool DeviceAgent::set_config_byte(int key, int value, const sr_channel *ch, const sr_channel_group *cg)
{
    assert(_dev_handle);

    if (_custom_device)
        return set_config(key, g_variant_new_byte((uint8_t)value), ch, cg);

    GVariant *gvar = g_variant_new_byte((uint8_t)value);
    int ret = ds_set_actived_device_config(ch, cg, key, gvar);

    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR: DeviceAgent::set_config_byte, Failed to set value of config id:", key);
        return false;
    }
    return true;
}

bool DeviceAgent::get_config_double(int key, double &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant* gvar = get_config(key, ch, cg);
    
    if (gvar != NULL) {
        value = g_variant_get_double(gvar);
        g_variant_unref(gvar);
        return true;
    }

    return false;
}

bool DeviceAgent::set_config_double(int key, double value, const sr_channel *ch, const sr_channel_group *cg)
{
    assert(_dev_handle);

    if (_custom_device)
        return set_config(key, g_variant_new_double(value), ch, cg);

    GVariant *gvar = g_variant_new_double(value);
    int ret = ds_set_actived_device_config(ch, cg, key, gvar);

    if (ret != SR_OK)
    {
        if (ret != SR_ERR_NA)
            dsv_err("%s%d", "ERROR: DeviceAgent::set_config_double, Failed to set value of config id:", key);
        return false;
    }
    return true;
}

//---------------device config end -----------/

GVariant *DeviceAgent::custom_config_variant(int key) const
{
    switch (key) {
    case SR_CONF_SAMPLERATE:
        return g_variant_ref_sink(g_variant_new_uint64(_custom_sample_rate));
    case SR_CONF_LIMIT_SAMPLES:
        return g_variant_ref_sink(g_variant_new_uint64(_custom_sample_limit));
    case SR_CONF_OPERATION_MODE:
        return g_variant_ref_sink(g_variant_new_int16(_custom_work_mode));
    case SR_CONF_STREAM:
        return g_variant_ref_sink(g_variant_new_boolean(FALSE));
    default:
        return NULL;
    }
}

bool DeviceAgent::set_custom_config_variant(int key, GVariant *data)
{
    if (!data)
        return false;

    switch (key) {
    case SR_CONF_SAMPLERATE:
        if (!g_variant_is_of_type(data, G_VARIANT_TYPE_UINT64))
            return false;
        _custom_sample_rate = g_variant_get_uint64(data);
        return true;
    case SR_CONF_LIMIT_SAMPLES:
        if (!g_variant_is_of_type(data, G_VARIANT_TYPE_UINT64))
            return false;
        _custom_sample_limit = g_variant_get_uint64(data);
        return true;
    case SR_CONF_OPERATION_MODE:
        if (!g_variant_is_of_type(data, G_VARIANT_TYPE_INT16))
            return false;
        _custom_work_mode = g_variant_get_int16(data);
        _custom_mode_entry.mode = _custom_work_mode;
        return true;
    case SR_CONF_STREAM:
        return g_variant_is_of_type(data, G_VARIANT_TYPE_BOOLEAN);
    default:
        return false;
    }
}

