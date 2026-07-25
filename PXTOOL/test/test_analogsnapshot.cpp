/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include <boost/test/unit_test.hpp>

#include "pv/data/analogpacketadapter.h"
#include "pv/data/analogsnapshot.h"

BOOST_AUTO_TEST_SUITE(analog_snapshot)

BOOST_AUTO_TEST_CASE(keeps_float_channels_and_extrema)
{
    pv::data::AnalogSnapshot snapshot;
    const auto packet = pv::data::makeAnalogPacket(
        {{"A0", 0}, {"A1", 1}}, {1.0F, 10.0F, 2.0F, 20.0F}, 2,
        1'000'000, SR_MQ_VOLTAGE, SR_UNIT_VOLT);

    BOOST_REQUIRE(snapshot.first_payload(packet.analog, 8, packet.channels));
    BOOST_CHECK(snapshot.is_float());
    BOOST_CHECK_CLOSE(snapshot.sample_as_double(0, 1), 2.0, 0.001);
    BOOST_CHECK_EQUAL(snapshot.sample_as_double(1, 0), 10.0);
    BOOST_CHECK_CLOSE(snapshot.channel_min(0), 1.0, 0.001);
    BOOST_CHECK_CLOSE(snapshot.channel_max(1), 20.0, 0.001);
}

BOOST_AUTO_TEST_SUITE_END()
