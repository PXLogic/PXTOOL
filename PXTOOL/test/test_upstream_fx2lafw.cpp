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
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "test_datafeed_stub.h"

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

int sr_session_source_add(gintptr poll_object, int events, int timeout,
    sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi)
{
    (void)poll_object;
    (void)events;
    (void)timeout;
    (void)cb;
    (void)sdi;
    return SR_OK;
}

int sr_session_source_remove(gintptr poll_object)
{
    (void)poll_object;
    return SR_OK;
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
    BOOST_CHECK_EQUAL(path, "/tmp/dsview-fw/fx2lafw/fx2lafw-saleae-logic.fw");
    g_free(path);

    g_strlcpy(DS_RES_PATH, "/tmp/dsview-fw/", sizeof(DS_RES_PATH));
    path = nullptr;
    BOOST_REQUIRE_EQUAL(fx2lafw_firmware_path(profile, &path), SR_OK);
    BOOST_REQUIRE(path != nullptr);
    BOOST_CHECK_EQUAL(path, "/tmp/dsview-fw/fx2lafw/fx2lafw-saleae-logic.fw");
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

BOOST_AUTO_TEST_CASE(close_without_open_is_idempotent)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = sr_dev_inst_new(LOGIC, SR_ST_INITIALIZING,
        profile->vendor, profile->model, nullptr);
    BOOST_REQUIRE(sdi != nullptr);
    sdi->driver = &fx2lafw_driver_info;
    sdi->conn = sr_usb_dev_inst_new(1, 2);

	BOOST_REQUIRE(fx2lafw_driver_info.dev_close != nullptr);
	BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_close(sdi), SR_OK);
	BOOST_CHECK_EQUAL(sdi->status, SR_ST_INACTIVE);
    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(firmware_manifest_matches_profiles)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    std::ifstream manifest(FX2LAFW_MANIFEST_PATH);
    BOOST_REQUIRE_MESSAGE(manifest.good(), FX2LAFW_MANIFEST_PATH);

    std::vector<std::string> manifest_files;
    std::set<std::string> unique_manifest_files;
    std::string line;
    while (std::getline(manifest, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        manifest_files.push_back(line);
        BOOST_CHECK_MESSAGE(unique_manifest_files.insert(line).second,
            "duplicate firmware manifest entry: " << line);
    }

    std::vector<std::string> profile_files;
    std::set<std::string> unique_profile_files;
    for (size_t i = 0; i < fx2lafw_profile_count(); i++) {
        const fx2lafw_profile *profile = fx2lafw_profile_get(i);
        BOOST_REQUIRE(profile != nullptr);
        BOOST_REQUIRE(profile->firmware != nullptr);
        profile_files.push_back(profile->firmware);
        BOOST_CHECK_MESSAGE(unique_profile_files.insert(profile->firmware).second,
            "duplicate fx2lafw profile firmware: " << profile->firmware);
    }

    BOOST_CHECK_EQUAL(manifest_files.size(), profile_files.size());
    BOOST_CHECK_EQUAL(unique_manifest_files.size(), manifest_files.size());
    BOOST_CHECK_EQUAL(unique_profile_files.size(), profile_files.size());
    BOOST_CHECK_EQUAL_COLLECTIONS(
        unique_profile_files.begin(), unique_profile_files.end(),
        unique_manifest_files.begin(), unique_manifest_files.end());
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(firmware_manifest_documents_all_profiles)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    BOOST_CHECK_EQUAL(fx2lafw_profile_count(), 10U);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_helpers_choose_sample_width_from_enabled_channels)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x1d50, 0x608d, "sigrok", "fx2lafw");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(
        profile, 1, 2, SR_ST_ACTIVE, TRUE, 0);
    BOOST_REQUIRE(sdi != nullptr);

    BOOST_CHECK_EQUAL(fx2lafw_enabled_channel_mask(sdi), 0xffff);
    BOOST_CHECK_EQUAL(fx2lafw_sample_wide_for_channels(sdi), TRUE);

    for (GSList *l = sdi->channels; l; l = l->next) {
        struct sr_channel *channel = static_cast<struct sr_channel *>(l->data);
        if (channel->index > 7)
            channel->enabled = FALSE;
    }

    BOOST_CHECK_EQUAL(fx2lafw_enabled_channel_mask(sdi), 0x00ff);
    BOOST_CHECK_EQUAL(fx2lafw_sample_wide_for_channels(sdi), FALSE);

    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_start_command_uses_upstream_clock_rules)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    fx2lafw_start_command command = {};

    BOOST_REQUIRE_EQUAL(fx2lafw_build_start_command(SR_MHZ(24), FALSE, &command), SR_OK);
    BOOST_CHECK_EQUAL(command.flags, FX2LAFW_CMD_START_FLAGS_CLK_48MHZ |
        FX2LAFW_CMD_START_FLAGS_SAMPLE_8BIT);
    BOOST_CHECK_EQUAL(command.sample_delay_h, 0);
    BOOST_CHECK_EQUAL(command.sample_delay_l, 1);

    command = {};
    BOOST_REQUIRE_EQUAL(fx2lafw_build_start_command(SR_MHZ(48), FALSE, &command), SR_OK);
    BOOST_CHECK_EQUAL(command.flags, FX2LAFW_CMD_START_FLAGS_CLK_48MHZ |
        FX2LAFW_CMD_START_FLAGS_SAMPLE_8BIT);
    BOOST_CHECK_EQUAL(command.sample_delay_h, 0);
    BOOST_CHECK_EQUAL(command.sample_delay_l, 0);

    command = {};
    BOOST_REQUIRE_EQUAL(fx2lafw_build_start_command(SR_MHZ(12), TRUE, &command), SR_OK);
    BOOST_CHECK_EQUAL(command.flags, FX2LAFW_CMD_START_FLAGS_CLK_48MHZ |
        FX2LAFW_CMD_START_FLAGS_SAMPLE_16BIT);
    BOOST_CHECK_EQUAL(command.sample_delay_h, 0);
    BOOST_CHECK_EQUAL(command.sample_delay_l, 3);

    BOOST_CHECK_EQUAL(fx2lafw_build_start_command(SR_MHZ(16), TRUE, &command), SR_ERR);
    BOOST_CHECK_EQUAL(fx2lafw_build_start_command(1234567, FALSE, &command), SR_ERR);
    BOOST_CHECK_EQUAL(fx2lafw_build_start_command(SR_MHZ(1), FALSE, nullptr), SR_ERR_ARG);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_transfer_sizing_matches_upstream_rules)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    BOOST_CHECK_EQUAL(fx2lafw_transfer_buffer_size(SR_MHZ(1)), 10240U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_count(SR_MHZ(1)), 32U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_timeout_ms(SR_MHZ(1)), 408U);

    BOOST_CHECK_EQUAL(fx2lafw_transfer_buffer_size(SR_KHZ(20)), 512U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_count(SR_KHZ(20)), 19U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_timeout_ms(SR_KHZ(20)), 607U);

    BOOST_CHECK_EQUAL(fx2lafw_transfer_buffer_size(0), 0U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_count(0), 0U);
    BOOST_CHECK_EQUAL(fx2lafw_transfer_timeout_ms(0), 0U);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(logic_packet_adapter_forwards_cross_data)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(
        profile, 1, 2, SR_ST_ACTIVE, TRUE, 0);
    BOOST_REQUIRE(sdi != nullptr);

    uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
    test_datafeed_reset();
    BOOST_REQUIRE_EQUAL(fx2lafw_send_logic_packet(sdi, data, sizeof(data), 1), SR_OK);
    const test_captured_datafeed_packet *packet = test_datafeed_last_packet();

    BOOST_CHECK_EQUAL(packet->type, SR_DF_LOGIC);
    BOOST_CHECK_EQUAL(packet->status, SR_PKT_OK);
    BOOST_CHECK_EQUAL(packet->logic_length, sizeof(data));
    BOOST_CHECK_EQUAL(packet->logic_format, LA_CROSS_DATA);
    BOOST_CHECK_EQUAL(packet->logic_unitsize, 1);
    BOOST_CHECK_EQUAL(packet->logic_data, data);

    BOOST_CHECK_EQUAL(fx2lafw_send_logic_packet(sdi, data, sizeof(data), 2), SR_OK);
    packet = test_datafeed_last_packet();
    BOOST_CHECK_EQUAL(packet->logic_unitsize, 2);

    BOOST_CHECK_EQUAL(fx2lafw_send_logic_packet(nullptr, data, sizeof(data), 1), SR_ERR_ARG);
    BOOST_CHECK_EQUAL(fx2lafw_send_logic_packet(sdi, nullptr, sizeof(data), 1), SR_ERR_ARG);
    BOOST_CHECK_EQUAL(fx2lafw_send_logic_packet(sdi, data, 0, 1), SR_ERR_ARG);
    BOOST_CHECK_EQUAL(fx2lafw_send_logic_packet(sdi, data, sizeof(data), 0), SR_ERR_ARG);

    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(driver_exposes_acquisition_lifecycle)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    BOOST_REQUIRE(fx2lafw_driver_info.dev_acquisition_start != nullptr);
    BOOST_REQUIRE(fx2lafw_driver_info.dev_acquisition_stop != nullptr);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_start_requires_active_open_device)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(
        profile, 1, 2, SR_ST_INACTIVE, TRUE, 0);
    BOOST_REQUIRE(sdi != nullptr);

    BOOST_REQUIRE(fx2lafw_driver_info.dev_acquisition_start != nullptr);
    BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_acquisition_start(sdi, sdi),
        SR_ERR_DEVICE_CLOSED);

    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_stop_without_running_is_safe)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x0925, 0x3881, "", "");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(
        profile, 1, 2, SR_ST_ACTIVE, TRUE, 0);
    BOOST_REQUIRE(sdi != nullptr);

    BOOST_REQUIRE(fx2lafw_driver_info.dev_acquisition_stop != nullptr);
    BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_acquisition_stop(sdi, sdi), SR_OK);

    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_stop_is_idempotent_for_test_device)
{
#ifdef HAVE_UPSTREAM_FX2LAFW
    const fx2lafw_profile *profile = fx2lafw_profile_find(
        0x1d50, 0x608d, "sigrok", "fx2lafw");
    BOOST_REQUIRE(profile != nullptr);

    sr_dev_inst *sdi = fx2lafw_dev_inst_new_for_profile(
        profile, 1, 2, SR_ST_ACTIVE, TRUE, 0);
    BOOST_REQUIRE(sdi != nullptr);

    BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_acquisition_stop(sdi, sdi), SR_OK);
    BOOST_CHECK_EQUAL(fx2lafw_driver_info.dev_acquisition_stop(sdi, sdi), SR_OK);

    sr_dev_inst_free(sdi);
#else
    BOOST_TEST_MESSAGE("upstream fx2lafw is disabled for this build");
#endif
}

BOOST_AUTO_TEST_SUITE_END()
