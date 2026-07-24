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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "output/chronovu-la8"

#include "../log.h"

#define CHRONOVU_LA8_DATASIZE (8 * 1024 * 1024)
#define CHRONOVU_LA8_HDRSIZE  (sizeof(uint8_t) + sizeof(uint32_t))

struct context {
	uint64_t samplerate;
	uint64_t samplecount;
	uint32_t trigger_position;
	GString *data;
};

/**
 * Check if the given samplerate is supported by the LA8 hardware.
 *
 * @param samplerate The samplerate (in Hz) to check.
 *
 * @return 1 if the samplerate is supported/valid, 0 otherwise.
 */
static gboolean is_valid_samplerate(uint64_t samplerate)
{
	unsigned int i;

	for (i = 0; i < 255; i++) {
		if (samplerate == (SR_MHZ(100) / (i + 1)))
			return TRUE;
	}

	return FALSE;
}

/**
 * Convert a samplerate (in Hz) to the 'divcount' value the LA8 wants.
 *
 * LA8 hardware: sample period = (divcount + 1) * 10ns.
 * Min. value for divcount: 0x00 (10ns sample period, 100MHz samplerate).
 * Max. value for divcount: 0xfe (2550ns sample period, 392.15kHz samplerate).
 *
 * @param samplerate The samplerate in Hz.
 *
 * @return The divcount value as needed by the hardware, or 0xff upon errors.
 */
static uint8_t samplerate_to_divcount(uint64_t samplerate)
{
	if (samplerate == 0 || !is_valid_samplerate(samplerate)) {
		sr_warn("Invalid samplerate (%" PRIu64 "Hz)", samplerate);
		return 0xff;
	}

	return (SR_MHZ(100) / samplerate) - 1;
}

static int init(struct sr_output *o, GHashTable *options)
{
	struct context *ctx;

	(void)options;

	if (!o || !o->sdi)
		return SR_ERR_ARG;

	ctx = g_malloc0(sizeof(struct context));
	o->priv = ctx;
	ctx->data = g_string_sized_new(CHRONOVU_LA8_DATASIZE);
	if (!ctx->data) {
		g_free(ctx);
		o->priv = NULL;
		return SR_ERR_MALLOC;
	}

	return SR_OK;
}

static int receive(const struct sr_output *o, const struct sr_datafeed_packet *packet,
		GString **out)
{
	const struct sr_datafeed_logic *logic;
	const struct sr_datafeed_meta *meta;
	const struct sr_config *src;
	struct context *ctx;
	GVariant *gvar;
	GSList *l;
	uint8_t divcount;

	*out = NULL;
	if (!o || !o->sdi)
		return SR_ERR_ARG;
	if (!(ctx = o->priv))
		return SR_ERR_ARG;

	switch (packet->type) {
	case SR_DF_HEADER:
		break;
	case SR_DF_META:
		meta = packet->payload;
		for (l = meta->config; l; l = l->next) {
			src = l->data;
			if (src->key == SR_CONF_SAMPLERATE)
				ctx->samplerate = g_variant_get_uint64(src->data);
		}
		break;
	case SR_DF_TRIGGER:
		ctx->trigger_position = ctx->samplecount;
		break;
	case SR_DF_LOGIC:
		logic = packet->payload;
		if (!logic->unitsize || logic->length >
			CHRONOVU_LA8_DATASIZE - ctx->data->len)
			return SR_ERR_ARG;
		g_string_append_len(ctx->data, logic->data, logic->length);
		ctx->samplecount += logic->length / logic->unitsize;
		break;
	case SR_DF_END:
		if (!ctx->samplerate && sr_config_get(o->sdi->driver, o->sdi,
				NULL, NULL, SR_CONF_SAMPLERATE, &gvar) == SR_OK) {
			ctx->samplerate = g_variant_get_uint64(gvar);
			g_variant_unref(gvar);
		}
		divcount = samplerate_to_divcount(ctx->samplerate);
		*out = g_string_sized_new(CHRONOVU_LA8_DATASIZE + CHRONOVU_LA8_HDRSIZE);
		g_string_set_size(*out, CHRONOVU_LA8_DATASIZE + CHRONOVU_LA8_HDRSIZE);
		memset((*out)->str, 0, (*out)->len);
		memcpy((*out)->str, ctx->data->str, ctx->data->len);
		(*out)->str[CHRONOVU_LA8_DATASIZE] = divcount;
		(*out)->str[CHRONOVU_LA8_DATASIZE + 1] = ctx->trigger_position & 0xff;
		(*out)->str[CHRONOVU_LA8_DATASIZE + 2] = (ctx->trigger_position >> 8) & 0xff;
		(*out)->str[CHRONOVU_LA8_DATASIZE + 3] = (ctx->trigger_position >> 16) & 0xff;
		(*out)->str[CHRONOVU_LA8_DATASIZE + 4] = (ctx->trigger_position >> 24) & 0xff;
		break;
	}

	return SR_OK;
}

static int cleanup(struct sr_output *o)
{
	struct context *ctx;

	if (!o || !o->sdi)
		return SR_ERR_ARG;

	if (o->priv) {
		ctx = o->priv;
		g_string_free(ctx->data, TRUE);
		g_free(o->priv);
		o->priv = NULL;
	}

	return SR_OK;
}

SR_PRIV struct sr_output_module output_chronovu_la8 = {
	.id = "chronovu-la8",
	.name = "ChronoVu LA8",
	.desc = "ChronoVu LA8 native file format data",
	.exts = (const char*[]){"kdt", NULL},
	.flags = 0,
	.options = NULL,
	.init = init,
	.receive = receive,
	.cleanup = cleanup,
};
