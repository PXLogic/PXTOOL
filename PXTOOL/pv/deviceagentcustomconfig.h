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
 * along with this program.  If not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef DSVIEW_PV_DEVICEAGENTCUSTOMCONFIG_H
#define DSVIEW_PV_DEVICEAGENTCUSTOMCONFIG_H

#include <stdint.h>

#include <glib.h>

inline GVariant *custom_samplerate_list_variant(uint64_t samplerate)
{
    if (samplerate == 0)
        return nullptr;

    GVariantBuilder rates;
    g_variant_builder_init(&rates, G_VARIANT_TYPE("at"));
    g_variant_builder_add(&rates, "t", samplerate);

    GVariantBuilder dict;
    g_variant_builder_init(&dict, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&dict, "{sv}", "samplerates",
                          g_variant_builder_end(&rates));
    return g_variant_ref_sink(g_variant_builder_end(&dict));
}

#endif // DSVIEW_PV_DEVICEAGENTCUSTOMCONFIG_H
