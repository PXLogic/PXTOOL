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

#include "fx2lafw.h"

#include <string.h>

#undef LOG_PREFIX
#define LOG_PREFIX "upstream-fx2lafw: "

struct fx2lafw_context {
	const struct fx2lafw_profile *profile;
	uint64_t samplerate;
	uint64_t limit_samples;
};

static const struct fx2lafw_profile supported_fx2[] = {
	{0x08a9, 0x0014, "CWAV", "USBee AX", "fx2lafw-cwav-usbeeax.fw", 0, NULL, NULL},
	{0x08a9, 0x0015, "CWAV", "USBee DX", "fx2lafw-cwav-usbeedx.fw", FX2LAFW_DEV_CAPS_16BIT, NULL, NULL},
	{0x08a9, 0x0009, "CWAV", "USBee SX", "fx2lafw-cwav-usbeesx.fw", 0, NULL, NULL},
	{0x08a9, 0x0005, "CWAV", "USBee ZX", "fx2lafw-cwav-usbeezx.fw", 0, NULL, NULL},
	{0x0925, 0x3881, "Saleae", "Logic", "fx2lafw-saleae-logic.fw", 0, NULL, NULL},
	{0x04b4, 0x8613, "Cypress", "FX2", "fx2lafw-cypress-fx2.fw", FX2LAFW_DEV_CAPS_16BIT, NULL, NULL},
	{0x16d0, 0x0498, "Braintechnology", "USB-LPS", "fx2lafw-braintechnology-usb-lps.fw", FX2LAFW_DEV_CAPS_16BIT, NULL, NULL},
	{0x1d50, 0x608c, "sigrok", "FX2 LA (8ch)", "fx2lafw-sigrok-fx2-8ch.fw", 0, NULL, NULL},
	{0x1d50, 0x608d, "sigrok", "FX2 LA (16ch)", "fx2lafw-sigrok-fx2-16ch.fw", FX2LAFW_DEV_CAPS_16BIT, NULL, NULL},
	{0x1d50, 0x608f, "sigrok", "usb-c-grok", "fx2lafw-usb-c-grok.fw", 0, NULL, NULL},
	{0, 0, NULL, NULL, NULL, 0, NULL, NULL},
};

static const uint64_t samplerates[] = {
	SR_KHZ(20),
	SR_KHZ(25),
	SR_KHZ(50),
	SR_KHZ(100),
	SR_KHZ(200),
	SR_KHZ(250),
	SR_KHZ(500),
	SR_MHZ(1),
	SR_MHZ(2),
	SR_MHZ(3),
	SR_MHZ(4),
	SR_MHZ(6),
	SR_MHZ(8),
	SR_MHZ(12),
	SR_MHZ(16),
	SR_MHZ(24),
	SR_MHZ(48),
};

static const int32_t devopts[] = {
	SR_CONF_SAMPLERATE,
	SR_CONF_LIMIT_SAMPLES,
	SR_CONF_VLD_CH_NUM,
	SR_CONF_PROBE_EN,
};

SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_find(
	uint16_t vid, uint16_t pid, const char *manufacturer, const char *product)
{
	for (int i = 0; supported_fx2[i].vid; i++) {
		const struct fx2lafw_profile *profile = &supported_fx2[i];
		if (profile->vid != vid || profile->pid != pid)
			continue;
		if (profile->usb_manufacturer &&
				(!manufacturer || strcmp(profile->usb_manufacturer, manufacturer)))
			continue;
		if (profile->usb_product &&
				(!product || strcmp(profile->usb_product, product)))
			continue;
		return profile;
	}

	return NULL;
}

SR_PRIV int fx2lafw_profile_channel_count(const struct fx2lafw_profile *profile)
{
	if (!profile)
		return 0;

	return (profile->dev_caps & FX2LAFW_DEV_CAPS_16BIT) ? 16 : 8;
}

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
	(void)options;
	return NULL;
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
	const struct sr_channel *ch, const struct sr_channel_group *cg)
{
	const struct fx2lafw_context *devc;

	(void)cg;

	if (!data)
		return SR_ERR_ARG;
	if (!sdi || !sdi->priv)
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
		*data = g_variant_new_int16(fx2lafw_profile_channel_count(devc->profile));
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
	struct fx2lafw_context *devc;

	(void)cg;

	if (!data || !sdi || !sdi->priv)
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
			devopts, ARRAY_SIZE(devopts) * sizeof(int32_t), TRUE, NULL, NULL);
		return SR_OK;
	case SR_CONF_SAMPLERATE:
		*data = samplerates_variant();
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

SR_PRIV struct sr_dev_driver fx2lafw_driver_info = {
	.name = "fx2lafw",
	.longname = "fx2lafw (upstream compat scan-only)",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = hw_init,
	.cleanup = hw_cleanup,
	.scan = hw_scan,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.priv = NULL,
};
