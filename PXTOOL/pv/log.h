/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef _DSV_LOG_H_
#define _DSV_LOG_H_

#include <log/xlog.h>
#include <QString>
#include <QList>
#include <assert.h>

extern xlog_writer *dsv_log;

void dsv_log_init();
void dsv_log_uninit();

xlog_context* dsv_log_context();
void dsv_log_level(int l);

void dsv_log_enalbe_logfile(bool append);
void dsv_remove_log_file();
void dsv_clear_log_file();
void dsv_set_log_file_enable(bool flag);

QString get_dsv_log_path();

// UI log buffer — thread-safe, drained by LogDock every 100 ms
struct UiLogEntry { int level; QString text; };
void dsv_ui_log(int level, const char *fmt, ...);
void dsv_set_ui_log_level(int level);
QList<UiLogEntry> dsv_take_ui_logs();

#define LOG_PREFIX ""
#define dsv_err(fmt, args...)    do { xlog_err(dsv_log,    LOG_PREFIX fmt, ##args); dsv_ui_log(XLOG_LEVEL_ERR,    LOG_PREFIX fmt, ##args); } while(0)
#define dsv_warn(fmt, args...)   do { xlog_warn(dsv_log,   LOG_PREFIX fmt, ##args); dsv_ui_log(XLOG_LEVEL_WARN,   LOG_PREFIX fmt, ##args); } while(0)
#define dsv_info(fmt, args...)   do { xlog_info(dsv_log,   LOG_PREFIX fmt, ##args); dsv_ui_log(XLOG_LEVEL_INFO,   LOG_PREFIX fmt, ##args); } while(0)
#define dsv_dbg(fmt, args...)    do { xlog_dbg(dsv_log,    LOG_PREFIX fmt, ##args); dsv_ui_log(XLOG_LEVEL_DBG,    LOG_PREFIX fmt, ##args); } while(0)
#define dsv_detail(fmt, args...) do { xlog_detail(dsv_log, LOG_PREFIX fmt, ##args); dsv_ui_log(XLOG_LEVEL_DETAIL, LOG_PREFIX fmt, ##args); } while(0)

#endif