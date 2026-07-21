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

#include <cstdint>

#include <QByteArray>

#include "test_datafeed_stub.h"

extern "C" {
#include "libsigrok-internal.h"

void test_input_observer_reset(void);
unsigned int test_input_observer_logic_packets(void);
uint64_t test_input_observer_logic_samples(void);
unsigned int test_input_observer_analog_packets(void);
uint64_t test_input_observer_samplerate(void);
bool test_input_observer_saw_end(void);
}

namespace {

GHashTable *options(std::initializer_list<std::pair<const char *, GVariant *>> values)
{
    GHashTable *table = g_hash_table_new_full(g_str_hash, g_str_equal, nullptr,
        reinterpret_cast<GDestroyNotify>(g_variant_unref));

    for (const auto &value : values)
        g_hash_table_insert(table, const_cast<char *>(value.first), g_variant_ref_sink(value.second));

    return table;
}

class StreamingInput
{
public:
    StreamingInput(const char *module_id, GHashTable *options = nullptr)
    {
        const sr_input_module *module = sr_input_find(module_id);
        BOOST_REQUIRE(module != nullptr);

        input_ = sr_input_new(module, options);
        if (options)
            g_hash_table_unref(options);
        BOOST_REQUIRE(input_ != nullptr);

        test_input_observer_reset();
    }

    ~StreamingInput()
    {
        sr_input_free(input_);
    }

    void send(const QByteArray &data)
    {
        GString *buffer = g_string_new_len(data.constData(), data.size());
        BOOST_REQUIRE_EQUAL(sr_input_send(input_, buffer), SR_OK);
        g_string_free(buffer, TRUE);
    }

    void send(const char *data)
    {
        send(QByteArray(data));
    }

    void end()
    {
        BOOST_REQUIRE_EQUAL(sr_input_end(input_), SR_OK);
    }

    unsigned int logicPackets() const { return test_input_observer_logic_packets(); }
    uint64_t logicSamples() const { return test_input_observer_logic_samples(); }
    unsigned int analogPackets() const { return test_input_observer_analog_packets(); }
    uint64_t samplerate() const { return test_input_observer_samplerate(); }
    bool sawEnd() const { return test_input_observer_saw_end(); }

private:
    sr_input *input_ = nullptr;
};

QByteArray makeWavFixture()
{
    const char wav[] = {
        'R', 'I', 'F', 'F',
        40, 0, 0, 0,
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        16, 0, 0, 0,
        1, 0,
        1, 0,
        0x40, 0x1f, 0, 0,
        0x40, 0x1f, 0, 0,
        1, 0,
        8, 0,
        'd', 'a', 't', 'a',
        4, 0, 0, 0,
        0, 64, static_cast<char>(128), static_cast<char>(255),
    };

    return QByteArray(wav, sizeof(wav));
}

} // namespace

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

BOOST_AUTO_TEST_CASE(binary_input_streams_logic_packets)
{
    StreamingInput input("binary", options({
        {"numchannels", g_variant_new_int32(2)},
        {"samplerate", g_variant_new_uint64(1000000)},
    }));

    input.send(QByteArray::fromHex("00010302"));
    input.end();

    BOOST_CHECK_EQUAL(input.logicPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicSamples(), 4);
    BOOST_CHECK_EQUAL(input.samplerate(), 1000000);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(vcd_input_streams_logic_packets)
{
    static const char vcd[] =
        "$timescale 1 us $end\n"
        "$scope module test $end\n"
        "$var wire 1 ! flag $end\n"
        "$upscope $end\n"
        "$enddefinitions $end\n"
        "#0\n"
        "0!\n"
        "#1\n"
        "1!\n"
        "#2\n"
        "0!\n";
    StreamingInput input("vcd", options({
        {"numchannels", g_variant_new_uint32(1)},
        {"skip", g_variant_new_uint64(0)},
        {"samplerate_overwrite", g_variant_new_uint64(1000000)},
        {"downsample", g_variant_new_uint64(1)},
        {"compress", g_variant_new_uint64(0)},
    }));

    input.send(QByteArray(vcd, sizeof(vcd) - 1));
    input.end();

    BOOST_CHECK_GE(input.logicPackets(), 1);
    BOOST_CHECK_GE(input.logicSamples(), 2);
    BOOST_CHECK_EQUAL(input.samplerate(), 1000000);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(wav_input_streams_analog_packets)
{
    StreamingInput input("wav");

    input.send(makeWavFixture());
    input.end();

    BOOST_CHECK_EQUAL(input.analogPackets(), 1);
    BOOST_CHECK_EQUAL(input.samplerate(), 8000);
    BOOST_CHECK(input.sawEnd());
}
