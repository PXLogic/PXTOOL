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

#include "sessioncallback.h"

namespace pv {

SessionCallback::SessionCallback(ISessionCallback *target)
    : _target(target)
    , _active(false)
{
}

void SessionCallback::session_error()
{
    if (_active) _target->session_error();
}

void SessionCallback::session_save()
{
    if (_active) _target->session_save();
}

void SessionCallback::data_updated()
{
    if (_active) _target->data_updated();
}

void SessionCallback::update_capture()
{
    if (_active) _target->update_capture();
}

void SessionCallback::cur_snap_samplerate_changed()
{
    if (_active) _target->cur_snap_samplerate_changed();
}

void SessionCallback::signals_changed()
{
    if (_active) _target->signals_changed();
}

void SessionCallback::receive_trigger(quint64 trigger_pos)
{
    if (_active) _target->receive_trigger(trigger_pos);
}

void SessionCallback::frame_ended()
{
    if (_active) _target->frame_ended();
}

void SessionCallback::frame_began()
{
    if (_active) _target->frame_began();
}

void SessionCallback::show_region(uint64_t start, uint64_t end, bool keep)
{
    if (_active) _target->show_region(start, end, keep);
}

void SessionCallback::show_wait_trigger()
{
    if (_active) _target->show_wait_trigger();
}

void SessionCallback::repeat_hold(int percent)
{
    if (_active) _target->repeat_hold(percent);
}

void SessionCallback::decode_done()
{
    if (_active) _target->decode_done();
}

void SessionCallback::receive_data_len(quint64 len)
{
    if (_active) _target->receive_data_len(len);
}

void SessionCallback::receive_header()
{
    if (_active) _target->receive_header();
}

void SessionCallback::trigger_message(int msg)
{
    if (_active) _target->trigger_message(msg);
}

void SessionCallback::delay_prop_msg(QString strMsg)
{
    if (_active) _target->delay_prop_msg(strMsg);
}

} // namespace pv
