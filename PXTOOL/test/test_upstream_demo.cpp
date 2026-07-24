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
extern SR_PRIV struct sr_dev_driver upstream_demo_driver_info;

void test_input_observer_reset(void);
unsigned int test_input_observer_logic_packets(void);
uint64_t test_input_observer_logic_samples(void);
bool test_input_observer_saw_end(void);
}

BOOST_AUTO_TEST_SUITE(upstream_demo)

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
