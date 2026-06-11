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

#include "../pv/dock/logsearch.h"

using pv::dock::LogSearchMatch;
using pv::dock::findLogSearchMatches;

BOOST_AUTO_TEST_SUITE(LogSearchTest)

BOOST_AUTO_TEST_CASE(simple_matching_is_case_insensitive)
{
	const QVector<LogSearchMatch> matches =
		findLogSearchMatches("Error ERROR error", "error", false);

	BOOST_REQUIRE_EQUAL(matches.size(), 3);
	BOOST_CHECK_EQUAL(matches[0].position, 0);
	BOOST_CHECK_EQUAL(matches[0].length, 5);
	BOOST_CHECK_EQUAL(matches[1].position, 6);
	BOOST_CHECK_EQUAL(matches[1].length, 5);
	BOOST_CHECK_EQUAL(matches[2].position, 12);
	BOOST_CHECK_EQUAL(matches[2].length, 5);
}

BOOST_AUTO_TEST_CASE(exact_matching_is_case_sensitive)
{
	const QVector<LogSearchMatch> matches =
		findLogSearchMatches("Error ERROR error", "Error", true);

	BOOST_REQUIRE_EQUAL(matches.size(), 1);
	BOOST_CHECK_EQUAL(matches[0].position, 0);
	BOOST_CHECK_EQUAL(matches[0].length, 5);
}

BOOST_AUTO_TEST_CASE(multiple_matches_are_returned_in_order)
{
	const QVector<LogSearchMatch> matches =
		findLogSearchMatches("abc abc abc", "abc", false);

	BOOST_REQUIRE_EQUAL(matches.size(), 3);
	BOOST_CHECK_EQUAL(matches[0].position, 0);
	BOOST_CHECK_EQUAL(matches[1].position, 4);
	BOOST_CHECK_EQUAL(matches[2].position, 8);
}

BOOST_AUTO_TEST_CASE(empty_query_returns_no_matches)
{
	const QVector<LogSearchMatch> matches =
		findLogSearchMatches("anything", "", false);

	BOOST_CHECK(matches.empty());
}

BOOST_AUTO_TEST_SUITE_END()
