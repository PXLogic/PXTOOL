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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <boost/test/unit_test.hpp>
#include <type_traits>

#include "pv/deviceagent.h"
#include "pv/deviceagentcustomconfig.h"

extern "C" {
#include "libsigrok-internal.h"
}

namespace {

bool has_mode(const GSList *modes, int wanted_mode)
{
    for (const GSList *entry = modes; entry; entry = entry->next) {
        const sr_dev_mode *mode = static_cast<const sr_dev_mode *>(entry->data);
        if (mode && mode->mode == wanted_mode)
            return true;
    }

    return false;
}

sr_dev_inst *make_imported_device(sr_channeltype channel_type)
{
    sr_dev_inst *sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE,
                                       "DSView", "Imported", "0.1");
    if (sdi)
        sr_channel_new(sdi, 0, channel_type, TRUE, "CH0");
    return sdi;
}

sr_dev_inst *make_mixed_imported_device()
{
    sr_dev_inst *sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE,
                                       "DSView", "Imported", "0.1");
    if (sdi) {
        sr_channel_new(sdi, 0, SR_CHANNEL_LOGIC, TRUE, "D0");
        sr_channel_new(sdi, 1, SR_CHANNEL_ANALOG, TRUE, "A0");
    }
    return sdi;
}

} // namespace

BOOST_AUTO_TEST_SUITE(deviceagent_capability)

BOOST_AUTO_TEST_CASE(capability_methods_are_available)
{
    static_assert(std::is_same<bool (DeviceAgent::*)(int),
        decltype(&DeviceAgent::supports_config)>::value,
        "supports_config must accept a config key and return bool");

    static_assert(std::is_same<bool (DeviceAgent::*)(int),
        decltype(&DeviceAgent::supports_capability)>::value,
        "supports_capability must accept a capability id and return bool");

    static_assert(std::is_same<bool (DeviceAgent::*)(),
        decltype(&DeviceAgent::supports_waveform)>::value,
        "supports_waveform must return bool");

    static_assert(std::is_same<bool (DeviceAgent::*)(),
        decltype(&DeviceAgent::supports_stream)>::value,
        "supports_stream must return bool");

    static_assert(std::is_same<bool (DeviceAgent::*)(),
        decltype(&DeviceAgent::supports_advanced_trigger)>::value,
        "supports_advanced_trigger must return bool");
}

BOOST_AUTO_TEST_CASE(custom_samplerate_list_exposes_single_imported_value)
{
    GVariant *dict = custom_samplerate_list_variant(1000000);
    BOOST_REQUIRE(dict != nullptr);

    GVariant *rates = g_variant_lookup_value(dict, "samplerates", G_VARIANT_TYPE("at"));
    BOOST_REQUIRE(rates != nullptr);

    gsize count = 0;
    const uint64_t *values = static_cast<const uint64_t *>(
        g_variant_get_fixed_array(rates, &count, sizeof(uint64_t)));
    BOOST_REQUIRE_EQUAL(count, 1U);
    BOOST_CHECK_EQUAL(values[0], 1000000ULL);

    g_variant_unref(rates);
    g_variant_unref(dict);
}

BOOST_AUTO_TEST_CASE(demo_operation_mode_defaults_to_random_without_pattern_config)
{
    sr_dev_inst *sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE,
        "DSView", "Custom Demo", "0.1");
    BOOST_REQUIRE(sdi != nullptr);

    DeviceAgent agent;
    agent.bind_custom_device(sdi,
                             DEV_TYPE_DEMO,
                             LOGIC,
                             QStringLiteral("Custom Demo"),
                             QString(),
                             QStringLiteral("custom"),
                             1000000,
                             1024);

    BOOST_CHECK(agent.is_demo());
    BOOST_CHECK(!agent.supports_config(SR_CONF_PATTERN_MODE));
    BOOST_CHECK_EQUAL(agent.get_demo_operation_mode().toStdString(), "random");

    agent.release();
}

BOOST_AUTO_TEST_CASE(imported_device_modes_follow_channel_capabilities)
{
    DeviceAgent agent;

    sr_dev_inst *logic_device = make_imported_device(SR_CHANNEL_LOGIC);
    BOOST_REQUIRE(logic_device != nullptr);
    agent.bind_custom_device(logic_device, DEV_TYPE_DEMO, LOGIC,
                             QStringLiteral("Logic import"), QString(),
                             QStringLiteral("import"), 1'000'000, 1024);
    BOOST_CHECK(has_mode(agent.get_device_mode_list(), LOGIC));
    BOOST_CHECK(!has_mode(agent.get_device_mode_list(), ANALOG));
    BOOST_CHECK(!has_mode(agent.get_device_mode_list(), MSO));

    sr_dev_inst *analog_device = make_imported_device(SR_CHANNEL_ANALOG);
    BOOST_REQUIRE(analog_device != nullptr);
    agent.bind_custom_device(analog_device, DEV_TYPE_DEMO, LOGIC,
                             QStringLiteral("Analog import"), QString(),
                             QStringLiteral("import"), 1'000'000, 1024);
    BOOST_CHECK(has_mode(agent.get_device_mode_list(), ANALOG));
    BOOST_CHECK(!has_mode(agent.get_device_mode_list(), LOGIC));
    BOOST_CHECK(!has_mode(agent.get_device_mode_list(), MSO));

    sr_dev_inst *mixed_device = make_mixed_imported_device();
    BOOST_REQUIRE(mixed_device != nullptr);
    agent.bind_custom_device(mixed_device, DEV_TYPE_DEMO, MSO,
                             QStringLiteral("Mixed import"), QString(),
                             QStringLiteral("import"), 1'000'000, 1024);
    const GSList *mixed_modes = agent.get_device_mode_list();
    BOOST_REQUIRE(mixed_modes != nullptr);
    const sr_dev_mode *first_mode =
        static_cast<const sr_dev_mode *>(mixed_modes->data);
    BOOST_REQUIRE(first_mode != nullptr);
    BOOST_CHECK_EQUAL(first_mode->mode, MSO);
    BOOST_CHECK(has_mode(mixed_modes, MSO));
    BOOST_CHECK(has_mode(mixed_modes, LOGIC));
    BOOST_CHECK(has_mode(mixed_modes, ANALOG));

    agent.release();
}

BOOST_AUTO_TEST_CASE(custom_device_mode_updates_through_device_mode_config)
{
    sr_dev_inst *mixed_device = make_mixed_imported_device();
    BOOST_REQUIRE(mixed_device != nullptr);

    DeviceAgent agent;
    agent.bind_custom_device(mixed_device, DEV_TYPE_DEMO, LOGIC,
                             QStringLiteral("Mixed import"), QString(),
                             QStringLiteral("import"), 1'000'000, 1024);

    GVariant *mode = g_variant_ref_sink(g_variant_new_int16(MSO));
    BOOST_REQUIRE(agent.set_config(SR_CONF_DEVICE_MODE, mode));
    g_variant_unref(mode);
    BOOST_CHECK_EQUAL(agent.get_work_mode(), MSO);

    agent.release();
}

BOOST_AUTO_TEST_SUITE_END()
