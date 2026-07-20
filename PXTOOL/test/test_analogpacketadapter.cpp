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

BOOST_AUTO_TEST_CASE(upstream_direct_core_initializes_standard_analog_packet)
{
    sr_datafeed_analog analog{};
    sr_analog_encoding encoding{};
    sr_analog_meaning meaning{};
    sr_analog_spec spec{};

    BOOST_REQUIRE_EQUAL(sr_analog_init(&analog, &encoding, &meaning, &spec, 3), SR_OK);
    BOOST_CHECK_EQUAL(analog.encoding, &encoding);
    BOOST_CHECK_EQUAL(analog.meaning, &meaning);
    BOOST_CHECK_EQUAL(analog.spec, &spec);
    BOOST_CHECK_EQUAL(encoding.unitsize, sizeof(float));
    BOOST_CHECK(encoding.is_float);
    BOOST_CHECK_EQUAL(spec.spec_digits, 3);
}

BOOST_AUTO_TEST_CASE(upstream_direct_core_forwards_analog_datafeed_packet)
{
    sr_dev_inst sdi{};
    sr_datafeed_packet packet{};
    sr_datafeed_analog analog{};
    sr_analog_encoding encoding{};
    sr_analog_meaning meaning{};
    sr_analog_spec spec{};
    float sample = 1.25f;

    BOOST_REQUIRE_EQUAL(sr_analog_init(&analog, &encoding, &meaning, &spec, 2), SR_OK);
    analog.data = &sample;
    analog.num_samples = 1;
    packet.type = SR_DF_ANALOG;
    packet.payload = &analog;

    test_datafeed_reset();
    BOOST_REQUIRE_EQUAL(sr_session_send(&sdi, &packet), SR_OK);
    BOOST_CHECK_EQUAL(test_datafeed_last_packet()->type, SR_DF_ANALOG);
}
