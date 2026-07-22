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

#include "../pv/view/sampleconversion.h"

BOOST_AUTO_TEST_SUITE(ViewSampleConversionTest)

BOOST_AUTO_TEST_CASE(pixel_to_index_applies_horizontal_trigger_offset)
{
    const uint64_t sampleRate = 1000000;
    const double scale = 0.000001;
    const int64_t offset = 400;
    const double trigHoff = 7;
    const double pixel = 25;

    const uint64_t index =
        pv::view::pixel_to_sample_index(pixel, sampleRate, scale, offset, trigHoff);

    BOOST_CHECK_EQUAL(index, 418);
}

BOOST_AUTO_TEST_CASE(index_to_pixel_applies_horizontal_trigger_offset)
{
    const uint64_t sampleRate = 1000000;
    const double scale = 0.000001;
    const int64_t offset = 400;
    const double trigHoff = 7;
    const uint64_t index = 418;

    const double pixel =
        pv::view::sample_index_to_pixel(index, sampleRate, scale, offset, trigHoff);

    BOOST_CHECK_CLOSE(pixel, 25.0, 0.000001);
}

BOOST_AUTO_TEST_CASE(edge_nav_scrolls_only_when_target_is_near_view_edges)
{
    BOOST_CHECK(pv::view::edge_nav_target_requires_scroll(-1, 1000));
    BOOST_CHECK(pv::view::edge_nav_target_requires_scroll(50, 1000));
    BOOST_CHECK(!pv::view::edge_nav_target_requires_scroll(500, 1000));
    BOOST_CHECK(pv::view::edge_nav_target_requires_scroll(950, 1000));
    BOOST_CHECK(pv::view::edge_nav_target_requires_scroll(1001, 1000));
}

BOOST_AUTO_TEST_CASE(next_edge_from_anchor_compares_against_anchor_sample)
{
    BOOST_CHECK_EQUAL(
        pv::view::edge_nav_next_reference_sample_index(true, 7, 8), 7);
    BOOST_CHECK_EQUAL(
        pv::view::edge_nav_next_reference_sample_index(false, 7, 8), 8);
}

BOOST_AUTO_TEST_SUITE_END()
