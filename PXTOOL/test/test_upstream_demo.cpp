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

#include <cmath>
#include <cstdio>
#include <cstring>

extern "C" {
#include "libsigrok-internal.h"
extern SR_PRIV struct sr_dev_driver upstream_demo_driver_info;
extern SR_PRIV struct sr_dev_driver demo_driver_info;
extern char DS_USR_PATH[500];

void test_input_observer_reset(void);
unsigned int test_input_observer_logic_packets(void);
uint64_t test_input_observer_logic_samples(void);
bool test_input_observer_saw_end(void);
bool test_input_observer_analog_is_standard_float(void);
unsigned int test_input_observer_analog_channel_count(void);
int test_input_observer_analog_channel_index(unsigned int index);
size_t test_input_observer_analog_prefix_length(void);
float test_input_observer_analog_prefix(unsigned int index);
}

BOOST_AUTO_TEST_SUITE(upstream_demo)

namespace {

sr_dev_inst *open_native_demo()
{
    std::snprintf(DS_USR_PATH, 500, "%s/PXTOOL", DSVIEW_SOURCE_DIR);
    GSList *devices = demo_driver_info.scan(nullptr);
    BOOST_REQUIRE(devices != nullptr);
    sr_dev_inst *sdi = static_cast<sr_dev_inst *>(devices->data);
    BOOST_REQUIRE(sdi != nullptr);
    g_slist_free(devices);
    BOOST_REQUIRE_EQUAL(demo_driver_info.dev_open(sdi), SR_OK);
    BOOST_REQUIRE_EQUAL(demo_driver_info.config_set(
        SR_CONF_DEVICE_MODE, g_variant_new_int16(ANALOG), sdi, nullptr, nullptr), SR_OK);
    return sdi;
}

void close_native_demo(sr_dev_inst *sdi)
{
    if (sdi)
        demo_driver_info.dev_destroy(sdi);
}

bool pattern_list_contains(GVariant *patterns, const char *pattern)
{
    gsize count = 0;
    const gchar * const *names = g_variant_get_strv(patterns, &count);
    bool found = false;
    for (gsize i = 0; i < count; i++)
        found = found || std::strcmp(names[i], pattern) == 0;
    g_free(const_cast<gchar **>(names));
    return found;
}

void collect_native_analog(sr_dev_inst *sdi, uint64_t samples)
{
    BOOST_REQUIRE_EQUAL(demo_driver_info.config_set(
        SR_CONF_LIMIT_SAMPLES, g_variant_new_uint64(samples), sdi, nullptr, nullptr), SR_OK);
    test_input_observer_reset();
    BOOST_REQUIRE(sr_session_new() != nullptr);
    BOOST_REQUIRE_EQUAL(demo_driver_info.dev_acquisition_start(sdi, nullptr), SR_OK);
    BOOST_REQUIRE_EQUAL(sr_session_run(), SR_OK);
    BOOST_REQUIRE_EQUAL(sr_session_destroy(), SR_OK);
}

}

BOOST_AUTO_TEST_CASE(analog_has_five_channels_and_generator_patterns)
{
    sr_dev_inst *sdi = open_native_demo();
    BOOST_REQUIRE_EQUAL(g_slist_length(sdi->channels), 5);
    for (GSList *item = sdi->channels; item; item = item->next) {
        sr_channel *channel = static_cast<sr_channel *>(item->data);
        BOOST_REQUIRE(channel != nullptr);
        BOOST_CHECK_EQUAL(channel->type, SR_CHANNEL_ANALOG);
        BOOST_CHECK(channel->enabled);
    }

    GVariant *patterns = nullptr;
    BOOST_REQUIRE_EQUAL(demo_driver_info.config_list(
        SR_CONF_PATTERN_MODE, &patterns, sdi, nullptr), SR_OK);
    BOOST_REQUIRE(patterns != nullptr);
    BOOST_CHECK(pattern_list_contains(patterns, "sine"));
    BOOST_CHECK(pattern_list_contains(patterns, "square"));
    BOOST_CHECK(pattern_list_contains(patterns, "triangle"));
    BOOST_CHECK(pattern_list_contains(patterns, "sawtooth"));
    BOOST_CHECK(pattern_list_contains(patterns, "random"));
    g_variant_unref(patterns);

    sr_channel *channel0 = static_cast<sr_channel *>(sdi->channels->data);
    BOOST_REQUIRE_EQUAL(demo_driver_info.config_set(
        SR_CONF_PATTERN_MODE, g_variant_new_string("triangle"), sdi, nullptr, nullptr), SR_OK);
    GVariant *pattern = nullptr;
    BOOST_REQUIRE_EQUAL(demo_driver_info.config_get(
        SR_CONF_PATTERN_MODE, &pattern, sdi, channel0, nullptr), SR_OK);
    BOOST_REQUIRE(pattern != nullptr);
    BOOST_CHECK_EQUAL(std::strcmp(g_variant_get_string(pattern, nullptr), "triangle"), 0);
    g_variant_unref(pattern);
    BOOST_CHECK_EQUAL(demo_driver_info.config_set(
        SR_CONF_AMPLITUDE, g_variant_new_double(NAN), sdi, channel0, nullptr), SR_ERR_ARG);
    close_native_demo(sdi);
}

BOOST_AUTO_TEST_CASE(generated_analog_packet_is_standard_float)
{
    sr_dev_inst *sdi = open_native_demo();
    collect_native_analog(sdi, 32);
    BOOST_CHECK(test_input_observer_analog_is_standard_float());
    BOOST_CHECK_EQUAL(test_input_observer_analog_channel_count(), 5U);
    BOOST_CHECK_EQUAL(test_input_observer_analog_prefix_length(), 160U);
    for (unsigned int index = 0; index < 5; index++)
        BOOST_CHECK_EQUAL(test_input_observer_analog_channel_index(index), static_cast<int>(index));
    close_native_demo(sdi);
}

BOOST_AUTO_TEST_CASE(generated_analog_data_is_packet_rate_limited)
{
    sr_dev_inst *sdi = open_native_demo();
    const gint64 started_at = g_get_monotonic_time();
    collect_native_analog(sdi, 1024);
    const gint64 elapsed_us = g_get_monotonic_time() - started_at;

    // The demo emits two 512-sample packets at 200 packets per second.
    // A generator must not drain both packets in one event-loop burst.
    BOOST_CHECK_GE(elapsed_us, 8 * G_TIME_SPAN_MILLISECOND);
    close_native_demo(sdi);
}

BOOST_AUTO_TEST_CASE(analog_waveform_respects_configuration_and_enablement)
{
    sr_dev_inst *sdi = open_native_demo();
    sr_channel *channel0 = static_cast<sr_channel *>(sdi->channels->data);
    sr_channel *channel1 = static_cast<sr_channel *>(sdi->channels->next->data);
    BOOST_REQUIRE_EQUAL(demo_driver_info.config_set(
        SR_CONF_PATTERN_MODE, g_variant_new_string("square"), sdi, channel0, nullptr), SR_OK);
    BOOST_REQUIRE_EQUAL(demo_driver_info.config_set(
        SR_CONF_AMPLITUDE, g_variant_new_double(2.0), sdi, channel0, nullptr), SR_OK);
    BOOST_REQUIRE_EQUAL(demo_driver_info.config_set(
        SR_CONF_OFFSET, g_variant_new_double(1.0), sdi, channel0, nullptr), SR_OK);
    BOOST_REQUIRE_EQUAL(demo_driver_info.config_set(
        SR_CONF_PROBE_EN, g_variant_new_boolean(FALSE), sdi, channel1, nullptr), SR_OK);
    BOOST_REQUIRE_EQUAL(demo_driver_info.config_set(
        SR_CONF_SAMPLERATE, g_variant_new_uint64(SR_KHZ(10)), sdi, nullptr, nullptr), SR_OK);

    collect_native_analog(sdi, 512);
    BOOST_CHECK_EQUAL(test_input_observer_analog_channel_count(), 4U);
    for (unsigned int index = 0; index < 4; index++)
        BOOST_CHECK_NE(test_input_observer_analog_channel_index(index), 1);

    bool saw_high = false;
    bool saw_low = false;
    for (size_t index = 0; index < test_input_observer_analog_prefix_length(); index += 4) {
        const float value = test_input_observer_analog_prefix(index);
        BOOST_CHECK(value >= -1.0F);
        BOOST_CHECK(value <= 3.0F);
        saw_high = saw_high || std::fabs(value - 3.0F) < 0.0001F;
        saw_low = saw_low || std::fabs(value + 1.0F) < 0.0001F;
    }
    BOOST_CHECK(saw_high);
    BOOST_CHECK(saw_low);
    close_native_demo(sdi);
}

BOOST_AUTO_TEST_CASE(channels_default_enabled_and_support_probe_enable_config)
{
#ifdef HAVE_UPSTREAM_COMPAT_DEMO
    GSList *devices = upstream_demo_driver_info.scan(nullptr);

    BOOST_REQUIRE(devices != nullptr);
    sr_dev_inst *sdi = static_cast<sr_dev_inst *>(devices->data);
    BOOST_REQUIRE(sdi != nullptr);
    BOOST_REQUIRE_EQUAL(g_slist_length(sdi->channels), 8);

    for (GSList *l = sdi->channels; l; l = l->next) {
        sr_channel *ch = static_cast<sr_channel *>(l->data);
        BOOST_REQUIRE(ch != nullptr);
        BOOST_CHECK(ch->enabled);
    }

    GVariant *devopts = nullptr;
    BOOST_REQUIRE_EQUAL(upstream_demo_driver_info.config_list(
        SR_CONF_DEVICE_OPTIONS, &devopts, sdi, nullptr), SR_OK);
    BOOST_REQUIRE(devopts != nullptr);
    bool has_probe_enable = false;
    gsize option_count = 0;
    const int32_t *options = static_cast<const int32_t *>(
        g_variant_get_fixed_array(devopts, &option_count, sizeof(int32_t)));
    for (gsize i = 0; i < option_count; i++) {
        if (options[i] == SR_CONF_PROBE_EN)
            has_probe_enable = true;
    }
    g_variant_unref(devopts);
    BOOST_CHECK(has_probe_enable);

    GVariant *valid_channels = nullptr;
    BOOST_REQUIRE_EQUAL(upstream_demo_driver_info.config_get(
        SR_CONF_VLD_CH_NUM, &valid_channels, sdi, nullptr, nullptr), SR_OK);
    BOOST_REQUIRE(valid_channels != nullptr);
    BOOST_CHECK_EQUAL(g_variant_get_int16(valid_channels), 8);
    g_variant_unref(valid_channels);

    sr_channel *first_ch = static_cast<sr_channel *>(sdi->channels->data);
    GVariant *enabled = nullptr;
    BOOST_REQUIRE_EQUAL(upstream_demo_driver_info.config_get(
        SR_CONF_PROBE_EN, &enabled, sdi, first_ch, nullptr), SR_OK);
    BOOST_REQUIRE(enabled != nullptr);
    BOOST_CHECK(g_variant_get_boolean(enabled));
    g_variant_unref(enabled);

    BOOST_REQUIRE_EQUAL(upstream_demo_driver_info.config_set(
        SR_CONF_PROBE_EN, g_variant_new_boolean(FALSE), sdi, first_ch, nullptr), SR_OK);
    BOOST_CHECK(!first_ch->enabled);

    BOOST_REQUIRE_EQUAL(upstream_demo_driver_info.config_set(
        SR_CONF_PROBE_EN, g_variant_new_boolean(TRUE), sdi, first_ch, nullptr), SR_OK);
    BOOST_CHECK(first_ch->enabled);

    upstream_demo_driver_info.dev_destroy(sdi);
    g_slist_free(devices);
#else
    BOOST_TEST_MESSAGE("upstream compat demo is disabled for this build");
#endif
}

BOOST_AUTO_TEST_CASE(acquisition_emits_configured_sample_limit)
{
#ifdef HAVE_UPSTREAM_COMPAT_DEMO
    GSList *devices = upstream_demo_driver_info.scan(nullptr);

    BOOST_REQUIRE(devices != nullptr);
    sr_dev_inst *sdi = static_cast<sr_dev_inst *>(devices->data);
    BOOST_REQUIRE(sdi != nullptr);

    const uint64_t limit_samples = 1024;
    BOOST_REQUIRE_EQUAL(upstream_demo_driver_info.config_set(
        SR_CONF_LIMIT_SAMPLES, g_variant_new_uint64(limit_samples), sdi, nullptr, nullptr), SR_OK);

    test_input_observer_reset();

    BOOST_REQUIRE_EQUAL(upstream_demo_driver_info.dev_acquisition_start(sdi, nullptr), SR_OK);

    BOOST_CHECK_EQUAL(test_input_observer_logic_packets(), 1);
    BOOST_CHECK_EQUAL(test_input_observer_logic_samples(), limit_samples);
    BOOST_CHECK(test_input_observer_saw_end());

    upstream_demo_driver_info.dev_destroy(sdi);
    g_slist_free(devices);
#else
    BOOST_TEST_MESSAGE("upstream compat demo is disabled for this build");
#endif
}

BOOST_AUTO_TEST_SUITE_END()
