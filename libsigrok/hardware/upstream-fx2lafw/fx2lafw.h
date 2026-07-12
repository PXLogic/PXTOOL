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
#define FX2LAFW_FIRMWARE_DIR "fx2lafw"
#define FX2LAFW_USB_INTERFACE 0
#define FX2LAFW_USB_CONFIGURATION 1
#define FX2LAFW_UNKNOWN_ADDRESS 0xff
#define FX2LAFW_MAX_RENUM_DELAY_MS 3000
#define FX2LAFW_REQUIRED_VERSION_MAJOR 1
#define FX2LAFW_CMD_GET_FW_VERSION 0xb0
#define FX2LAFW_CMD_START 0xb1
#define FX2LAFW_BULK_ENDPOINT (2 | LIBUSB_ENDPOINT_IN)
#define FX2LAFW_NUM_SIMUL_TRANSFERS 32
#define FX2LAFW_MAX_EMPTY_TRANSFERS (FX2LAFW_NUM_SIMUL_TRANSFERS * 2)
#define FX2LAFW_MAX_16BIT_SAMPLE_RATE SR_MHZ(12)
#define FX2LAFW_MAX_SAMPLE_DELAY (6 * 256)
#define FX2LAFW_CMD_START_FLAGS_SAMPLE_8BIT (0 << 5)
#define FX2LAFW_CMD_START_FLAGS_SAMPLE_16BIT (1 << 5)
#define FX2LAFW_CMD_START_FLAGS_CLK_30MHZ (0 << 6)
#define FX2LAFW_CMD_START_FLAGS_CLK_48MHZ (1 << 6)
#define FX2LAFW_USB_TIMEOUT_MS 100

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

#pragma pack(push, 1)
struct fx2lafw_start_command {
	uint8_t flags;
	uint8_t sample_delay_h;
	uint8_t sample_delay_l;
};
#pragma pack(pop)

SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_find(
	uint16_t vid, uint16_t pid, const char *manufacturer, const char *product);
SR_PRIV size_t fx2lafw_profile_count(void);
SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_get(size_t index);
SR_PRIV int fx2lafw_profile_channel_count(const struct fx2lafw_profile *profile);
SR_PRIV int fx2lafw_firmware_path(const struct fx2lafw_profile *profile,
	char **path);
SR_PRIV int fx2lafw_has_firmware(const char *manufacturer,
	const char *product);
SR_PRIV struct sr_dev_inst *fx2lafw_dev_inst_new_for_profile(
	const struct fx2lafw_profile *profile, uint8_t bus, uint8_t address,
	int status, gboolean firmware_loaded, gint64 fw_updated);
SR_PRIV uint16_t fx2lafw_enabled_channel_mask(const struct sr_dev_inst *sdi);
SR_PRIV gboolean fx2lafw_sample_wide_for_channels(const struct sr_dev_inst *sdi);
SR_PRIV int fx2lafw_build_start_command(uint64_t samplerate,
	gboolean sample_wide, struct fx2lafw_start_command *command);
SR_PRIV size_t fx2lafw_transfer_buffer_size(uint64_t samplerate);
SR_PRIV unsigned int fx2lafw_transfer_count(uint64_t samplerate);
SR_PRIV unsigned int fx2lafw_transfer_timeout_ms(uint64_t samplerate);
SR_PRIV int fx2lafw_send_logic_packet(const struct sr_dev_inst *sdi,
	const uint8_t *data, size_t length, size_t unitsize);
extern SR_PRIV struct sr_dev_driver fx2lafw_driver_info;

#endif
