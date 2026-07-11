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

#ifdef HAVE_UPSTREAM_FX2LAFW
extern "C" {
char DS_RES_PATH[500];

void ds_set_last_error(int error)
{
    (void)error;
}
}
#endif

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

BOOST_AUTO_TEST_CASE(scan_without_options_does_not_crash)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    sr_context ctx = {};

    BOOST_REQUIRE_EQUAL(fx2lafw_driver_info.init(&ctx), SR_OK);
    GSList *devices = fx2lafw_driver_info.scan(nullptr);
    g_slist_free_full(devices, (GDestroyNotify)sr_dev_inst_free);
    BOOST_CHECK_EQUAL(fx2lafw_driver_info.cleanup(), SR_OK);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(firmware_path_requires_resource_directory)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    DS_RES_PATH[0] = '\0';
    char *path = nullptr;
    BOOST_CHECK_EQUAL(fx2lafw_firmware_path(profile, &path),
        SR_ERR_FIRMWARE_NOT_EXIST);
    BOOST_CHECK(path == nullptr);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(firmware_path_joins_resource_directory)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    g_strlcpy(DS_RES_PATH, "/tmp/dsview-fw", sizeof(DS_RES_PATH));
    char *path = nullptr;
    BOOST_REQUIRE_EQUAL(fx2lafw_firmware_path(profile, &path), SR_OK);
    BOOST_REQUIRE(path != nullptr);
    BOOST_CHECK_EQUAL(path, "/tmp/dsview-fw/fx2lafw-saleae-logic.fw");
    g_free(path);

    g_strlcpy(DS_RES_PATH, "/tmp/dsview-fw/", sizeof(DS_RES_PATH));
    path = nullptr;
    BOOST_REQUIRE_EQUAL(fx2lafw_firmware_path(profile, &path), SR_OK);
    BOOST_REQUIRE(path != nullptr);
    BOOST_CHECK_EQUAL(path, "/tmp/dsview-fw/fx2lafw-saleae-logic.fw");
    g_free(path);
    DS_RES_PATH[0] = '\0';
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(firmware_state_uses_sigrok_fx2lafw_strings)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    BOOST_CHECK_EQUAL(fx2lafw_has_firmware("sigrok", "fx2lafw"), TRUE);
    BOOST_CHECK_EQUAL(fx2lafw_has_firmware("Saleae", "Logic"), FALSE);
    BOOST_CHECK_EQUAL(fx2lafw_has_firmware(nullptr, "fx2lafw"), FALSE);
    BOOST_CHECK_EQUAL(fx2lafw_has_firmware("sigrok", nullptr), FALSE);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(driver_exposes_open_lifecycle)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    BOOST_REQUIRE(fx2lafw_driver_info.dev_open != nullptr);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(close_without_open_returns_error)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE,
        profile->vendor, profile->model, nullptr);
    BOOST_REQUIRE(sdi != nullptr);
    sdi->driver = &fx2lafw_driver_info;
    sdi->conn = sr_usb_dev_inst_new(1, 2);

    BOOST_REQUIRE(fx2lafw_driver_info.dev_close != nullptr);
    BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_close(sdi), SR_ERR_BUG);
    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_SUITE_END()
