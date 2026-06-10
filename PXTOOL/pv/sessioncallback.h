/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef DSVIEW_PV_SESSIONCALLBACK_H
#define DSVIEW_PV_SESSIONCALLBACK_H

#include <cstdint>
#include <string>
#include <QtGlobal>
#include <QString>
#include "interface/icallbacks.h"

namespace pv {

/**
 * Per-session proxy for ISessionCallback.
 *
 * Each SigSession tab owns one SessionCallback. When a tab is the active
 * tab, its SessionCallback is "active" and all calls are forwarded to the
 * single real ISessionCallback (MainWindow). When the tab is hidden, the
 * proxy is deactivated and all calls are silently dropped. This prevents
 * background/inactive sessions from corrupting the MainWindow UI state or
 * triggering device re-initialization while the user is on a different tab.
 */
class SessionCallback : public ISessionCallback
{
public:
    explicit SessionCallback(ISessionCallback *target);
    virtual ~SessionCallback() = default;

    /**
     * Activate or deactivate this proxy. Only the currently visible session
     * tab's proxy should be active at any given time.
     */
    void setActive(bool active) { _active = active; }
    bool isActive() const { return _active; }

    // ISessionCallback — each method forwards to _target only when active
    void session_error() override;
    void session_save() override;
    void data_updated() override;
    void update_capture() override;
    void cur_snap_samplerate_changed() override;
    void signals_changed() override;
    void receive_trigger(quint64 trigger_pos) override;
    void frame_ended() override;
    void frame_began() override;
    void show_region(uint64_t start, uint64_t end, bool keep) override;
    void show_wait_trigger() override;
    void repeat_hold(int percent) override;
    void decode_done() override;
    void receive_data_len(quint64 len) override;
    void receive_header() override;
    void trigger_message(int msg) override;
    void delay_prop_msg(QString strMsg) override;

private:
    ISessionCallback *_target;
    bool              _active;
};

} // namespace pv

#endif // DSVIEW_PV_SESSIONCALLBACK_H
