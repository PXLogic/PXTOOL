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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/test/unit_test.hpp>

#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string mainwindow_source()
{
    std::ifstream stream(std::string(DSVIEW_SOURCE_DIR) +
                         "/PXTOOL/pv/mainwindow.cpp");
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

} // namespace

BOOST_AUTO_TEST_SUITE(device_switch_order)

BOOST_AUTO_TEST_CASE(initializes_device_before_rebinding_ui)
{
    const std::string source = mainwindow_source();
    const std::size_t function = source.find(
        "void MainWindow::switch_to_session_for_handle");
    BOOST_REQUIRE(function != std::string::npos);

    const std::size_t bind_device = source.find("_session->set_device(handle)", function);
    const std::size_t bind_sampling_bar = source.find(
        "_sampling_bar->setSession(_session)", function);
    const std::size_t bind_sidebar = source.find(
        "_sidebar_widget->setSession(_session)", function);
    const std::size_t show_view = source.find(
        "_session_stack->setCurrentWidget(_view)", function);

    BOOST_REQUIRE(bind_device != std::string::npos);
    BOOST_REQUIRE(bind_sampling_bar != std::string::npos);
    BOOST_REQUIRE(bind_sidebar != std::string::npos);
    BOOST_REQUIRE(show_view != std::string::npos);
    BOOST_CHECK(bind_device < bind_sampling_bar);
    BOOST_CHECK(bind_device < bind_sidebar);
    BOOST_CHECK(bind_device < show_view);
}

BOOST_AUTO_TEST_SUITE_END()
