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

#ifndef DEVICE_SOURCE_H
#define DEVICE_SOURCE_H

#include "libsigrok-internal.h"

SR_PRIV void ds_device_source_set(struct sr_dev_inst *sdi, int source_kind);
SR_PRIV int ds_device_source_get(const struct sr_dev_inst *sdi);
SR_PRIV gboolean ds_device_supports_config_key(const struct sr_dev_inst *sdi, int key);
SR_PRIV gboolean ds_device_supports_capability(const struct sr_dev_inst *sdi, int capability);

#endif
