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

#include "device_source.h"

static gboolean variant_option_list_contains(GVariant *data, int key)
{
	GVariantIter iter;
	int32_t item;

	if (!data)
		return FALSE;

	if (!g_variant_is_of_type(data, G_VARIANT_TYPE("ai")))
		return FALSE;

	g_variant_iter_init(&iter, data);
	while (g_variant_iter_next(&iter, "i", &item)) {
		if ((int)item == key)
			return TRUE;
	}

	return FALSE;
}

SR_PRIV void ds_device_source_set(struct sr_dev_inst *sdi, int source_kind)
{
	if (!sdi)
		return;

	sdi->source_kind = source_kind;
}

SR_PRIV int ds_device_source_get(const struct sr_dev_inst *sdi)
{
	if (!sdi)
		return DS_DEVICE_SOURCE_UNKNOWN;

	if (sdi->source_kind != DS_DEVICE_SOURCE_UNKNOWN)
		return sdi->source_kind;

	switch (sdi->dev_type) {
	case DEV_TYPE_DEMO:
		return DS_DEVICE_SOURCE_DEMO;
	case DEV_TYPE_FILELOG:
		return DS_DEVICE_SOURCE_FILE;
	case DEV_TYPE_USB:
		return DS_DEVICE_SOURCE_NATIVE;
	default:
		return DS_DEVICE_SOURCE_UNKNOWN;
	}
}

SR_PRIV gboolean ds_device_supports_config_key(const struct sr_dev_inst *sdi, int key)
{
	GVariant *data;
	gboolean supported;
	int ret;

	if (!sdi || !sdi->driver || !sdi->driver->config_list)
		return FALSE;

	data = NULL;
	ret = sdi->driver->config_list(SR_CONF_DEVICE_OPTIONS, &data, sdi, NULL);
	if (ret != SR_OK || !data)
		return FALSE;

	supported = variant_option_list_contains(data, key);
	g_variant_unref(data);
	return supported;
}

SR_PRIV gboolean ds_device_supports_capability(const struct sr_dev_inst *sdi, int capability)
{
	if (!sdi)
		return FALSE;

	switch (capability) {
	case DS_DEVICE_CAP_WAVEFORM:
		return sdi->mode == LOGIC || sdi->mode == ANALOG || sdi->mode == DSO;
	case DS_DEVICE_CAP_LOGIC:
		return sdi->mode == LOGIC;
	case DS_DEVICE_CAP_ANALOG:
		return sdi->mode == ANALOG;
	case DS_DEVICE_CAP_DSO:
		return sdi->mode == DSO;
	case DS_DEVICE_CAP_ADVANCED_TRIGGER:
		return ds_device_supports_config_key(sdi, SR_CONF_HAVE_ADVANCED_TRIGGER);
	case DS_DEVICE_CAP_STREAM:
		return ds_device_supports_config_key(sdi, SR_CONF_STREAM);
	case DS_DEVICE_CAP_DISK_CACHE:
		return ds_device_supports_config_key(sdi, SR_CONF_DISK_CACHE_ENABLE)
			|| ds_device_supports_config_key(sdi, SR_CONF_DISK_CACHE_PATH);
	default:
		return FALSE;
	}
}
