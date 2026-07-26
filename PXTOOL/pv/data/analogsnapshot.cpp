/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <limits>
 
#include "analogsnapshot.h"
#include "../dsvdef.h"

using namespace std;

namespace pv {
namespace data {

const int AnalogSnapshot::EnvelopeScalePower = 4;
const int AnalogSnapshot::EnvelopeScaleFactor = 1 << EnvelopeScalePower;
const float AnalogSnapshot::LogEnvelopeScaleFactor =
	logf(EnvelopeScaleFactor);
const uint64_t AnalogSnapshot::EnvelopeDataUnit = 64*1024;	// bytes

AnalogSnapshot::AnalogSnapshot() :
    Snapshot(sizeof(uint16_t), 1, 1)
{
	memset(_envelope_levels, 0, sizeof(_envelope_levels));
    _unit_pitch = 0;
    _data  = NULL;
    _is_float = false;
}

AnalogSnapshot::~AnalogSnapshot()
{
    free_envelop();
}

void AnalogSnapshot::free_envelop()
{
    // _channel_num describes the *new* packet during first_payload(). It can
    // therefore be smaller than the channel count used to allocate the old
    // envelopes. Iterate over the storage itself so every allocation is
    // released exactly once, regardless of channel-count changes.
    for (unsigned int i = 0; i < DS_MAX_ANALOG_PROBES_NUM; i++) {
        for(auto &e : _envelope_levels[i]) {
            if (e.samples) {
                free(e.samples);
                e.samples = NULL;
            }
        }
    }
    memset(_envelope_levels, 0, sizeof(_envelope_levels));
}

void AnalogSnapshot::init()
{
    std::lock_guard<std::mutex> lock(_mutex);
    init_all();
}

void AnalogSnapshot::init_all()
{
    _sample_count = 0;
    _ring_sample_count = 0;
    _memory_failed = false;
    _last_ended = true;
    _channel_mins.clear();
    _channel_maxs.clear();

    for (unsigned int i = 0; i < _channel_num; i++) {
        for (unsigned int level = 0; level < ScaleStepCount; level++) {
            _envelope_levels[i][level].length = 0;
            _envelope_levels[i][level].ring_length = 0;
            // fix hang issue, count should not be clear
            //_envelope_levels[i][level].count = 0;
            _envelope_levels[i][level].data_length = 0;
        }
    }
}

void AnalogSnapshot::free_data()
{
    Snapshot::free_data();

    if (_data != NULL){
        free(_data);
        _data = NULL;
    }
}

void AnalogSnapshot::clear()
{
    std::lock_guard<std::mutex> lock(_mutex);
    free_data();
    free_envelop();
    init_all();
}

bool AnalogSnapshot::first_payload(const sr_datafeed_analog &analog, uint64_t total_sample_count, GSList *channels)
{
    const bool standard_packet = analog.encoding || analog.meaning;
    if (standard_packet) {
        if (!analog.encoding || !analog.meaning || !analog.meaning->channels ||
            !analog.data || !analog.encoding->unitsize || total_sample_count == 0)
            return false;
        _unit_bytes = analog.encoding->unitsize;
        _is_float = analog.encoding->is_float;
        channels = analog.meaning->channels;
    } else {
        if (!analog.data || !analog.unit_bits || !channels || total_sample_count == 0)
            return false;
        _unit_bytes = (analog.unit_bits + 7) / 8;
        _is_float = false;
    }

    if (_unit_bytes != 1 && _unit_bytes != 2 && _unit_bytes != 4 && _unit_bytes != 8)
        return false;

    _total_sample_count = total_sample_count;

    _channel_num = 0; // The enabled and disabled channels count.

    for (const GSList *l = channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;

        // TODO: data of disabled channels should not be captured.
        if (probe->type == SR_CHANNEL_ANALOG) {
            _channel_num ++;
        }
    }

    bool isOk = true;
    uint64_t size = _total_sample_count * _channel_num * _unit_bytes + sizeof(uint64_t);

    if (size != _capacity) {
        free_data();
        _data = malloc(size);

        if (_data) {
            free_envelop();

            for (unsigned int i = 0; i < _channel_num; i++) {
                uint64_t envelop_count = _total_sample_count / EnvelopeScaleFactor;
                for (unsigned int level = 0; level < ScaleStepCount; level++) {
                    _envelope_levels[i][level].count = envelop_count;

                    if (envelop_count == 0)
                        break;

                    _envelope_levels[i][level].samples = (EnvelopeSample*)malloc(envelop_count * sizeof(EnvelopeSample));
                    
                    if (!_envelope_levels[i][level].samples) {
                        isOk = false;
                        break;
                    }

                    envelop_count = envelop_count / EnvelopeScaleFactor;
                }
                if (!isOk)
                    break;
            }
        }
        else {
            isOk = false;
        }
    }

    if (isOk) {
        _ch_index.clear();
        _enabled_channel_indexs.clear();

        for (const GSList *l = channels; l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;

            // TODO: get the enabled channel index.
            if (probe->type == SR_CHANNEL_ANALOG) {
                _ch_index.push_back(probe->index);

                if (probe->enabled){
                    _enabled_channel_indexs.push_back(probe->index);
                }
            }
        }

        _capacity = size;
        _memory_failed = false;
        _channel_mins.assign(_channel_num, std::numeric_limits<double>::infinity());
        _channel_maxs.assign(_channel_num, -std::numeric_limits<double>::infinity());
        if (!append_payload(analog))
            return false;
        _last_ended = false;
        return true;
    }
    else {
        free_data();
        free_envelop();
        _memory_failed = true;
        return false;
    }
}

bool AnalogSnapshot::append_payload(const sr_datafeed_analog &analog)
{
    if (!analog.data || !analog.num_samples)
        return false;

    if (analog.encoding) {
        if (!analog.meaning || !analog.meaning->channels ||
            analog.encoding->unitsize != _unit_bytes ||
            analog.encoding->is_float != _is_float)
            return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    append_data(analog.data, analog.num_samples,
                analog.encoding ? 1 : analog.unit_pitch);

    recompute_extrema();

	// Generate the first mip-map from the data
    if (analog.num_samples != 0) // guarantee new samples to compute
        append_payload_to_envelope_levels();
    return true;
}

bool AnalogSnapshot::is_float() const
{
    return _is_float;
}

uint32_t AnalogSnapshot::channel_count() const
{
    return _channel_num;
}

uint64_t AnalogSnapshot::sample_count() const
{
    return _sample_count;
}

uint64_t AnalogSnapshot::physical_sample_index(uint64_t sample_index) const
{
    if (_sample_count < _total_sample_count)
        return sample_index;
    return (_ring_sample_count + sample_index) % _total_sample_count;
}

double AnalogSnapshot::sample_as_double(uint32_t channel_order,
                                        uint64_t sample_index) const
{
    if (!_data || channel_order >= _channel_num || sample_index >= _sample_count)
        return 0.0;

    const uint64_t physical = physical_sample_index(sample_index);
    const uint8_t *source = static_cast<const uint8_t *>(_data) +
        (physical * _channel_num + channel_order) * _unit_bytes;
    if (_is_float) {
        if (_unit_bytes == sizeof(float)) {
            float value;
            memcpy(&value, source, sizeof(value));
            return value;
        }
        if (_unit_bytes == sizeof(double)) {
            double value;
            memcpy(&value, source, sizeof(value));
            return value;
        }
        return 0.0;
    }

    uint64_t value = 0;
    memcpy(&value, source, _unit_bytes);
    return static_cast<double>(value);
}

void AnalogSnapshot::recompute_extrema()
{
    _channel_mins.assign(_channel_num, std::numeric_limits<double>::infinity());
    _channel_maxs.assign(_channel_num, -std::numeric_limits<double>::infinity());
    for (uint64_t sample = 0; sample < _sample_count; sample++) {
        for (uint32_t channel = 0; channel < _channel_num; channel++) {
            const double value = sample_as_double(channel, sample);
            _channel_mins[channel] = std::min(_channel_mins[channel], value);
            _channel_maxs[channel] = std::max(_channel_maxs[channel], value);
        }
    }
}

double AnalogSnapshot::channel_min(uint32_t channel_order) const
{
    return channel_order < _channel_mins.size() ? _channel_mins[channel_order] : 0.0;
}

double AnalogSnapshot::channel_max(uint32_t channel_order) const
{
    return channel_order < _channel_maxs.size() ? _channel_maxs[channel_order] : 0.0;
}

void AnalogSnapshot::append_data(void *data, uint64_t samples, uint16_t pitch)
{
    int bytes_per_sample = _unit_bytes * _channel_num;
    if (pitch <= 1) {
        if (_sample_count + samples < _total_sample_count)
            _sample_count += samples;
        else
            _sample_count = _total_sample_count;

        if (_ring_sample_count + samples >= _total_sample_count) {
            memcpy((uint8_t*)_data + _ring_sample_count * bytes_per_sample,
                data, (_total_sample_count - _ring_sample_count) * bytes_per_sample);
            data = (uint8_t*)data + (_total_sample_count - _ring_sample_count) * bytes_per_sample;
            _ring_sample_count = (samples + _ring_sample_count - _total_sample_count) % _total_sample_count;
            memcpy((uint8_t*)_data,
                data, _ring_sample_count * bytes_per_sample);
        } else {
            memcpy((uint8_t*)_data + _ring_sample_count * bytes_per_sample,
                data, samples * bytes_per_sample);
            _ring_sample_count += samples;
        }
    }
    else {
        while(samples--) {
            if (_unit_pitch == 0) {
                if (_sample_count < _total_sample_count)
                    _sample_count++;
                memcpy((uint8_t*)_data + _ring_sample_count * bytes_per_sample,
                    data, bytes_per_sample);
                data = (uint8_t*)data + bytes_per_sample*pitch;
                _ring_sample_count = (_ring_sample_count + 1) % _total_sample_count;
                _unit_pitch = pitch;
            }
            _unit_pitch--;
        }
    }
}

const uint8_t* AnalogSnapshot::get_samples(int64_t start_sample)
{
	assert(start_sample >= 0);
    assert(start_sample < (int64_t)get_sample_count());

    return (uint8_t*)_data + start_sample * _unit_bytes * _channel_num;
}

void AnalogSnapshot::get_envelope_section(EnvelopeSection &s,
    uint64_t start, int64_t count, float min_length, int probe_index)
{
    assert(count >= 0);
	assert(min_length > 0);

    const unsigned int min_level = max((int)floorf(logf(min_length) /
            LogEnvelopeScaleFactor) - 1, 0);
    const unsigned int scale_power = (min_level + 1) * EnvelopeScalePower;
	start >>= scale_power;

    s.start = start;
    s.scale = (1 << scale_power);
    s.length = (count >> scale_power);
    s.samples_num = _envelope_levels[probe_index][min_level].length;
    s.samples = _envelope_levels[probe_index][min_level].samples;
}

void AnalogSnapshot::reallocate_envelope(Envelope &e)
{
	const uint64_t new_data_length = ((e.length + EnvelopeDataUnit - 1) /
		EnvelopeDataUnit) * EnvelopeDataUnit;
    if (new_data_length > e.data_length)
	{
		e.data_length = new_data_length;
	}
}

void AnalogSnapshot::append_payload_to_envelope_levels()
{
    int i;
    for (i = 0; i < (int)_channel_num; i++) {
        Envelope &e0 = _envelope_levels[i][0];
        uint64_t prev_length;
        EnvelopeSample *dest_ptr;

        // Expand the data buffer to fit the new samples
        e0.length = _sample_count / EnvelopeScaleFactor;
        prev_length = e0.ring_length;
        e0.ring_length = _ring_sample_count / EnvelopeScaleFactor;

        if (e0.length == 0)
            continue;

        //reallocate_envelope(e0);

        dest_ptr = e0.samples + prev_length;

        // Iterate through the samples to populate the first level mipmap
        const uint64_t src_size = _total_sample_count * _unit_bytes * _channel_num;
        uint64_t e0_sample_num = (e0.ring_length > prev_length) ? e0.ring_length - prev_length :
                                                                  e0.ring_length + (_total_sample_count / EnvelopeScaleFactor) - prev_length;
        uint8_t *src_ptr = (uint8_t*)_data +
                    (prev_length * EnvelopeScaleFactor * _channel_num + i) * _unit_bytes;
        for (uint64_t j = 0; j < e0_sample_num; j++) {
            const uint8_t *end_src_ptr =
                src_ptr + EnvelopeScaleFactor * _unit_bytes * _channel_num;
            if (end_src_ptr >= (uint8_t*)_data + src_size)
                end_src_ptr -= src_size;
            EnvelopeSample sub_sample;
            sub_sample.min = *src_ptr;
            sub_sample.max = *src_ptr;
            src_ptr += _channel_num * _unit_bytes;
            while(src_ptr != end_src_ptr) {
                sub_sample.min = min(sub_sample.min, *src_ptr);
                sub_sample.max = max(sub_sample.max, *src_ptr);
                src_ptr += _channel_num * _unit_bytes;
                if (src_ptr >= (uint8_t*)_data + src_size)
                    src_ptr -= src_size;
            }

            *dest_ptr++ = sub_sample;
            if (dest_ptr >= e0.samples + e0.count)
                dest_ptr = e0.samples;
        }

        // Compute higher level mipmaps
        for (unsigned int level = 1; level < ScaleStepCount; level++)
        {
            Envelope &e = _envelope_levels[i][level];
            const Envelope &el = _envelope_levels[i][level-1];

            // Expand the data buffer to fit the new samples
            e.length = el.length / EnvelopeScaleFactor;
            prev_length = e.ring_length;
            e.ring_length = el.ring_length / EnvelopeScaleFactor;

            // Break off if there are no more samples to computed
            if (e.ring_length == prev_length)
                break;

            //reallocate_envelope(e);

            // Subsample the level lower level
            const EnvelopeSample *src_ptr =
                el.samples + prev_length * EnvelopeScaleFactor;
            const EnvelopeSample *const end_dest_ptr = (e.ring_length == e.count) ? e.samples : e.samples + e.ring_length;
            dest_ptr = (prev_length == e.count) ? e.samples : e.samples + prev_length;
            while(dest_ptr != end_dest_ptr) {
                const EnvelopeSample * end_src_ptr =
                    src_ptr + EnvelopeScaleFactor;
                if (end_src_ptr >= el.samples + el.count)
                    end_src_ptr -= el.count;

                EnvelopeSample sub_sample = *src_ptr++;
                while (src_ptr != end_src_ptr)
                {
                    sub_sample.min = min(sub_sample.min, src_ptr->min);
                    sub_sample.max = max(sub_sample.max, src_ptr->max);
                    src_ptr++;
                    if (src_ptr >= el.samples + el.count)
                        src_ptr = el.samples;
                }

                *dest_ptr++ = sub_sample;
                if (dest_ptr >= e.samples + e.count)
                    dest_ptr = e.samples;
            }
        }
    }
}

int AnalogSnapshot::get_ch_order(int sig_index)
{
    uint16_t order = 0;
    for (auto& iter:_ch_index) {
        if (iter == sig_index)
            break;
        order++;
    }

    if (order >= _ch_index.size())
        return -1;
    else
        return order;
}

int AnalogSnapshot::get_scale_factor()
{
    return EnvelopeScaleFactor;
}

bool AnalogSnapshot::has_data(int index)
{
    for (int iter : _ch_index) {
        if (iter == index)
            return true;
    }
    return false;
}

int AnalogSnapshot::get_block_num()
{
    const uint64_t size = _sample_count * get_unit_bytes() * get_channel_num();
    return (size >> LeafBlockPower) +
           ((size & LeafMask) != 0);
}

uint64_t AnalogSnapshot::get_block_size(int block_index)
{
    assert(block_index < get_block_num());

    if (block_index < get_block_num() - 1) {
        return LeafBlockSamples;
    } else {
        const uint64_t size = _sample_count * get_unit_bytes() * get_channel_num();
        if (size % LeafBlockSamples == 0)
            return LeafBlockSamples;
        else
            return size % LeafBlockSamples;
    }
}

void* AnalogSnapshot::get_data()
{
    return _data;
}

bool AnalogSnapshot::has_enabled_channel(int index)
{

    for (int iter : _enabled_channel_indexs) {
        if (iter == index)
            return true;
    }
    return false; 
}

} // namespace data
} // namespace pv
