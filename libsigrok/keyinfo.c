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
#include <libsigrok/libsigrok.h>

static const struct sr_key_info config_keys[] = {
	{ SR_CONF_SAMPLERATE, SR_T_UINT64, "samplerate", "Sample rate", NULL },
	{ SR_CONF_LIMIT_SAMPLES, SR_T_UINT64, "limit_samples", "Sample limit", NULL },
	{ SR_CONF_REF_MIN, SR_T_UINT32, "ref_min", "Reference minimum", NULL },
	{ SR_CONF_REF_MAX, SR_T_UINT32, "ref_max", "Reference maximum", NULL },
	{ 0, 0, NULL, NULL, NULL },
};

SR_API const struct sr_key_info *sr_key_info_get(int keytype, uint32_t key)
{
	int i;

	if (keytype != SR_KEY_CONFIG)
		return NULL;

	for (i = 0; config_keys[i].key; i++) {
		if (config_keys[i].key == key)
			return &config_keys[i];
	}

	return NULL;
}
