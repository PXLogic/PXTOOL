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

#include "libsigrok-internal.h"
#include "hardware/pxlogic/pxlogic.h"

extern SR_PRIV struct sr_dev_driver px_driver_test_info;

int test_pxlogic_stream_config_state(void)
{
    struct sr_dev_inst sdi = {0};
    struct PX_context devc = {0};
    GVariant *stream = NULL;

    sdi.priv = &devc;
    sdi.status = SR_ST_ACTIVE;
    devc.op_mode = OP_STREAM;

    if (px_driver_test_info.config_get(SR_CONF_STREAM, &stream, &sdi,
            NULL, NULL) != SR_OK || !stream)
        return 0;
    if (!g_variant_get_boolean(stream)) {
        g_variant_unref(stream);
        return 0;
    }
    g_variant_unref(stream);

    if (px_driver_test_info.config_set(SR_CONF_LOOP_MODE,
            g_variant_new_boolean(TRUE), &sdi, NULL, NULL) != SR_OK)
        return 0;

    return devc.is_loop == 1;
}

int test_pxlogic_session_options_exclude_runtime_state(void)
{
    struct sr_dev_inst sdi = {0};
    GVariant *options = NULL;
    gsize count = 0;
    const int32_t *keys;

    if (px_driver_test_info.config_list(SR_CONF_DEVICE_SESSIONS, &options,
            &sdi, NULL) != SR_OK || !options)
        return 0;

    keys = g_variant_get_fixed_array(options, &count, sizeof(*keys));
    for (gsize i = 0; i < count; i++) {
        if (keys[i] == SR_CONF_STREAM || keys[i] == SR_CONF_LOOP_MODE) {
            g_variant_unref(options);
            return 0;
        }
    }

    g_variant_unref(options);
    return 1;
}

#ifndef HAVE_UPSTREAM_FX2LAFW
char DS_RES_PATH[500];

void ds_set_last_error(int error)
{
    (void)error;
}
#endif

GSList *sr_usb_find(libusb_context *usb_ctx, const char *conn)
{
    (void)usb_ctx;
    (void)conn;
    return NULL;
}

int sr_usb_device_is_exists(libusb_device *usb_dev)
{
    (void)usb_dev;
    return 0;
}
