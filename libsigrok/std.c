/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
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

#include "libsigrok-internal.h"
#include <glib.h>
#include <string.h>
#include "log.h"
#include <assert.h>

/**
 * Standard sr_driver_init() API helper.
 *
 * This function can be used to simplify most driver's hw_init() API callback.
 *
 * It creates a new 'struct drv_context' (drvc), assigns sr_ctx to it, and
 * then 'drvc' is assigned to the 'struct sr_dev_driver' (di) that is passed.
 *
 * @param sr_ctx The libsigrok context to assign.
 * @param di The driver instance to use.
 * @param prefix A driver-specific prefix string used for log messages.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments, or
 *         SR_ERR_MALLOC upon memory allocation errors.
 */
SR_PRIV int std_hw_init(struct sr_context *sr_ctx, struct sr_dev_driver *di,
			const char *prefix)
{
	struct drv_context *drvc;

	if (!di) {
		sr_err("%sInvalid driver, cannot initialize.", prefix);
		return SR_ERR_ARG;
	}

	if (!(drvc = malloc(sizeof(struct drv_context)))) {
		sr_err("%sDriver context malloc failed.", prefix);
		return SR_ERR_MALLOC;
	}
	// not need init.

	drvc->sr_ctx = sr_ctx;
	di->priv = drvc;

	return SR_OK;
}

/**
 * Standard API helper for sending an SR_DF_HEADER packet.
 *
 * This function can be used to simplify most driver's
 * hw_dev_acquisition_start() API callback.
 *
 * @param sdi The device instance to use.
 * @param prefix A driver-specific prefix string used for log messages.
 * 		 Must not be NULL. An empty string is allowed.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments, or
 *         SR_ERR upon other errors.
 */
SR_PRIV int std_session_send_df_header(const struct sr_dev_inst *sdi)
{
	int ret;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_header header;

	if (!sdi) {
		sr_err("Invalid device instance.");
		return SR_ERR_ARG;
	}

	/* Send header packet to the session bus. */
	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_HEADER;
    packet.status = SR_PKT_OK;
	packet.payload = (uint8_t *)&header;
	header.feed_version = 1;
	gettimeofday(&header.starttime, NULL);

	if ((ret = sr_session_send(sdi, &packet)) < 0) {
		sr_err("Failed to send header packet: %d.", ret);
		return ret;
	}

	return SR_OK;
}

static int send_df_without_payload(const struct sr_dev_inst *sdi,
		uint16_t packet_type)
{
	struct sr_datafeed_packet packet;

	if (!sdi)
		return SR_ERR_ARG;

	memset(&packet, 0, sizeof(packet));
	packet.type = packet_type;
	packet.status = SR_PKT_OK;
	return sr_session_send(sdi, &packet);
}

SR_PRIV int std_session_send_df_end(const struct sr_dev_inst *sdi)
{
	return send_df_without_payload(sdi, SR_DF_END);
}

SR_PRIV int std_session_send_df_trigger(const struct sr_dev_inst *sdi)
{
	return send_df_without_payload(sdi, SR_DF_TRIGGER);
}

SR_PRIV int std_session_send_df_frame_begin(const struct sr_dev_inst *sdi)
{
	return send_df_without_payload(sdi, SR_DF_FRAME_BEGIN);
}

SR_PRIV int std_session_send_df_frame_end(const struct sr_dev_inst *sdi)
{
	return send_df_without_payload(sdi, SR_DF_FRAME_END);
}
