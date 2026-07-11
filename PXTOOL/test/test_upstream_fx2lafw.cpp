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

extern "C" {
#include "libsigrok-internal.h"
#ifdef HAVE_UPSTREAM_FX2LAFW
#include "hardware/upstream-fx2lafw/fx2lafw.h"
#endif
}

BOOST_AUTO_TEST_SUITE(upstream_fx2lafw)

BOOST_AUTO_TEST_CASE(profile_lookup_matches_saleae_logic)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");

    BOOST_REQUIRE(profile != nullptr);
    BOOST_CHECK_EQUAL(profile->vendor, "Saleae");
    BOOST_CHECK_EQUAL(profile->model, "Logic");
    BOOST_CHECK_EQUAL(fx2lafw_profile_channel_count(profile), 8);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(profile_lookup_supports_16_channel_sigrok_fx2)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x1d50, 0x608d, "sigrok", "fx2lafw");

    BOOST_REQUIRE(profile != nullptr);
    BOOST_CHECK_EQUAL(profile->vendor, "sigrok");
    BOOST_CHECK_EQUAL(profile->model, "FX2 LA (16ch)");
    BOOST_CHECK_EQUAL(fx2lafw_profile_channel_count(profile), 16);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(driver_exposes_dsview_supported_configs)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    GVariant *options = nullptr;
    BOOST_REQUIRE_EQUAL(fx2lafw_driver_info.config_list(
        SR_CONF_DEVICE_OPTIONS, &options, nullptr, nullptr), SR_OK);
    BOOST_REQUIRE(options != nullptr);

    bool has_samplerate = false;
    bool has_limit_samples = false;
    bool has_valid_channels = false;
    bool has_probe_enable = false;
    gsize option_count = 0;
    const int32_t *items = static_cast<const int32_t *>(
        g_variant_get_fixed_array(options, &option_count, sizeof(int32_t)));
    for (gsize i = 0; i < option_count; i++) {
        has_samplerate |= items[i] == SR_CONF_SAMPLERATE;
        has_limit_samples |= items[i] == SR_CONF_LIMIT_SAMPLES;
        has_valid_channels |= items[i] == SR_CONF_VLD_CH_NUM;
        has_probe_enable |= items[i] == SR_CONF_PROBE_EN;
    }
    g_variant_unref(options);

    BOOST_CHECK(has_samplerate);
    BOOST_CHECK(has_limit_samples);
    BOOST_CHECK(has_valid_channels);
    BOOST_CHECK(has_probe_enable);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_SUITE_END()
