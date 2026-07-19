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
#include "device_source.h"
#include "../../log.h"

#include <libusb.h>
#include <stdio.h>
#include <string.h>

#undef LOG_PREFIX
#define LOG_PREFIX "upstream-fx2lafw: "

struct fx2lafw_context {
	const struct fx2lafw_profile *profile;
	uint64_t samplerate;
	uint64_t limit_samples;
	gint64 fw_updated;
	gboolean firmware_loaded;
	gboolean acquisition_running;
	gboolean acquisition_aborted;
	gboolean sample_wide;
	uint64_t sent_samples;
	unsigned int submitted_transfers;
	unsigned int num_transfers;
	unsigned int empty_transfer_count;
	struct libusb_transfer **transfers;
	gintptr event_source;
	gboolean event_source_added;
	gboolean end_sent;
	gboolean logged_first_sample_block;
};

struct fx2lafw_driver_context {
	libusb_context *libusb_ctx;
	gboolean owns_libusb_ctx;
};

#pragma pack(push, 1)
struct fx2lafw_version_info {
	uint8_t major;
	uint8_t minor;
};
#pragma pack(pop)

static int command_get_fw_version(libusb_device_handle *devhdl,
	struct fx2lafw_version_info *version)
{
	int ret;

	if (!devhdl || !version)
		return SR_ERR_ARG;

	ret = libusb_control_transfer(devhdl,
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
		FX2LAFW_CMD_GET_FW_VERSION, 0x0000, 0x0000,
		(unsigned char *)version, sizeof(*version),
		FX2LAFW_USB_TIMEOUT_MS);
	if (ret < 0) {
		sr_err("Unable to get fx2lafw firmware version: %s.",
			libusb_error_name(ret));
		return SR_ERR;
	}
	if (ret != (int)sizeof(*version)) {
		sr_err("Short fx2lafw firmware version response: %d bytes.", ret);
		return SR_ERR;
	}

	return SR_OK;
}

static int command_start_acquisition(const struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;
	struct sr_usb_dev_inst *usb;
	struct fx2lafw_start_command command;
	int ret;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;

	devc = sdi->priv;
	usb = sdi->conn;
	if (!usb->devhdl)
		return SR_ERR_DEVICE_CLOSED;

	ret = fx2lafw_build_start_command(devc->samplerate,
		devc->sample_wide, &command);
	if (ret != SR_OK)
		return ret;

	ret = libusb_control_transfer(usb->devhdl,
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
		FX2LAFW_CMD_START, 0x0000, 0x0000,
		(unsigned char *)&command, sizeof(command),
		FX2LAFW_USB_TIMEOUT_MS);
	if (ret < 0) {
		sr_err("Unable to send fx2lafw start command: %s.",
			libusb_error_name(ret));
		return SR_ERR;
	}
	if (ret != (int)sizeof(command)) {
		sr_err("Short fx2lafw start command write: %d bytes.", ret);
		return SR_ERR;
	}

	return SR_OK;
}

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

SR_PRIV size_t fx2lafw_profile_count(void)
{
	size_t count;

	for (count = 0; supported_fx2[count].vid; count++)
		;

	return count;
}

SR_PRIV const struct fx2lafw_profile *fx2lafw_profile_get(size_t index)
{
	if (index >= fx2lafw_profile_count())
		return NULL;

	return &supported_fx2[index];
}

SR_PRIV int fx2lafw_profile_channel_count(const struct fx2lafw_profile *profile)
{
	if (!profile)
		return 0;

	return (profile->dev_caps & FX2LAFW_DEV_CAPS_16BIT) ? 16 : 8;
}

SR_PRIV uint16_t fx2lafw_enabled_channel_mask(const struct sr_dev_inst *sdi)
{
	GSList *l;
	uint16_t mask;

	if (!sdi)
		return 0;

	mask = 0;
	for (l = sdi->channels; l; l = l->next) {
		struct sr_channel *channel = l->data;
		if (channel && channel->enabled && channel->index < 16)
			mask |= (uint16_t)(1U << channel->index);
	}

	return mask;
}

SR_PRIV gboolean fx2lafw_sample_wide_for_channels(const struct sr_dev_inst *sdi)
{
	return (fx2lafw_enabled_channel_mask(sdi) & 0xff00) != 0;
}

SR_PRIV int fx2lafw_build_start_command(uint64_t samplerate,
	gboolean sample_wide, struct fx2lafw_start_command *command)
{
	uint64_t delay;
	gboolean delay_valid;

	if (!command)
		return SR_ERR_ARG;
	memset(command, 0, sizeof(*command));

	if (samplerate == 0)
		return SR_ERR;
	if (sample_wide && samplerate > FX2LAFW_MAX_16BIT_SAMPLE_RATE)
		return SR_ERR;

	delay = 0;
	delay_valid = FALSE;
	if (SR_MHZ(48) % samplerate == 0) {
		delay = SR_MHZ(48) / samplerate - 1;
		if (delay <= FX2LAFW_MAX_SAMPLE_DELAY) {
			command->flags = FX2LAFW_CMD_START_FLAGS_CLK_48MHZ;
			delay_valid = TRUE;
		}
	}

	if (!delay_valid && SR_MHZ(30) % samplerate == 0) {
		delay = SR_MHZ(30) / samplerate - 1;
		command->flags = FX2LAFW_CMD_START_FLAGS_CLK_30MHZ;
		delay_valid = TRUE;
	}

	if (!delay_valid || delay > FX2LAFW_MAX_SAMPLE_DELAY)
		return SR_ERR;

	command->sample_delay_h = (delay >> 8) & 0xff;
	command->sample_delay_l = delay & 0xff;
	command->flags |= sample_wide ?
		FX2LAFW_CMD_START_FLAGS_SAMPLE_16BIT :
		FX2LAFW_CMD_START_FLAGS_SAMPLE_8BIT;

	return SR_OK;
}

static unsigned int fx2lafw_bytes_per_ms(uint64_t samplerate)
{
	if (samplerate == 0)
		return 0;

	return samplerate / 1000;
}

SR_PRIV size_t fx2lafw_transfer_buffer_size(uint64_t samplerate)
{
	size_t size;
	unsigned int bytes_per_ms;

	bytes_per_ms = fx2lafw_bytes_per_ms(samplerate);
	if (bytes_per_ms == 0)
		return 0;

	size = 10 * bytes_per_ms;
	return (size + 511) & ~((size_t)511);
}

SR_PRIV unsigned int fx2lafw_transfer_count(uint64_t samplerate)
{
	size_t buffer_size;
	unsigned int bytes_per_ms;
	unsigned int count;

	bytes_per_ms = fx2lafw_bytes_per_ms(samplerate);
	buffer_size = fx2lafw_transfer_buffer_size(samplerate);
	if (bytes_per_ms == 0 || buffer_size == 0)
		return 0;

	count = (500 * bytes_per_ms) / buffer_size;
	if (count == 0)
		count = 1;
	if (count > FX2LAFW_NUM_SIMUL_TRANSFERS)
		count = FX2LAFW_NUM_SIMUL_TRANSFERS;

	return count;
}

SR_PRIV unsigned int fx2lafw_transfer_timeout_ms(uint64_t samplerate)
{
	size_t total_size;
	unsigned int bytes_per_ms;
	unsigned int timeout;

	bytes_per_ms = fx2lafw_bytes_per_ms(samplerate);
	if (bytes_per_ms == 0)
		return 0;

	total_size = fx2lafw_transfer_buffer_size(samplerate) *
		fx2lafw_transfer_count(samplerate);
	timeout = total_size / bytes_per_ms;
	return timeout + timeout / 4;
}

SR_PRIV int fx2lafw_send_logic_packet(const struct sr_dev_inst *sdi,
	const uint8_t *data, size_t length, size_t unitsize)
{
	struct sr_datafeed_logic logic;
	struct sr_datafeed_packet packet;

	if (!sdi || !data || length == 0 || unitsize == 0)
		return SR_ERR_ARG;

	memset(&logic, 0, sizeof(logic));
	memset(&packet, 0, sizeof(packet));

	logic.length = length;
	logic.format = LA_CROSS_DATA;
	logic.unitsize = unitsize;
	logic.data = (void *)data;

	packet.type = SR_DF_LOGIC;
	packet.status = SR_PKT_OK;
	packet.payload = &logic;

	return ds_data_forward(sdi, &packet);
}

SR_PRIV size_t fx2lafw_pack_interleaved_samples(const uint8_t *input,
	size_t sample_count, size_t unitsize, uint8_t *output)
{
	const size_t channel_count = unitsize * 8;
	size_t block;

	if (!input || !output || (unitsize != 1 && unitsize != 2) ||
			sample_count == 0 || sample_count % 64 != 0)
		return 0;

	for (block = 0; block < sample_count / 64; block++) {
		size_t channel;
		for (channel = 0; channel < channel_count; channel++) {
			uint64_t channel_samples = 0;
			size_t sample;

			for (sample = 0; sample < 64; sample++) {
				const size_t index = (block * 64 + sample) * unitsize;
				uint16_t value = input[index];
				if (unitsize == 2)
					value |= (uint16_t)input[index + 1] << 8;
				channel_samples |= (uint64_t)((value >> channel) & 1) << sample;
			}
			memcpy(output + (block * channel_count + channel) *
					sizeof(channel_samples), &channel_samples,
				sizeof(channel_samples));
		}
	}

	return sample_count * unitsize;
}

static int fx2lafw_forward_interleaved_samples(const struct sr_dev_inst *sdi,
	const uint8_t *data, size_t sample_count, size_t unitsize)
{
	struct fx2lafw_context *devc;
	uint8_t *packed;
	size_t length;
	int ret;

	if (!sdi || !data || sample_count == 0 || sample_count % 64 != 0)
		return SR_ERR_ARG;

	length = sample_count * unitsize;
	packed = g_try_malloc(length);
	if (!packed)
		return SR_ERR_MALLOC;
	if (fx2lafw_pack_interleaved_samples(data, sample_count, unitsize,
			packed) != length) {
		g_free(packed);
		return SR_ERR;
	}

	devc = sdi->priv;
	if (devc && !devc->logged_first_sample_block) {
		const size_t channel_count = unitsize * 8;
		GString *message = g_string_new("FX2 first 64 samples:");
		size_t channel;

		for (channel = 0; channel < channel_count; channel++) {
			uint64_t samples;
			memcpy(&samples, packed + channel * sizeof(samples),
				sizeof(samples));
			g_string_append_printf(message, " D%zu=0x%016llx", channel,
				(unsigned long long)samples);
		}
		sr_info("%s", message->str);
		g_string_free(message, TRUE);
		devc->logged_first_sample_block = TRUE;
	}

	ret = fx2lafw_send_logic_packet(sdi, packed, length, unitsize);
	g_free(packed);
	return ret;
}

SR_PRIV int fx2lafw_firmware_path(const struct fx2lafw_profile *profile,
	char **path)
{
	size_t dir_len;

	if (!path)
		return SR_ERR_ARG;
	*path = NULL;

	if (!profile || !profile->firmware)
		return SR_ERR_ARG;
	if (DS_RES_PATH[0] == '\0')
		return SR_ERR_FIRMWARE_NOT_EXIST;

	dir_len = strlen(DS_RES_PATH);
	if (dir_len > 0 && DS_RES_PATH[dir_len - 1] == '/')
		*path = g_strdup_printf("%s%s/%s", DS_RES_PATH,
			FX2LAFW_FIRMWARE_DIR, profile->firmware);
	else
		*path = g_strdup_printf("%s/%s/%s", DS_RES_PATH,
			FX2LAFW_FIRMWARE_DIR, profile->firmware);

	return *path ? SR_OK : SR_ERR_MALLOC;
}

SR_PRIV int fx2lafw_has_firmware(const char *manufacturer,
	const char *product)
{
	return manufacturer && product &&
		strcmp(manufacturer, "sigrok") == 0 &&
		strcmp(product, "fx2lafw") == 0;
}

static int hw_init(struct sr_context *ctx)
{
	struct fx2lafw_driver_context *drvc;

	drvc = g_malloc0(sizeof(*drvc));
	if (!drvc)
		return SR_ERR_MALLOC;

	if (ctx && ctx->libusb_ctx) {
		drvc->libusb_ctx = ctx->libusb_ctx;
		drvc->owns_libusb_ctx = FALSE;
	} else if (libusb_init(&drvc->libusb_ctx) == 0) {
		drvc->owns_libusb_ctx = TRUE;
	} else {
		g_free(drvc);
		return SR_ERR;
	}

	fx2lafw_driver_info.priv = drvc;
	return SR_OK;
}

static int hw_cleanup(void)
{
	struct fx2lafw_driver_context *drvc;

	drvc = fx2lafw_driver_info.priv;
	if (!drvc)
		return SR_OK;

	if (drvc->owns_libusb_ctx && drvc->libusb_ctx)
		libusb_exit(drvc->libusb_ctx);
	g_free(drvc);
	fx2lafw_driver_info.priv = NULL;

	return SR_OK;
}

SR_PRIV struct sr_dev_inst *fx2lafw_dev_inst_new_for_profile(
	const struct fx2lafw_profile *profile, uint8_t bus, uint8_t address,
	int status, gboolean firmware_loaded, gint64 fw_updated)
{
	struct fx2lafw_context *devc;
	struct sr_dev_inst *sdi;
	int channel_count;

	if (!profile)
		return NULL;

	sdi = sr_dev_inst_new(LOGIC, status,
		profile->vendor, profile->model, NULL);
	if (!sdi)
		return NULL;

	devc = g_malloc0(sizeof(*devc));
	devc->profile = profile;
	devc->samplerate = samplerates[0];
	devc->limit_samples = SR_MHZ(1);
	devc->firmware_loaded = firmware_loaded;
	devc->fw_updated = fw_updated;
	devc->event_source = -2;
	sdi->priv = devc;
	sdi->driver = &fx2lafw_driver_info;
	sdi->dev_type = DEV_TYPE_USB;
	sdi->conn = sr_usb_dev_inst_new(bus, address);
	ds_device_source_set(sdi, DS_DEVICE_SOURCE_UPSTREAM_COMPAT);

	channel_count = fx2lafw_profile_channel_count(profile);
	for (int i = 0; i < channel_count; i++) {
		char name[8];
		struct sr_channel *probe;

		snprintf(name, sizeof(name), "D%d", i);
		probe = sr_channel_new(i, SR_CHANNEL_LOGIC, TRUE, name);
		if (!probe) {
			sr_dev_inst_free(sdi);
			return NULL;
		}
		sdi->channels = g_slist_append(sdi->channels, probe);
	}

	return sdi;
}

static void close_usb_handle(struct sr_dev_inst *sdi)
{
	struct sr_usb_dev_inst *usb;

	if (!sdi || !sdi->conn)
		return;

	usb = sdi->conn;
	if (!usb->devhdl)
		return;

	libusb_release_interface(usb->devhdl, FX2LAFW_USB_INTERFACE);
	libusb_close(usb->devhdl);
	usb->devhdl = NULL;
}

static GSList *hw_scan(GSList *options)
{
	struct fx2lafw_driver_context *drvc;
	libusb_device **devlist;
	GSList *devices;
	ssize_t device_count;

	(void)options;

	drvc = fx2lafw_driver_info.priv;
	if (!drvc || !drvc->libusb_ctx)
		return NULL;

	devices = NULL;
	device_count = libusb_get_device_list(drvc->libusb_ctx, &devlist);
	if (device_count < 0)
		return NULL;

	for (ssize_t i = 0; i < device_count; i++) {
		struct libusb_device_descriptor desc;
		libusb_device_handle *handle;
		char manufacturer[64];
		char product[64];
		const struct fx2lafw_profile *profile;

		manufacturer[0] = '\0';
		product[0] = '\0';

		if (libusb_get_device_descriptor(devlist[i], &desc) < 0)
			continue;

		profile = fx2lafw_profile_find(desc.idVendor, desc.idProduct, "", "");
		if (!profile)
			continue;

		if (libusb_open(devlist[i], &handle) == 0) {
			if (desc.iManufacturer)
				libusb_get_string_descriptor_ascii(handle, desc.iManufacturer,
					(unsigned char *)manufacturer, sizeof(manufacturer));
			if (desc.iProduct)
				libusb_get_string_descriptor_ascii(handle, desc.iProduct,
					(unsigned char *)product, sizeof(product));
			libusb_close(handle);
		}

		profile = fx2lafw_profile_find(desc.idVendor, desc.idProduct,
			manufacturer, product);
		if (!profile)
			continue;

		gboolean has_firmware;
		gint64 fw_updated;
		uint8_t address;
		int status;

		has_firmware = fx2lafw_has_firmware(manufacturer, product);
		fw_updated = 0;
		address = libusb_get_device_address(devlist[i]);
		status = SR_ST_INACTIVE;

		if (!has_firmware) {
			char *firmware;
			int upload_ret;

			firmware = NULL;
			upload_ret = fx2lafw_firmware_path(profile, &firmware);
			if (upload_ret == SR_OK &&
					!g_file_test(firmware, G_FILE_TEST_IS_REGULAR)) {
				sr_err("fx2lafw firmware resource is missing: %s.", firmware);
				upload_ret = SR_ERR_FIRMWARE_NOT_EXIST;
			}
			if (upload_ret == SR_OK) {
				upload_ret = ezusb_upload_firmware(devlist[i],
					FX2LAFW_USB_CONFIGURATION, firmware);
			}
			g_free(firmware);
			if (upload_ret == SR_OK) {
				sr_info("fx2lafw firmware uploaded for device %d.%d; waiting for re-enumeration.",
					libusb_get_bus_number(devlist[i]),
					libusb_get_device_address(devlist[i]));
				continue;
			} else {
				sr_err("Firmware upload failed for device %d.%d, name %s.",
					libusb_get_bus_number(devlist[i]),
					libusb_get_device_address(devlist[i]),
					profile->firmware ? profile->firmware : "(null)");
				continue;
			}
		}

		struct sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(profile,
			libusb_get_bus_number(devlist[i]), address, status, has_firmware,
			fw_updated);
		if (sdi)
			devices = g_slist_append(devices, sdi);
	}

	libusb_free_device_list(devlist, 1);
	return devices;
}

static int open_matching_device(struct sr_dev_inst *sdi)
{
	struct fx2lafw_driver_context *drvc;
	struct fx2lafw_context *devc;
	struct sr_usb_dev_inst *usb;
	libusb_device **devlist;
	ssize_t device_count;
	int ret;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;

	drvc = fx2lafw_driver_info.priv;
	devc = sdi->priv;
	usb = sdi->conn;
	if (!drvc || !drvc->libusb_ctx || !devc->profile)
		return SR_ERR;

	device_count = libusb_get_device_list(drvc->libusb_ctx, &devlist);
	if (device_count < 0)
		return SR_ERR;

	ret = SR_ERR;
	for (ssize_t i = 0; i < device_count; i++) {
		struct libusb_device_descriptor desc;
		libusb_device_handle *handle;
		uint8_t bus;
		uint8_t address;

		if (libusb_get_device_descriptor(devlist[i], &desc) < 0)
			continue;
		if (desc.idVendor != devc->profile->vid ||
				desc.idProduct != devc->profile->pid)
			continue;

		bus = libusb_get_bus_number(devlist[i]);
		address = libusb_get_device_address(devlist[i]);
		if (usb->bus != bus)
			continue;
		if (usb->address != FX2LAFW_UNKNOWN_ADDRESS &&
				usb->address != address)
			continue;

		if (libusb_open(devlist[i], &handle) != 0)
			continue;
		if (usb->address == FX2LAFW_UNKNOWN_ADDRESS) {
			char manufacturer[64];
			char product[64];

			manufacturer[0] = '\0';
			product[0] = '\0';
			if (desc.iManufacturer)
				libusb_get_string_descriptor_ascii(handle, desc.iManufacturer,
					(unsigned char *)manufacturer, sizeof(manufacturer));
			if (desc.iProduct)
				libusb_get_string_descriptor_ascii(handle, desc.iProduct,
					(unsigned char *)product, sizeof(product));
			if (!fx2lafw_has_firmware(manufacturer, product)) {
				libusb_close(handle);
				continue;
			}
		}

		usb->bus = bus;
		usb->address = address;
		usb->devhdl = handle;
		ret = SR_OK;
		break;
	}

	libusb_free_device_list(devlist, 1);
	return ret;
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

static int hw_dev_open(struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;
	struct sr_usb_dev_inst *usb;
	struct fx2lafw_version_info version;
	int ret;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;

	devc = sdi->priv;
	usb = sdi->conn;

	if (usb->devhdl)
		return SR_OK;

	if (sdi->status == SR_ST_INITIALIZING && devc->fw_updated > 0) {
		gint64 waited_ms;

		g_usleep(300 * 1000);
		waited_ms = 0;
		while (waited_ms < FX2LAFW_MAX_RENUM_DELAY_MS) {
			ret = open_matching_device(sdi);
			if (ret == SR_OK)
				break;
			g_usleep(100 * 1000);
			waited_ms = (g_get_monotonic_time() - devc->fw_updated) / 1000;
		}
		if (!usb->devhdl) {
			sdi->status = SR_ST_INACTIVE;
			return SR_ERR;
		}
	} else {
		ret = open_matching_device(sdi);
		if (ret != SR_OK)
			return ret;
	}

#if !defined(__APPLE__)
	if (libusb_kernel_driver_active(usb->devhdl, FX2LAFW_USB_INTERFACE) == 1) {
		ret = libusb_detach_kernel_driver(usb->devhdl,
			FX2LAFW_USB_INTERFACE);
		if (ret < 0) {
			close_usb_handle(sdi);
			return SR_ERR;
		}
	}
#endif

	ret = libusb_claim_interface(usb->devhdl, FX2LAFW_USB_INTERFACE);
	if (ret != 0) {
		close_usb_handle(sdi);
		return SR_ERR;
	}

	ret = command_get_fw_version(usb->devhdl, &version);
	if (ret != SR_OK) {
		close_usb_handle(sdi);
		return ret;
	}

	if (version.major != FX2LAFW_REQUIRED_VERSION_MAJOR) {
		close_usb_handle(sdi);
		return SR_ERR_DEVICE_FIRMWARE_VERSION_LOW;
	}

	devc->firmware_loaded = TRUE;
	sdi->status = SR_ST_ACTIVE;
	return SR_OK;
}

static int hw_dev_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data);
static int fx2lafw_drain_transfers(struct sr_dev_inst *sdi);

static int hw_dev_close(struct sr_dev_inst *sdi)
{
	struct sr_usb_dev_inst *usb;

	if (!sdi || !sdi->conn)
		return SR_ERR_ARG;
	if (sdi->priv) {
		hw_dev_acquisition_stop(sdi, sdi);
		if (fx2lafw_drain_transfers(sdi) != SR_OK) {
			sr_err("Refusing to close fx2lafw device with live transfers.");
			return SR_ERR;
		}
	}

	usb = sdi->conn;
	if (!usb->devhdl) {
		sdi->status = SR_ST_INACTIVE;
		return SR_OK;
	}

	close_usb_handle(sdi);
	sdi->status = SR_ST_INACTIVE;
	return SR_OK;
}

static void fx2lafw_send_end_once(struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;
	struct sr_datafeed_packet packet;

	if (!sdi || !sdi->priv)
		return;

	devc = sdi->priv;
	if (devc->end_sent)
		return;

	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_END;
	packet.status = SR_PKT_OK;
	packet.payload = NULL;
	ds_data_forward(sdi, &packet);
	devc->end_sent = TRUE;
}

static void fx2lafw_remove_event_source(struct fx2lafw_context *devc)
{
	if (!devc || !devc->event_source_added)
		return;

	sr_session_source_remove(devc->event_source);
	devc->event_source_added = FALSE;
}

static void fx2lafw_free_transfer_array(struct fx2lafw_context *devc)
{
	if (!devc || devc->submitted_transfers != 0)
		return;

	devc->num_transfers = 0;
	g_free(devc->transfers);
	devc->transfers = NULL;
}

static void fx2lafw_finish_acquisition(struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;

	if (!sdi || !sdi->priv)
		return;

	devc = sdi->priv;
	fx2lafw_remove_event_source(devc);
	fx2lafw_send_end_once(sdi);
	devc->acquisition_running = FALSE;
	fx2lafw_free_transfer_array(devc);
}

static void fx2lafw_free_transfer(struct libusb_transfer *transfer)
{
	struct sr_dev_inst *sdi;
	struct fx2lafw_context *devc;
	unsigned int i;

	if (!transfer)
		return;

	sdi = transfer->user_data;
	devc = sdi ? sdi->priv : NULL;

	if (devc) {
		for (i = 0; i < devc->num_transfers; i++) {
			if (devc->transfers && devc->transfers[i] == transfer) {
				devc->transfers[i] = NULL;
				break;
			}
		}
		if (devc->submitted_transfers > 0)
			devc->submitted_transfers--;
	}

	g_free(transfer->buffer);
	transfer->buffer = NULL;
	libusb_free_transfer(transfer);

	if (sdi && devc && devc->submitted_transfers == 0)
		fx2lafw_finish_acquisition(sdi);
}

static void fx2lafw_abort_acquisition(struct fx2lafw_context *devc)
{
	unsigned int i;

	if (!devc || devc->acquisition_aborted)
		return;

	devc->acquisition_aborted = TRUE;
	for (i = 0; i < devc->num_transfers; i++) {
		if (devc->transfers && devc->transfers[i])
			libusb_cancel_transfer(devc->transfers[i]);
	}
}

static void fx2lafw_resubmit_transfer(struct libusb_transfer *transfer)
{
	int ret;

	ret = libusb_submit_transfer(transfer);
	if (ret == LIBUSB_SUCCESS)
		return;

	sr_err("Unable to resubmit fx2lafw transfer: %s.", libusb_error_name(ret));
	fx2lafw_free_transfer(transfer);
}

static void LIBUSB_CALL fx2lafw_receive_transfer(struct libusb_transfer *transfer)
{
	struct sr_dev_inst *sdi;
	struct fx2lafw_context *devc;
	size_t unitsize;
	size_t sample_count;
	size_t bytes_to_send;
	gboolean packet_has_error;

	sdi = transfer ? transfer->user_data : NULL;
	devc = sdi ? sdi->priv : NULL;
	if (!transfer || !sdi || !devc)
		return;

	if (devc->acquisition_aborted) {
		fx2lafw_free_transfer(transfer);
		return;
	}

	packet_has_error = FALSE;
	switch (transfer->status) {
	case LIBUSB_TRANSFER_NO_DEVICE:
		fx2lafw_abort_acquisition(devc);
		fx2lafw_free_transfer(transfer);
		return;
	case LIBUSB_TRANSFER_COMPLETED:
	case LIBUSB_TRANSFER_TIMED_OUT:
		break;
	default:
		packet_has_error = TRUE;
		break;
	}

	if (transfer->actual_length == 0 || packet_has_error) {
		devc->empty_transfer_count++;
		if (devc->empty_transfer_count > FX2LAFW_MAX_EMPTY_TRANSFERS) {
			fx2lafw_abort_acquisition(devc);
			fx2lafw_free_transfer(transfer);
		} else {
			fx2lafw_resubmit_transfer(transfer);
		}
		return;
	}

	devc->empty_transfer_count = 0;
	unitsize = devc->sample_wide ? 2 : 1;
	sample_count = transfer->actual_length / unitsize;
	if (sample_count == 0) {
		fx2lafw_resubmit_transfer(transfer);
		return;
	}

	if (devc->limit_samples &&
			devc->sent_samples + sample_count > devc->limit_samples)
		sample_count = devc->limit_samples - devc->sent_samples;
	bytes_to_send = sample_count * unitsize;

	if (bytes_to_send > 0) {
		const size_t packed_samples = sample_count - sample_count % 64;

		if (packed_samples > 0 && fx2lafw_forward_interleaved_samples(sdi,
				transfer->buffer, packed_samples, unitsize) != SR_OK) {
			fx2lafw_abort_acquisition(devc);
			fx2lafw_free_transfer(transfer);
			return;
		}
		devc->sent_samples += sample_count;
	}

	if (devc->limit_samples && devc->sent_samples >= devc->limit_samples) {
		fx2lafw_abort_acquisition(devc);
		fx2lafw_free_transfer(transfer);
	} else {
		fx2lafw_resubmit_transfer(transfer);
	}
}

static int fx2lafw_handle_events(int fd, int revents,
	const struct sr_dev_inst *cb_data)
{
	struct sr_dev_inst *sdi;
	struct fx2lafw_driver_context *drvc;
	struct timeval tv;

	(void)fd;
	(void)revents;

	sdi = (struct sr_dev_inst *)cb_data;
	if (!sdi || !sdi->priv)
		return FALSE;

	drvc = fx2lafw_driver_info.priv;
	if (!drvc || !drvc->libusb_ctx)
		return FALSE;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	libusb_handle_events_timeout(drvc->libusb_ctx, &tv);

	return TRUE;
}

static int fx2lafw_drain_transfers(struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;
	struct fx2lafw_driver_context *drvc;
	struct timeval tv;
	unsigned int i;
	int ret;

	if (!sdi || !sdi->priv)
		return SR_ERR_ARG;

	devc = sdi->priv;
	if (devc->submitted_transfers == 0)
		return SR_OK;

	drvc = fx2lafw_driver_info.priv;
	if (!drvc || !drvc->libusb_ctx) {
		sr_err("Unable to drain fx2lafw transfers without a libusb context.");
		goto undrained;
	}

	for (i = 0; i < 10 && devc->submitted_transfers > 0; i++) {
		tv.tv_sec = 0;
		tv.tv_usec = FX2LAFW_USB_TIMEOUT_MS * 1000;
		ret = libusb_handle_events_timeout(drvc->libusb_ctx, &tv);
		if (ret < 0) {
			sr_err("Unable to drain fx2lafw transfers: %s.",
				libusb_error_name(ret));
			goto undrained;
		}
	}

	if (devc->submitted_transfers == 0)
		return SR_OK;

	sr_err("Timed out draining fx2lafw transfers.");

undrained:
	/* Do not leave a session-owned callback pointing at live transfers. */
	fx2lafw_remove_event_source(devc);
	return SR_ERR;
}

static int fx2lafw_cleanup_start_failure(struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;

	if (!sdi || !sdi->priv)
		return SR_ERR_ARG;

	devc = sdi->priv;
	fx2lafw_abort_acquisition(devc);
	if (devc->submitted_transfers != 0 &&
			fx2lafw_drain_transfers(sdi) != SR_OK) {
		sr_err("Unable to clean up failed fx2lafw acquisition start.");
		return SR_ERR;
	}

	/* No callback runs when submission failed before the first transfer. */
	if (devc->submitted_transfers == 0)
		fx2lafw_finish_acquisition(sdi);

	return SR_OK;
}

static int fx2lafw_start_transfers(struct sr_dev_inst *sdi)
{
	struct fx2lafw_context *devc;
	struct sr_usb_dev_inst *usb;
	unsigned int num_transfers;
	unsigned int timeout;
	size_t size;
	unsigned int i;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;

	devc = sdi->priv;
	usb = sdi->conn;
	if (!usb->devhdl)
		return SR_ERR_DEVICE_CLOSED;
	if (devc->submitted_transfers != 0)
		return SR_ERR;

	fx2lafw_free_transfer_array(devc);

	size = fx2lafw_transfer_buffer_size(devc->samplerate);
	num_transfers = fx2lafw_transfer_count(devc->samplerate);
	timeout = fx2lafw_transfer_timeout_ms(devc->samplerate);
	if (size == 0 || num_transfers == 0 || timeout == 0)
		return SR_ERR;

	devc->transfers = g_try_malloc0(sizeof(*devc->transfers) * num_transfers);
	if (!devc->transfers)
		return SR_ERR_MALLOC;

	devc->num_transfers = num_transfers;
	devc->submitted_transfers = 0;
	for (i = 0; i < num_transfers; i++) {
		struct libusb_transfer *transfer;
		unsigned char *buffer;
		int ret;

		buffer = g_try_malloc(size);
		if (!buffer) {
			fx2lafw_abort_acquisition(devc);
			fx2lafw_free_transfer_array(devc);
			return SR_ERR_MALLOC;
		}

		transfer = libusb_alloc_transfer(0);
		if (!transfer) {
			g_free(buffer);
			fx2lafw_abort_acquisition(devc);
			fx2lafw_free_transfer_array(devc);
			return SR_ERR_MALLOC;
		}

		libusb_fill_bulk_transfer(transfer, usb->devhdl,
			FX2LAFW_BULK_ENDPOINT, buffer, size,
			fx2lafw_receive_transfer, sdi, timeout);
		ret = libusb_submit_transfer(transfer);
		if (ret != LIBUSB_SUCCESS) {
			sr_err("Unable to submit fx2lafw transfer: %s.",
				libusb_error_name(ret));
			g_free(buffer);
			libusb_free_transfer(transfer);
			fx2lafw_abort_acquisition(devc);
			fx2lafw_free_transfer_array(devc);
			return SR_ERR;
		}

		devc->transfers[i] = transfer;
		devc->submitted_transfers++;
	}

	return SR_OK;
}

static int hw_dev_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
	struct fx2lafw_context *devc;
	int ret;
	unsigned int timeout;

	(void)cb_data;

	if (!sdi || !sdi->priv || !sdi->conn)
		return SR_ERR_ARG;
	if (sdi->status != SR_ST_ACTIVE) {
		ds_set_last_error(SR_ERR_DEVICE_CLOSED);
		return SR_ERR_DEVICE_CLOSED;
	}

	devc = sdi->priv;
	devc->sample_wide = fx2lafw_sample_wide_for_channels(sdi);
	devc->sent_samples = 0;
	devc->logged_first_sample_block = FALSE;
	devc->empty_transfer_count = 0;
	devc->acquisition_aborted = FALSE;
	devc->end_sent = FALSE;

	timeout = fx2lafw_transfer_timeout_ms(devc->samplerate);
	if (timeout == 0)
		return SR_ERR;

	devc->event_source = -2;
	ret = sr_session_source_add(devc->event_source, 0, timeout,
		fx2lafw_handle_events, sdi);
	if (ret != SR_OK)
		return ret;
	devc->event_source_added = TRUE;

	ret = fx2lafw_start_transfers(sdi);
	if (ret != SR_OK) {
		if (fx2lafw_cleanup_start_failure(sdi) != SR_OK)
			return SR_ERR;
		return ret;
	}

	ret = std_session_send_df_header(sdi, LOG_PREFIX);
	if (ret != SR_OK) {
		if (fx2lafw_cleanup_start_failure(sdi) != SR_OK)
			return SR_ERR;
		return ret;
	}

	ret = command_start_acquisition(sdi);
	if (ret != SR_OK) {
		if (fx2lafw_cleanup_start_failure(sdi) != SR_OK)
			return SR_ERR;
		return ret;
	}

	devc->acquisition_running = TRUE;
	return SR_OK;
}

static int hw_dev_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
	struct fx2lafw_context *devc;

	(void)cb_data;

	if (!sdi || !sdi->priv)
		return SR_ERR_ARG;

	devc = sdi->priv;
	if (!devc->acquisition_running && devc->submitted_transfers == 0) {
		fx2lafw_free_transfer_array(devc);
		fx2lafw_remove_event_source(devc);
		return SR_OK;
	}

	fx2lafw_abort_acquisition(devc);
	if (devc->submitted_transfers == 0)
		fx2lafw_finish_acquisition((struct sr_dev_inst *)sdi);

	return SR_OK;
}

SR_PRIV struct sr_dev_driver fx2lafw_driver_info = {
	.name = "fx2lafw",
	.longname = "fx2lafw (upstream compat lifecycle)",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = hw_init,
	.cleanup = hw_cleanup,
	.scan = hw_scan,
	.dev_open = hw_dev_open,
	.dev_close = hw_dev_close,
	.dev_acquisition_start = hw_dev_acquisition_start,
	.dev_acquisition_stop = hw_dev_acquisition_stop,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.priv = NULL,
};
