/*
 * This file is part of the PXTOOL project.
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

#ifndef PXTOOL_PV_VIEW_SAMPLECONVERSION_H
#define PXTOOL_PV_VIEW_SAMPLECONVERSION_H

#include <cmath>
#include <cstdint>

namespace pv {
namespace view {

inline double sample_index_to_pixel(uint64_t index, double sample_rate,
    double scale, int64_t offset, double trig_hoff)
{
    const double samples_per_pixel = sample_rate * scale;
    return index / samples_per_pixel - offset + trig_hoff / samples_per_pixel;
}

inline double pixel_to_sample_position(double pixel, double sample_rate,
    double scale, int64_t offset, double trig_hoff)
{
    const double samples_per_pixel = sample_rate * scale;
    return (pixel + offset) * samples_per_pixel - trig_hoff;
}

inline uint64_t pixel_to_sample_index(double pixel, double sample_rate,
    double scale, int64_t offset, double trig_hoff)
{
    return (uint64_t)std::round(
        pixel_to_sample_position(pixel, sample_rate, scale, offset, trig_hoff));
}

inline bool edge_nav_target_requires_scroll(double target_pixel, int view_width)
{
    if (view_width <= 0)
        return true;

    const double margin = view_width * 0.10;
    return target_pixel < margin || target_pixel > view_width - margin;
}

inline uint64_t edge_nav_next_reference_sample_index(bool has_anchor,
    uint64_t anchor_index, uint64_t search_index)
{
    return has_anchor ? anchor_index : search_index;
}

} // namespace view
} // namespace pv

#endif // PXTOOL_PV_VIEW_SAMPLECONVERSION_H
