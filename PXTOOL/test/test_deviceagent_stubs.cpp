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

#include <cstdarg>

#include "log/xlog.h"

#include "libsigrok.h"

xlog_writer *dsv_log = nullptr;

void dsv_ui_log(int, const char *, ...)
{
}

extern "C" {

int ds_enable_device_channel(const sr_channel *, gboolean)
{
    return SR_ERR_NA;
}

int ds_enable_device_channel_index(int, gboolean)
{
    return SR_ERR_NA;
}

int ds_set_device_channel_name(int, const char *)
{
    return SR_ERR_NA;
}

int ds_get_actived_device_config(const sr_channel *, const sr_channel_group *,
                                 int, GVariant **data)
{
    if (data)
        *data = nullptr;
    return SR_ERR_NA;
}

const GSList *ds_get_actived_device_mode_list(void)
{
    return nullptr;
}

int ds_trigger_is_enabled(void)
{
    return 0;
}

int ds_start_collect(void)
{
    return SR_ERR_NA;
}

int ds_stop_collect(void)
{
    return SR_ERR_NA;
}

int ds_get_actived_device_info(ds_device_full_info *)
{
    return SR_ERR_NA;
}

int ds_release_actived_device(void)
{
    return SR_OK;
}

int ds_channel_is_enabled(void)
{
    return 0;
}

int ds_get_actived_device_mode(void)
{
    return UNKNOWN_DSL_MODE;
}

int ds_get_actived_device_status(sr_status *, gboolean)
{
    return SR_ERR_NA;
}

GSList *ds_get_actived_device_channels(void)
{
    return nullptr;
}

int ds_actived_device_supports_config_key(int)
{
    return 0;
}

int ds_actived_device_supports_capability(int)
{
    return 0;
}

int ds_get_actived_device_init_status(int *)
{
    return SR_ERR_NA;
}

int ds_get_actived_device_config_list(const sr_channel_group *, int,
                                      GVariant **data)
{
    if (data)
        *data = nullptr;
    return SR_ERR_NA;
}

const sr_config_info *ds_get_actived_device_config_info(int)
{
    return nullptr;
}

int ds_set_actived_device_config(const sr_channel *, const sr_channel_group *,
                                 int, GVariant *)
{
    return SR_ERR_NA;
}

sr_config *ds_new_config(int key, GVariant *data)
{
    (void)key;
    (void)data;
    return nullptr;
}

void ds_free_config(sr_config *src)
{
    (void)src;
}

int ds_is_collecting(void)
{
    return 0;
}

const sr_config_info *sr_config_info_get(int)
{
    return nullptr;
}

}
