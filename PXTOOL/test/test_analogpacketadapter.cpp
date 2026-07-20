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

#include <stdexcept>
#include <utility>

#include "../pv/data/analogpacketadapter.h"

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

BOOST_AUTO_TEST_CASE(upstream_direct_core_reports_utf8_ohm_unit)
{
    sr_datafeed_analog analog{};
    sr_analog_encoding encoding{};
    sr_analog_meaning meaning{};
    sr_analog_spec spec{};
    char *unit = nullptr;

    BOOST_REQUIRE_EQUAL(sr_analog_init(&analog, &encoding, &meaning, &spec, 3), SR_OK);
    meaning.unit = SR_UNIT_OHM;

    BOOST_REQUIRE_EQUAL(sr_analog_unit_to_string(&analog, &unit), SR_OK);
    BOOST_REQUIRE(unit != nullptr);
    BOOST_CHECK_EQUAL(std::string(unit), std::string("\xe2\x84\xa6"));
    g_free(unit);
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

BOOST_AUTO_TEST_CASE(analog_adapter_preserves_channel_order_and_metadata)
{
    const auto packet = pv::data::makeAnalogPacket(
        {{"A0", 0}, {"A1", 1}},
        {0.0F, 1.0F, 0.5F, -0.5F},
        2,
        1'000'000,
        SR_MQ_VOLTAGE,
        SR_UNIT_VOLT);

    BOOST_REQUIRE(packet.packet.payload == &packet.analog);
    BOOST_CHECK_EQUAL(packet.packet.type, SR_DF_ANALOG);
    BOOST_REQUIRE(packet.analog.data == packet.samples.data());
    BOOST_REQUIRE(packet.analog.encoding == &packet.encoding);
    BOOST_REQUIRE(packet.analog.meaning == &packet.meaning);
    BOOST_REQUIRE(packet.analog.spec == &packet.spec);
    BOOST_CHECK_EQUAL(packet.analog.num_samples, 2);
    BOOST_CHECK_EQUAL(packet.encoding.unitsize, sizeof(float));
    BOOST_CHECK(packet.encoding.is_float);
    BOOST_CHECK_EQUAL(packet.meaning.mq, SR_MQ_VOLTAGE);
    BOOST_CHECK_EQUAL(packet.meaning.unit, SR_UNIT_VOLT);
    BOOST_CHECK_EQUAL(packet.samplerate, 1'000'000ULL);

    BOOST_REQUIRE_EQUAL(g_slist_length(packet.meaning.channels), 2);
    const auto *first = static_cast<const sr_channel *>(
        g_slist_nth_data(packet.meaning.channels, 0));
    const auto *second = static_cast<const sr_channel *>(
        g_slist_nth_data(packet.meaning.channels, 1));
    BOOST_REQUIRE(first != nullptr);
    BOOST_REQUIRE(second != nullptr);
    BOOST_CHECK_EQUAL(std::string(first->name), "A0");
    BOOST_CHECK_EQUAL(std::string(second->name), "A1");

    BOOST_REQUIRE_EQUAL(g_slist_length(packet.meta.config), 1);
    const auto *samplerate_config = static_cast<const sr_config *>(
        packet.meta.config->data);
    BOOST_REQUIRE(samplerate_config != nullptr);
    BOOST_CHECK_EQUAL(samplerate_config->key, SR_CONF_SAMPLERATE);
    BOOST_CHECK_EQUAL(g_variant_get_uint64(samplerate_config->data),
                      1'000'000ULL);
}

BOOST_AUTO_TEST_CASE(analog_adapter_keeps_borrowed_channel_identity)
{
    sr_channel channel{};
    channel.index = 3;
    channel.type = SR_CHANNEL_ANALOG;
    channel.enabled = TRUE;
    channel.name = const_cast<char *>("ADC3");

    const auto packet = pv::data::makeAnalogPacket(
        {pv::data::AnalogChannelRef(&channel)}, {0.25F}, 1, 48'000,
        SR_MQ_VOLTAGE, SR_UNIT_VOLT);

    BOOST_CHECK(packet.meaning.channels->data == &channel);
}

BOOST_AUTO_TEST_CASE(analog_adapter_rebinds_internal_pointers_after_move)
{
    auto source = pv::data::makeAnalogPacket(
        {{"A0", 0}}, {0.25F, -0.25F}, 2, 48'000,
        SR_MQ_VOLTAGE, SR_UNIT_VOLT);
    const sr_channel *channel = static_cast<const sr_channel *>(
        source.meaning.channels->data);

    auto moved = std::move(source);

    BOOST_CHECK(moved.packet.payload == &moved.analog);
    BOOST_CHECK(moved.analog.data == moved.samples.data());
    BOOST_CHECK(moved.analog.encoding == &moved.encoding);
    BOOST_CHECK(moved.analog.meaning == &moved.meaning);
    BOOST_CHECK(moved.analog.spec == &moved.spec);
    BOOST_CHECK(moved.meaning.channels == moved.channels);
    BOOST_CHECK(moved.meaning.channels->data == channel);
    BOOST_CHECK(moved.meta.config->data == &moved.samplerateConfig);
}

BOOST_AUTO_TEST_CASE(analog_adapter_rejects_invalid_channel_and_sample_counts)
{
    BOOST_CHECK_THROW(pv::data::makeAnalogPacket(
        {}, {0.0F}, 1, 1'000'000, SR_MQ_VOLTAGE, SR_UNIT_VOLT),
        std::invalid_argument);
    BOOST_CHECK_THROW(pv::data::makeAnalogPacket(
        {{"A0", 0}, {"A1", 1}}, {0.0F, 1.0F, 0.5F}, 2, 1'000'000,
        SR_MQ_VOLTAGE, SR_UNIT_VOLT), std::invalid_argument);
    BOOST_CHECK_THROW(pv::data::makeAnalogPacket(
        {{"A0", 0}}, {}, 0, 1'000'000,
        SR_MQ_VOLTAGE, SR_UNIT_VOLT), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(analog_adapter_converts_selected_unsigned8_channels)
{
    const std::vector<uint8_t> source = {
        10, 20, 30,
        11, 21, 31,
    };
    const std::vector<float> converted =
        pv::data::convertUnsigned8AnalogSamples(
            source.data(), 2, 3,
            {{2, 32.0, 0.1}, {0, 12.0, 0.5}});

    BOOST_REQUIRE_EQUAL(converted.size(), 4);
    BOOST_CHECK_CLOSE(converted[0], 0.2F, 0.001);
    BOOST_CHECK_CLOSE(converted[1], 1.0F, 0.001);
    BOOST_CHECK_CLOSE(converted[2], 0.1F, 0.001);
    BOOST_CHECK_CLOSE(converted[3], 0.5F, 0.001);

    BOOST_CHECK_THROW(pv::data::convertUnsigned8AnalogSamples(
        source.data(), 2, 3, {{3, 0.0, 1.0}}), std::invalid_argument);
}
