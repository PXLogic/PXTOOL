/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
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


#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"
#include <string.h>

struct feed_queue_logic {
	const struct sr_dev_inst *sdi;
	size_t unit_size;
	size_t channel_count;
	size_t alloc_count;
	size_t fill_count;
	uint8_t *data_bytes;
	uint8_t *cross_data;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;
};

SR_API struct feed_queue_logic *feed_queue_logic_alloc(
	const struct sr_dev_inst *sdi,
	size_t sample_count, size_t unit_size)
{
	struct feed_queue_logic *q;

	q = g_malloc0(sizeof(*q));
	q->sdi = sdi;
	q->unit_size = unit_size;
	q->alloc_count = sample_count;
	q->data_bytes = g_try_malloc(q->alloc_count * q->unit_size);
	if (!q->data_bytes) {
		g_free(q);
		return NULL;
	}

	memset(&q->packet, 0, sizeof(q->packet));
	memset(&q->logic, 0, sizeof(q->logic));
	q->packet.type = SR_DF_LOGIC;
	q->packet.payload = &q->logic;
	q->logic.unitsize = q->unit_size;
	q->logic.data = q->data_bytes;

	return q;
}

SR_API struct feed_queue_logic *feed_queue_logic_alloc_cross_data(
	const struct sr_dev_inst *sdi,
	size_t sample_count, size_t unit_size, size_t channel_count)
{
	struct feed_queue_logic *q;

	q = feed_queue_logic_alloc(sdi, sample_count, unit_size);
	if (!q)
		return NULL;

	q->channel_count = channel_count;
	if (q->channel_count && q->alloc_count >= 64)
		q->alloc_count -= q->alloc_count % 64;
	if (q->channel_count && q->alloc_count < 64)
		q->alloc_count = 64;

	return q;
}

static uint8_t *pack_logic_cross_data(const struct feed_queue_logic *q,
	size_t *cross_len)
{
	uint8_t *cross_data;
	size_t block_count, sample_idx, ch_idx;

	*cross_len = 0;
	if (!q->channel_count || !q->unit_size || !q->fill_count)
		return NULL;

	block_count = (q->fill_count + 63) / 64;
	*cross_len = block_count * q->channel_count * 8;
	cross_data = g_malloc0(*cross_len);
	if (!cross_data)
		return NULL;

	for (sample_idx = 0; sample_idx < q->fill_count; sample_idx++) {
		const uint8_t *sample;
		size_t block, bit, bit_byte, block_base;
		uint8_t bit_mask;

		sample = &q->data_bytes[sample_idx * q->unit_size];
		block = sample_idx / 64;
		bit = sample_idx % 64;
		bit_byte = bit / 8;
		bit_mask = 1 << (bit % 8);
		block_base = block * q->channel_count * 8;

		for (ch_idx = 0; ch_idx < q->channel_count; ch_idx++) {
			if (sample[ch_idx / 8] & (1 << (ch_idx % 8)))
				cross_data[block_base + ch_idx * 8 + bit_byte] |= bit_mask;
		}
	}

	return cross_data;
}

SR_API int feed_queue_logic_submit_one(struct feed_queue_logic *q,
	const uint8_t *data, size_t repeat_count)
{
	uint8_t *wrptr;
	int ret;

	wrptr = &q->data_bytes[q->fill_count * q->unit_size];
	while (repeat_count--) {
		memcpy(wrptr, data, q->unit_size);
		wrptr += q->unit_size;
		q->fill_count++;
		if (q->fill_count == q->alloc_count) {
			ret = feed_queue_logic_flush(q);
			if (ret != SR_OK)
				return ret;
			wrptr = &q->data_bytes[0];
		}
	}

	return SR_OK;
}

SR_API int feed_queue_logic_submit_many(struct feed_queue_logic *q,
	const uint8_t *data, size_t samples_count)
{
	uint8_t *wrptr;
	size_t space, copy_count;
	int ret;

	wrptr = &q->data_bytes[q->fill_count * q->unit_size];
	while (samples_count) {
		space = q->alloc_count - q->fill_count;
		copy_count = samples_count;
		if (copy_count > space)
			copy_count = space;
		memcpy(wrptr, data, copy_count * q->unit_size);
		data += copy_count * q->unit_size;
		samples_count -= copy_count;
		wrptr += copy_count * q->unit_size;
		q->fill_count += copy_count;
		if (q->fill_count == q->alloc_count) {
			ret = feed_queue_logic_flush(q);
			if (ret != SR_OK)
				return ret;
			wrptr = &q->data_bytes[0];
		}
	}

	return SR_OK;
}

SR_API int feed_queue_logic_flush(struct feed_queue_logic *q)
{
	int ret;
	size_t cross_len;

	if (!q->fill_count)
		return SR_OK;

	if (q->channel_count) {
		q->cross_data = pack_logic_cross_data(q, &cross_len);
		if (!q->cross_data)
			return SR_ERR_MALLOC;
		q->logic.format = LA_CROSS_DATA;
		q->logic.unitsize = 1;
		q->logic.length = cross_len;
		q->logic.data = q->cross_data;
	} else {
		q->logic.length = q->fill_count * q->unit_size;
		q->logic.data = q->data_bytes;
	}

	ret = sr_session_send(q->sdi, &q->packet);
	if (q->cross_data) {
		g_free(q->cross_data);
		q->cross_data = NULL;
		q->logic.data = q->data_bytes;
	}
	if (ret != SR_OK)
		return ret;
	q->fill_count = 0;

	return SR_OK;
}

SR_API int feed_queue_logic_send_trigger(struct feed_queue_logic *q)
{
	int ret;

	ret = feed_queue_logic_flush(q);
	if (ret != SR_OK)
		return ret;

	ret = std_session_send_df_trigger(q->sdi);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

SR_API void feed_queue_logic_free(struct feed_queue_logic *q)
{

	if (!q)
		return;

	g_free(q->data_bytes);
	g_free(q->cross_data);
	g_free(q);
}

struct feed_queue_analog {
	const struct sr_dev_inst *sdi;
	size_t alloc_count;
	size_t fill_count;
	float *data_values;
	int digits;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	GSList *channels;
};

SR_API struct feed_queue_analog *feed_queue_analog_alloc(
	const struct sr_dev_inst *sdi,
	size_t sample_count, int digits, struct sr_channel *ch)
{
	struct feed_queue_analog *q;

	q = g_malloc0(sizeof(*q));
	q->sdi = sdi;
	q->alloc_count = sample_count;
	q->data_values = g_try_malloc(q->alloc_count * sizeof(float));
	if (!q->data_values) {
		g_free(q);
		return NULL;
	}
	q->digits = digits;
	q->channels = g_slist_append(NULL, ch);

	memset(&q->packet, 0, sizeof(q->packet));
	sr_analog_init(&q->analog, &q->encoding, &q->meaning, &q->spec, digits);
	q->packet.type = SR_DF_ANALOG;
	q->packet.payload = &q->analog;
	q->encoding.is_signed = TRUE;
	q->meaning.channels = q->channels;
	q->analog.data = q->data_values;

	return q;
}

SR_API int feed_queue_analog_mq_unit(struct feed_queue_analog *q,
	enum sr_mq mq, enum sr_mqflag mq_flag, enum sr_unit unit)
{
	int ret;

	if (!q)
		return SR_ERR_ARG;

	ret = feed_queue_analog_flush(q);
	if (ret != SR_OK)
		return ret;

	q->meaning.mq = mq;
	q->meaning.mqflags = mq_flag;
	q->meaning.unit = unit;

	return SR_OK;
}

SR_API int feed_queue_analog_scale_offset(struct feed_queue_analog *q,
	const struct sr_rational *scale, const struct sr_rational *offset)
{
	int ret;

	if (!q)
		return SR_ERR_ARG;

	ret = feed_queue_analog_flush(q);
	if (ret != SR_OK)
		return ret;

	if (scale)
		q->encoding.scale = *scale;
	if (offset)
		q->encoding.offset = *offset;

	return SR_OK;
}

SR_API int feed_queue_analog_submit_one(struct feed_queue_analog *q,
	float data, size_t repeat_count)
{
	int ret;

	while (repeat_count--) {
		q->data_values[q->fill_count++] = data;
		if (q->fill_count == q->alloc_count) {
			ret = feed_queue_analog_flush(q);
			if (ret != SR_OK)
				return ret;
		}
	}

	return SR_OK;
}

SR_API int feed_queue_analog_flush(struct feed_queue_analog *q)
{
	int ret;

	if (!q->fill_count)
		return SR_OK;

	q->analog.num_samples = q->fill_count;
	ret = sr_session_send(q->sdi, &q->packet);
	if (ret != SR_OK)
		return ret;
	q->fill_count = 0;

	return SR_OK;
}

SR_API void feed_queue_analog_free(struct feed_queue_analog *q)
{

	if (!q)
		return;

	g_free(q->data_values);
	g_slist_free(q->channels);
	g_free(q);
}
