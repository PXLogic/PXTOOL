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

#include "channeltint.h"

#include <libsigrok.h>

namespace pv {
namespace view {

QColor channel_tint_color(QColor trace_colour, QColor back, bool enabled, int rows)
{
    if (!enabled || rows <= 0 || !trace_colour.isValid())
        return QColor();

    const bool dark_theme = back.lightness() < 128;
    trace_colour.setAlpha(dark_theme ? 38 : 24);
    return trace_colour;
}

QRect channel_tint_rect(int viewport_width, int viewport_height, int center_y,
                        int total_height, int margin)
{
    if (viewport_width <= 0 || viewport_height <= 0 || total_height <= 0)
        return QRect();

    const int half_height = total_height / 2;
    const int top = center_y - half_height - margin;
    const int height = total_height + 2 * margin;
    const QRect row_rect(0, top, viewport_width, height);
    const QRect viewport_rect(0, 0, viewport_width, viewport_height);
    return row_rect.intersected(viewport_rect);
}

bool channel_tint_accepts_signal_type(int signal_type)
{
    switch (signal_type) {
    case SR_CHANNEL_LOGIC:
    case SR_CHANNEL_DECODER:
    case SR_CHANNEL_GROUP:
    case SR_CHANNEL_ANALOG:
    case SR_CHANNEL_MATH:
        return true;
    default:
        return false;
    }
}

} // namespace view
} // namespace pv
