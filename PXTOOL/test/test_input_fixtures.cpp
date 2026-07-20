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
 */

#include <boost/test/unit_test.hpp>

#include "test_datafeed_stub.h"

extern "C" {
#include "libsigrok-internal.h"
}

BOOST_AUTO_TEST_CASE(upstream_direct_core_creates_channels_on_the_device)
{
    sr_dev_inst sdi{};
    sr_channel *channel = sr_channel_new(&sdi, 3, SR_CHANNEL_LOGIC, TRUE, "D3");

    BOOST_REQUIRE(channel != nullptr);
    BOOST_CHECK_EQUAL(g_slist_length(sdi.channels), 1);
    BOOST_CHECK_EQUAL(channel->index, 3);
    BOOST_CHECK_EQUAL(channel->sdi, &sdi);

    g_slist_free_full(sdi.channels, sr_channel_free_cb);
}

BOOST_AUTO_TEST_CASE(upstream_direct_core_forwards_standard_datafeed_packets)
{
    sr_dev_inst sdi{};
    sr_datafeed_packet packet{};
    sr_datafeed_logic logic{};
    uint8_t sample = 0xa5;

    test_datafeed_reset();
    BOOST_REQUIRE_EQUAL(std_session_send_df_header(&sdi), SR_OK);
    BOOST_CHECK_EQUAL(test_datafeed_last_packet()->type, SR_DF_HEADER);

    BOOST_REQUIRE_EQUAL(sr_session_send_meta(&sdi, SR_CONF_SAMPLERATE,
        g_variant_new_uint64(1000000)), SR_OK);
    BOOST_CHECK_EQUAL(test_datafeed_last_packet()->type, SR_DF_META);

    logic.length = 1;
    logic.unitsize = 1;
    logic.data = &sample;
    packet.type = SR_DF_LOGIC;
    packet.payload = &logic;
    BOOST_REQUIRE_EQUAL(sr_session_send(&sdi, &packet), SR_OK);
    BOOST_CHECK_EQUAL(test_datafeed_last_packet()->type, SR_DF_LOGIC);

    packet.type = SR_DF_END;
    packet.payload = nullptr;
    BOOST_REQUIRE_EQUAL(sr_session_send(&sdi, &packet), SR_OK);
    BOOST_CHECK_EQUAL(test_datafeed_last_packet()->type, SR_DF_END);
}

BOOST_AUTO_TEST_CASE(vcd_free_releases_cached_header_channels_and_groups)
{
    static const char vcd[] =
        "$timescale 1 ns $end\n"
        "$scope module test $end\n"
        "$var wire 1 ! flag $end\n"
        "$var wire 2 \" bus [1:0] $end\n"
        "$upscope $end\n"
        "$enddefinitions $end\n"
        "#0\n"
        "0!\n"
        "b00 \"\n";
    const sr_input_module *module = sr_input_find("vcd");
    sr_input *input;
    sr_dev_inst *sdi;
    GString *buffer;
    sr_channel *first_channel;
    sr_channel_group *first_group;

    BOOST_REQUIRE(module != nullptr);
    input = sr_input_new(module, nullptr);
    BOOST_REQUIRE(input != nullptr);

    buffer = g_string_new_len(vcd, sizeof(vcd) - 1);
    BOOST_REQUIRE_EQUAL(sr_input_send(input, buffer), SR_OK);
    g_string_free(buffer, TRUE);

    sdi = sr_input_dev_inst_get(input);
    BOOST_REQUIRE(sdi != nullptr);
    BOOST_CHECK_EQUAL(g_slist_length(sdi->channels), 3);
    BOOST_CHECK_EQUAL(g_slist_length(sdi->channel_groups), 1);
    first_channel = static_cast<sr_channel *>(sdi->channels->data);
    first_group = static_cast<sr_channel_group *>(sdi->channel_groups->data);

    BOOST_REQUIRE_EQUAL(sr_input_reset(input), SR_OK);
    buffer = g_string_new_len(vcd, sizeof(vcd) - 1);
    BOOST_REQUIRE_EQUAL(sr_input_send(input, buffer), SR_OK);
    g_string_free(buffer, TRUE);

    sdi = sr_input_dev_inst_get(input);
    BOOST_REQUIRE(sdi != nullptr);
    BOOST_CHECK_EQUAL(sdi->channels->data, first_channel);
    BOOST_CHECK_EQUAL(sdi->channel_groups->data, first_group);

    sr_test_channel_lifecycle_reset();
    sr_input_free(input);

    BOOST_CHECK_EQUAL(sr_test_channel_free_count(), 3);
    BOOST_CHECK_EQUAL(sr_test_channel_group_free_count(), 1);
}
