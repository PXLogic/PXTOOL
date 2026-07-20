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

static uint64_t test_samplerate;

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
    sdi.priv = NULL;

    return &sdi;
}

struct sr_dev_inst *make_test_sdi_with_samplerate(uint64_t samplerate)
{
    struct sr_dev_inst *sdi = make_test_sdi();

    test_samplerate = samplerate;
    sdi->priv = &test_samplerate;
    return sdi;
}

uint64_t test_sdi_samplerate_get(const struct sr_dev_inst *sdi)
{
    if (!sdi || !sdi->priv)
        return 0;

    return *(const uint64_t *)sdi->priv;
}
