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

#include "upstream_demo.h"
#include "device_source.h"
#include "../../log.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#undef LOG_PREFIX
#define LOG_PREFIX "upstream-demo: "

#define UPSTREAM_DEMO_CHANNEL_COUNT 8

struct upstream_demo_context {
	uint64_t samplerate;
	uint64_t limit_samples;
};

static const uint64_t samplerates[] = {
	SR_KHZ(100),
	SR_MHZ(1),
	SR_MHZ(10),
};

static const int32_t devopts[] = {
	SR_CONF_SAMPLERATE,
	SR_CONF_LIMIT_SAMPLES,
	SR_CONF_VLD_CH_NUM,
	SR_CONF_PROBE_EN,
};

static int hw_init(struct sr_context *ctx)
{
	(void)ctx;
	return SR_OK;
}

static int hw_cleanup(void)
{
	return SR_OK;
}

static GSList *hw_scan(GSList *options)
{
	struct sr_dev_inst *sdi;
	struct upstream_demo_context *devc;

	(void)options;

	sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE,
		"DSView", "Upstream Compat Demo", "0.1");
	if (!sdi)
		return NULL;

	devc = g_malloc0(sizeof(*devc));
	devc->samplerate = SR_MHZ(1);
	devc->limit_samples = SR_KHZ(1);
	sdi->priv = devc;
	sdi->driver = &upstream_demo_driver_info;
	sdi->dev_type = DEV_TYPE_USB;
	ds_device_source_set(sdi, DS_DEVICE_SOURCE_UPSTREAM_COMPAT);

	for (int i = 0; i < UPSTREAM_DEMO_CHANNEL_COUNT; i++) {
		char name[8];
		struct sr_channel *probe;

		snprintf(name, sizeof(name), "D%d", i);
		probe = sr_channel_new(sdi, i, SR_CHANNEL_LOGIC, TRUE, name);
		if (!probe) {
			sr_dev_inst_free(sdi);
			return NULL;
		}
	}

	return g_slist_append(NULL, sdi);
}

static const GSList *hw_dev_mode_list(const struct sr_dev_inst *sdi)
{
	(void)sdi;
	return g_slist_append(NULL, (gpointer)&sr_mode_list[0]);
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
	const struct sr_channel *ch, const struct sr_channel_group *cg)
{
	struct upstream_demo_context *devc;

	(void)cg;

	if (!sdi || !sdi->priv || !data)
		return SR_ERR_ARG;

	devc = sdi->priv;
	switch (id) {
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->samplerate);
		return SR_OK;
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		return SR_OK;
	case SR_CONF_VLD_CH_NUM:
		*data = g_variant_new_int16(UPSTREAM_DEMO_CHANNEL_COUNT);
		return SR_OK;
	case SR_CONF_PROBE_EN:
		if (!ch)
			return SR_ERR_ARG;
		*data = g_variant_new_boolean(ch->enabled);
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

static int config_set(int id, GVariant *data, struct sr_dev_inst *sdi,
	struct sr_channel *ch, struct sr_channel_group *cg)
{
	struct upstream_demo_context *devc;

	(void)cg;

	if (!sdi || !sdi->priv || !data)
		return SR_ERR_ARG;

	devc = sdi->priv;
	switch (id) {
	case SR_CONF_SAMPLERATE:
		devc->samplerate = g_variant_get_uint64(data);
		return SR_OK;
	case SR_CONF_LIMIT_SAMPLES:
		devc->limit_samples = g_variant_get_uint64(data);
		return SR_OK;
	case SR_CONF_PROBE_EN:
		if (!ch)
			return SR_ERR_ARG;
		ch->enabled = g_variant_get_boolean(data);
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

static GVariant *samplerates_variant(void)
{
	GVariantBuilder builder;
	GVariant *values;

	values = g_variant_new_from_data(G_VARIANT_TYPE("at"),
		samplerates, ARRAY_SIZE(samplerates) * sizeof(uint64_t),
		TRUE, NULL, NULL);

	g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&builder, "{sv}", "samplerates", values);
	return g_variant_builder_end(&builder);
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg)
{
	(void)sdi;
	(void)cg;

	if (!data)
		return SR_ERR_ARG;

	switch (key) {
	case SR_CONF_DEVICE_OPTIONS:
		*data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
			devopts, ARRAY_SIZE(devopts) * sizeof(int32_t),
			TRUE, NULL, NULL);
		return SR_OK;
	case SR_CONF_SAMPLERATE:
		*data = samplerates_variant();
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

static int hw_dev_open(struct sr_dev_inst *sdi)
{
	if (!sdi)
		return SR_ERR_ARG;

	sdi->status = SR_ST_ACTIVE;
	return SR_OK;
}

static int hw_dev_close(struct sr_dev_inst *sdi)
{
	if (!sdi)
		return SR_ERR_ARG;

	sdi->status = SR_ST_INACTIVE;
	return SR_OK;
}

static int hw_dev_destroy(struct sr_dev_inst *sdi)
{
	if (!sdi)
		return SR_OK;

	sr_dev_inst_free(sdi);
	return SR_OK;
}

static int hw_dev_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
	struct upstream_demo_context *devc;
	uint8_t *sample_data;
	uint64_t remaining_samples;
	struct sr_datafeed_logic logic;
	struct sr_datafeed_packet packet;
	int ret;

	const uint64_t chunk_samples = 8192;
	const uint16_t unitsize = 1;

	(void)cb_data;

	if (!sdi || !sdi->priv)
		return SR_ERR_ARG;

	devc = sdi->priv;
	sample_data = g_malloc(chunk_samples * unitsize);
	if (!sample_data)
		return SR_ERR_MALLOC;
	memset(sample_data, 0xaa, chunk_samples * unitsize);

	memset(&logic, 0, sizeof(logic));
	memset(&packet, 0, sizeof(packet));

	logic.format = LA_CROSS_DATA;
	logic.unitsize = unitsize;
	logic.data = sample_data;

	packet.type = SR_DF_LOGIC;
	packet.status = SR_PKT_OK;
	packet.payload = &logic;

	sr_info("Acquisition start: samplerate=%" PRIu64
		" limit_samples=%" PRIu64 " unitsize=%u.",
		devc->samplerate, devc->limit_samples, logic.unitsize);

	remaining_samples = devc->limit_samples;
	while (remaining_samples > 0) {
		uint64_t samples = MIN(remaining_samples, chunk_samples);
		logic.length = samples * unitsize;
		ret = ds_data_forward(sdi, &packet);
		if (ret != SR_OK) {
			g_free(sample_data);
			return ret;
		}
		remaining_samples -= samples;
	}

	g_free(sample_data);

	packet.type = SR_DF_END;
	packet.status = SR_PKT_OK;
	packet.payload = NULL;
	ret = ds_data_forward(sdi, &packet);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

static int hw_dev_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

SR_PRIV struct sr_dev_driver upstream_demo_driver_info = {
	.name = "upstream-demo",
	.longname = "Upstream Compat Demo",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_DEMO,
	.init = hw_init,
	.cleanup = hw_cleanup,
	.scan = hw_scan,
	.dev_mode_list = hw_dev_mode_list,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = hw_dev_open,
	.dev_close = hw_dev_close,
	.dev_destroy = hw_dev_destroy,
	.dev_status_get = NULL,
	.dev_acquisition_start = hw_dev_acquisition_start,
	.dev_acquisition_stop = hw_dev_acquisition_stop,
	.priv = NULL,
};
