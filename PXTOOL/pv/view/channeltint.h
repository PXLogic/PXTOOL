/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DSVIEW_PV_VIEW_CHANNELTINT_H
#define DSVIEW_PV_VIEW_CHANNELTINT_H

#include <QColor>
#include <QRect>

namespace pv {
namespace view {

QColor channel_tint_color(QColor trace_colour, QColor back, bool enabled, int rows);
QRect channel_tint_rect(int viewport_width, int viewport_height, int center_y,
                        int total_height, int margin);
bool channel_tint_accepts_signal_type(int signal_type);

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_CHANNELTINT_H
