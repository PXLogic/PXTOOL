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

#ifndef UPSTREAM_FX2LAFW_H
#define UPSTREAM_FX2LAFW_H

#include "libsigrok-internal.h"

#define FX2LAFW_DEV_CAPS_16BIT (1 << 0)

struct fx2lafw_profile {
	uint16_t vid;
	uint16_t pid;
	const char *vendor;
	const char *model;
	const char *firmware;
	uint32_t dev_caps;
	const char *usb_manufacturer;
	const char *usb_product;
};

SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_find(
	uint16_t vid, uint16_t pid, const char *manufacturer, const char *product);
SR_PRIV int fx2lafw_profile_channel_count(const struct fx2lafw_profile *profile);
SR_PRIV int fx2lafw_firmware_path(const struct fx2lafw_profile *profile,
	char **path);
extern SR_PRIV struct sr_dev_driver fx2lafw_driver_info;

#endif
