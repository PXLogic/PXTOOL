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

#include "pv/wintaskbarprogress.h"

BOOST_AUTO_TEST_CASE(taskbar_progress_value_is_clamped_to_percent_range)
{
	BOOST_CHECK_EQUAL(pv::WinTaskbarProgress::normalizedValue(-1), 0);
	BOOST_CHECK_EQUAL(pv::WinTaskbarProgress::normalizedValue(0), 0);
	BOOST_CHECK_EQUAL(pv::WinTaskbarProgress::normalizedValue(42), 42);
	BOOST_CHECK_EQUAL(pv::WinTaskbarProgress::normalizedValue(100), 100);
	BOOST_CHECK_EQUAL(pv::WinTaskbarProgress::normalizedValue(101), 100);
}
