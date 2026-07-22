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

#ifndef DSVIEW_PV_DOCK_CHANNELVISIBILITY_H
#define DSVIEW_PV_DOCK_CHANNELVISIBILITY_H

#include <algorithm>

namespace pv {
namespace dock {

inline int effective_valid_channel_limit(bool hasConfiguredLimit,
                                         int configuredLimit,
                                         int channelCount)
{
    if (channelCount <= 0)
        return 0;
    if (hasConfiguredLimit && configuredLimit > 0)
        return std::min(configuredLimit, channelCount);
    return channelCount;
}

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_DOCK_CHANNELVISIBILITY_H
