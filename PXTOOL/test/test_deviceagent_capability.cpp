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

BOOST_AUTO_TEST_SUITE_END()
