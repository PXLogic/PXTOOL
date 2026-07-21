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

#include "test_datafeed_stub.h"

#include <string.h>
#include <type_traits>

extern "C" {
#include "libsigrok-internal.h"

void test_input_observer_record_packet(const struct sr_datafeed_packet *packet);
}

static test_captured_datafeed_packet g_captured_packet;

void test_datafeed_reset(void)
{
    memset(&g_captured_packet, 0, sizeof(g_captured_packet));
}

const test_captured_datafeed_packet *test_datafeed_last_packet(void)
{
    return &g_captured_packet;
}

extern "C" int ds_data_forward(const struct sr_dev_inst *sdi,
    const struct sr_datafeed_packet *packet)
{
    (void)sdi;
    test_datafeed_reset();

    if (!packet)
        return SR_ERR_ARG;

    test_input_observer_record_packet(packet);

    g_captured_packet.type = packet->type;
    g_captured_packet.status = packet->status;
    if (packet->type == SR_DF_LOGIC && packet->payload) {
        const struct sr_datafeed_logic *logic =
            static_cast<const struct sr_datafeed_logic *>(packet->payload);
        g_captured_packet.logic_length = logic->length;
        g_captured_packet.logic_format = logic->format;
        g_captured_packet.logic_unitsize = logic->unitsize;
        g_captured_packet.logic_data = logic->data;
    }

    return SR_OK;
}
