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
 */

#include "libsigrok-internal.h"

struct sr_dev_inst *make_test_sdi(void)
{
    static struct sr_channel channel = {
        .index = 0,
        .type = SR_CHANNEL_LOGIC,
        .enabled = TRUE,
        .name = "D0",
    };
    static struct sr_dev_inst sdi;
    static gboolean initialized;

    if (!initialized) {
        sdi.channels = g_slist_append(NULL, &channel);
        initialized = TRUE;
    }

    return &sdi;
}
