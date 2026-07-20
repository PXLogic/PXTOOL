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
