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

SR_PRIV struct sr_config *sr_config_new(int key, GVariant *data)
{
    struct sr_config *src;

    if (!data)
        return NULL;

    src = g_malloc0(sizeof(struct sr_config));
    src->key = key;
    src->data = g_variant_ref_sink(data);
    return src;
}

SR_PRIV void sr_config_free(struct sr_config *src)
{
    if (!src)
        return;

    if (src->data)
        g_variant_unref(src->data);
    g_free(src);
}

SR_PRIV int sr_config_get(const struct sr_dev_driver *driver,
                         const struct sr_dev_inst *sdi,
                         const struct sr_channel *ch,
                         const struct sr_channel_group *cg,
                         int key, GVariant **data)
{
    (void)driver;
    (void)sdi;
    (void)ch;
    (void)cg;
    (void)key;
    (void)data;
    return SR_ERR_ARG;
}

SR_PRIV int std_session_send_df_header(const struct sr_dev_inst *sdi,
                                       const char *log_prefix)
{
    (void)sdi;
    (void)log_prefix;
    return SR_OK;
}
