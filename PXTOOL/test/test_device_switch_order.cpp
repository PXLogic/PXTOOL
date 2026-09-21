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

// SigSession::set_device() broadcasts DSV_MSG_CURRENT_DEVICE_CHANGED
// synchronously, and MainWindow's handler drives decoder work through the
// protocol dock (del_all_protocol, and load_demo_decoder_config ->
// StoreSession::load_decoders(protocol_widget(), ...)). load_decoders creates
// each decoder through the dock's session but applies "label" / "view_index" to
// StoreSession's session by positional index, so the two must be the same
// object. Binding the dock after set_device left it on the outgoing session
// while MainWindow::_session was already the incoming one, which sent the
// decoders to the wrong session and made the index lookup run against an empty
// _decode_traces -> null view::Trace -> SIGSEGV on the virtual set_name().
// Reproduced by clicking the "+" button to add a session tab on a demo device.
BOOST_AUTO_TEST_CASE(binds_protocol_dock_before_device_init)
{
    const std::string source = mainwindow_source();
    const std::size_t function = source.find(
        "void MainWindow::switch_to_session_for_handle");
    BOOST_REQUIRE(function != std::string::npos);

    const std::size_t bind_protocol_dock = source.find(
        "_sidebar_widget->protocol_widget()->setSession(_session)", function);
    const std::size_t bind_device = source.find("_session->set_device(handle)", function);

    BOOST_REQUIRE(bind_protocol_dock != std::string::npos);
    BOOST_REQUIRE(bind_device != std::string::npos);
    BOOST_CHECK(bind_protocol_dock < bind_device);
}

BOOST_AUTO_TEST_SUITE_END()
