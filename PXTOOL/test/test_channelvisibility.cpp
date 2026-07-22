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

#include <boost/test/unit_test.hpp>

#include "../pv/dock/channelvisibility.h"

BOOST_AUTO_TEST_CASE(missing_valid_channel_count_keeps_all_channels_visible)
{
    BOOST_CHECK_EQUAL(pv::dock::effective_valid_channel_limit(false, 0, 8), 8);
}

BOOST_AUTO_TEST_CASE(configured_valid_channel_count_clamps_to_channel_count)
{
    BOOST_CHECK_EQUAL(pv::dock::effective_valid_channel_limit(true, 16, 8), 8);
    BOOST_CHECK_EQUAL(pv::dock::effective_valid_channel_limit(true, 4, 8), 4);
}
