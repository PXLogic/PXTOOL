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

#include "../pv/sigsession.h"

using pv::SigSession;

BOOST_AUTO_TEST_SUITE(mso_mode)

BOOST_AUTO_TEST_CASE(mso_is_logic_capable_but_not_dso)
{
    BOOST_CHECK(SigSession::is_logic_capable_mode(MSO));
    BOOST_CHECK(!SigSession::is_dso_mode(MSO));
}

BOOST_AUTO_TEST_CASE(mso_accepts_logic_and_analog_only)
{
    BOOST_CHECK(SigSession::channel_type_visible_in_mode(MSO, SR_CHANNEL_LOGIC));
    BOOST_CHECK(SigSession::channel_type_visible_in_mode(MSO, SR_CHANNEL_ANALOG));
    BOOST_CHECK(!SigSession::channel_type_visible_in_mode(MSO, SR_CHANNEL_DSO));
}

BOOST_AUTO_TEST_SUITE_END()
