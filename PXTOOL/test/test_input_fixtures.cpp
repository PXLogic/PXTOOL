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
#include <cstring>
#include <vector>

#include <QByteArray>
#include <QFile>
#include <QTemporaryFile>
#include <glib.h>
#include <zlib.h>
#include "test_datafeed_stub.h"

#include "PXTOOL/pv/data/inputimporter.h"

extern "C" {
#include "libsigrok-internal.h"
#include "minilzo/minilzo.h"

void test_input_observer_reset(void);
unsigned int test_input_observer_header_packets(void);
unsigned int test_input_observer_meta_packets(void);
unsigned int test_input_observer_logic_packets(void);
uint64_t test_input_observer_logic_samples(void);
unsigned int test_input_observer_analog_packets(void);
uint64_t test_input_observer_analog_samples(void);
uint64_t test_input_observer_samplerate(void);
uint64_t test_input_observer_sample_limit(void);
bool test_input_observer_saw_end(void);
size_t test_input_observer_logic_prefix_len(void);
const uint8_t *test_input_observer_logic_prefix(void);
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

    unsigned int headerPackets() const { return test_input_observer_header_packets(); }
    unsigned int metaPackets() const { return test_input_observer_meta_packets(); }
    unsigned int logicPackets() const { return test_input_observer_logic_packets(); }
    uint64_t logicSamples() const { return test_input_observer_logic_samples(); }
    unsigned int analogPackets() const { return test_input_observer_analog_packets(); }
    uint64_t analogSamples() const { return test_input_observer_analog_samples(); }
    uint64_t samplerate() const { return test_input_observer_samplerate(); }
    uint64_t sampleLimit() const { return test_input_observer_sample_limit(); }
    bool sawEnd() const { return test_input_observer_saw_end(); }
    uint8_t logicPrefixByte(size_t index) const
    {
        BOOST_REQUIRE(index < test_input_observer_logic_prefix_len());
        return test_input_observer_logic_prefix()[index];
    }
    unsigned int channelCount() const
    {
        const sr_dev_inst *sdi = sr_input_dev_inst_get(input_);
        return sdi ? g_slist_length(sdi->channels) : 0U;
    }

    unsigned int enabledChannelCount() const
    {
        const sr_dev_inst *sdi = sr_input_dev_inst_get(input_);
        if (!sdi)
            return 0;

        unsigned int count = 0;
        for (const GSList *l = sdi->channels; l; l = l->next) {
            const sr_channel *channel = static_cast<const sr_channel *>(l->data);
            if (channel && channel->enabled)
                count++;
        }
        return count;
    }

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

void appendU16(QByteArray &data, uint16_t value)
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

void appendU32(QByteArray &data, uint32_t value)
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
    data.append(static_cast<char>((value >> 16) & 0xff));
    data.append(static_cast<char>((value >> 24) & 0xff));
}

void appendU64(QByteArray &data, uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8)
        data.append(static_cast<char>((value >> shift) & 0xff));
}

void appendDouble(QByteArray &data, double value)
{
    static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64-bit");
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    appendU64(data, bits);
}

QByteArray makeTrace32HeaderFixture()
{
    QByteArray data(0x50, '\0');
    const QByteArray name("trace32 power integrator data");

    std::copy(name.begin(), name.end(), data.begin());
    data[0x36] = 0x08;
    data[0x38] = 28;

    return data;
}

QByteArray makeTrace32ImportFixture()
{
    QByteArray data = makeTrace32HeaderFixture();
    QByteArray firstRecord(28, '\0');
    QByteArray secondRecord(28, '\0');

    firstRecord[0x08] = 0x01;
    secondRecord[0x00] = 0x01;
    secondRecord[0x08] = 0x00;

    data[0x3c] = 0x02;
    data[0x40] = 0x02;

    data.append(firstRecord);
    data.append(secondRecord);
    return data;
}

QByteArray makeSaleaeLogicFixture()
{
    QByteArray data;

    data.append("<SALEAE>");
    appendU32(data, 0);
    appendU32(data, 0);
    appendU32(data, 0);
    appendDouble(data, 0.0);
    appendDouble(data, 4e-6);
    appendU64(data, 3);
    appendDouble(data, 1e-6);
    appendDouble(data, 2e-6);
    appendDouble(data, 3e-6);

    return data;
}

QByteArray makeStfScanFixture()
{
    QByteArray data("Sigma Test File", 16);
    data.append("TestFirstTS=0\r\n");
    data.append("TestLengthTS=0\r\n");
    data.append("Sigma.ClockSource=Internal:Period=50ns\r\n");
    data.append("Sigma.SigmaInputs=A0\r\n");
    data.append("Traces.Traces=Input0:Signal=A0\r\n");
    data.append('\0');

    return data;
}

QByteArray makeIsfImportFixture()
{
    QByteArray data;

    data.append("NR_PT 3;");
    data.append("YOFF 0;");
    data.append("YZERO 0;");
    data.append("YMULT 1;");
    data.append("XINCR 0.5;");
    data.append("BYT_NR 1;");
    data.append("BYT_OR LSB;");
    data.append("BN_FMT RI;");
    data.append("ENCDG BIN;");
    data.append("WFID \"CH1,DC\";");
    data.append("CURVE #13");
    data.append(QByteArray::fromHex("000102"));

    return data;
}

QByteArray makeLogicportScanFixture()
{
    QByteArray data("Version", 7);
    data.append('\x11');
    data.append("1.2");
    data.append('\x11');
    data.append("123");
    data.append('\x11');
    data.append(" CAUTION: Do not change the contents of this file.\r\n");
    return data;
}

QByteArray makeStfImportFixture()
{
    QByteArray rawRecord;
    QByteArray compressed(1600, '\0');
    QByteArray data("Sigma Test File", 16);
    lzo_uint compressedLen = compressed.size();
    std::vector<unsigned char> workmem(LZO1X_1_MEM_COMPRESS);

    rawRecord.reserve(1440);
    appendU32(rawRecord, 0);
    appendU32(rawRecord, 1);
    appendU64(rawRecord, 0);
    appendU64(rawRecord, 447);
    appendU64(rawRecord, 448);

    for (int cluster = 0; cluster < 64; ++cluster)
        appendU64(rawRecord, static_cast<uint64_t>(cluster * 7));

    for (int cluster = 0; cluster < 64; ++cluster) {
        for (int sample = 0; sample < 7; ++sample)
            appendU16(rawRecord, ((cluster * 7 + sample) & 1) ? 0x0001 : 0x0000);
    }

    BOOST_REQUIRE_EQUAL(rawRecord.size(), 1440);

    BOOST_REQUIRE_EQUAL(lzo1x_1_compress(
        reinterpret_cast<const unsigned char *>(rawRecord.constData()),
        rawRecord.size(),
        reinterpret_cast<unsigned char *>(compressed.data()),
        &compressedLen,
        workmem.data()), 0);
    compressed.resize(static_cast<int>(compressedLen));

    data.append("TestFirstTS=0\r\n");
    data.append("TestLengthTS=447\r\n");
    data.append("Sigma.ClockSource=ClockScheme=0;Period=1\r\n");
    data.append("Sigma.SigmaInputs=A0\r\n");
    data.append("Traces.Traces=Type=Input:Caption=D0:Input0=0\r\n");
    data.append('\0');
    appendU32(data, static_cast<uint32_t>(compressed.size()));
    appendU32(data, crc32(0,
        reinterpret_cast<const unsigned char *>(compressed.constData()),
        compressed.size()));
    data.append(compressed);
    appendU32(data, 0xffffffffu);
    appendU32(data, 0u);

    return data;
}

QString writeFixtureFile(const QByteArray &data, const QString &suffix)
{
    QTemporaryFile file(QString("XXXXXX%1").arg(suffix));
    BOOST_REQUIRE(file.open());
    BOOST_REQUIRE_EQUAL(file.write(data), data.size());
    file.flush();
    file.setAutoRemove(false);
    return file.fileName();
}

QString createSparseFile(qint64 size, const QString &suffix)
{
    QTemporaryFile file(QString("XXXXXX%1").arg(suffix));
    BOOST_REQUIRE(file.open());
    BOOST_REQUIRE(file.resize(size));
    file.setAutoRemove(false);
    return file.fileName();
}

QByteArray makeDsViewAlternatingCsvFixture(int sampleCount)
{
    QByteArray csv =
        "; CSV, generated by libsigrok 0.2.0 on Wed Jul 22 18:44:40 2026\n"
        "; Channels (8/8)\n"
        "; Sample rate: 1 MHz\n"
        "; Sample count: " + QByteArray::number(sampleCount) + " Samples\n"
        "Time(s), D0, D1, D2, D3, D4, D5, D6, D7\n";

    for (int i = 0; i < sampleCount; i++) {
        csv += QByteArray::number(i) + "e-06";
        for (int ch = 0; ch < 8; ch++)
            csv += (i % 2) ? ",1" : ",0";
        csv += "\n";
    }

    return csv;
}

QByteArray makeDsViewAlternatingVcdFixture(int sampleCount)
{
    QByteArray vcd =
        "$date Wed Jul 22 22:20:21 2026 $end\n"
        "$version libsigrok 0.2.0 $end\n"
        "$comment\n"
        "  Acquisition with 8/8 channels at 1 MHz\n"
        "$end\n"
        "$timescale 1 us $end\n"
        "$scope module libsigrok $end\n"
        "$var wire 1 ! D0 $end\n"
        "$var wire 1 \" D1 $end\n"
        "$var wire 1 # D2 $end\n"
        "$var wire 1 $ D3 $end\n"
        "$var wire 1 % D4 $end\n"
        "$var wire 1 & D5 $end\n"
        "$var wire 1 ' D6 $end\n"
        "$var wire 1 ( D7 $end\n"
        "$upscope $end\n"
        "$enddefinitions $end\n";

    for (int i = 0; i < sampleCount; i++) {
        vcd += '#';
        vcd += QByteArray::number(i);
        for (int ch = 0; ch < 8; ch++) {
            vcd += ' ';
            vcd += (i % 2) ? '1' : '0';
            vcd += static_cast<char>('!' + ch);
        }
        vcd += '\n';
    }
    vcd += '#';
    vcd += QByteArray::number(sampleCount);
    vcd += '\n';

    return vcd;
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

    input.send(QByteArray::fromHex("0001"));
    input.send(QByteArray::fromHex("0302"));
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicPackets(), 1);
    // DSView cross-data packets are 64-sample channel blocks. The source
    // payload has four samples, followed by zero padding in this transport.
    BOOST_CHECK_EQUAL(input.logicSamples(), 64);
    BOOST_CHECK_EQUAL(input.logicPrefixByte(0), 0x06);
    BOOST_CHECK_EQUAL(input.logicPrefixByte(8), 0x0c);
    BOOST_CHECK_EQUAL(input.samplerate(), 1000000);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(chronovu_input_streams_logic_packets)
{
    StreamingInput input("chronovu-la8", options({
        {"numchannels", g_variant_new_int32(8)},
        {"samplerate", g_variant_new_uint64(100000000)},
    }));

    input.send(QByteArray::fromHex("0001"));
    input.send(QByteArray::fromHex("0302"));
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicSamples(), 64);
    BOOST_CHECK_EQUAL(input.logicPrefixByte(0), 0x06);
    BOOST_CHECK_EQUAL(input.logicPrefixByte(8), 0x0c);
    BOOST_CHECK_EQUAL(input.samplerate(), 100000000);
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

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_GE(input.logicPackets(), 1);
    BOOST_CHECK_GE(input.logicSamples(), 2);
    BOOST_CHECK_EQUAL(input.samplerate(), 1000000);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(vcd_import_timestamp_parser_accepts_inline_value_changes)
{
    uint64_t timestamp = 0;

    BOOST_REQUIRE(pv::data::parseVcdTimestampLine(QStringLiteral("#1024 0! 1\""), timestamp));
    BOOST_CHECK_EQUAL(timestamp, 1024ULL);
}

BOOST_AUTO_TEST_CASE(vcd_import_streams_dsview_export_waveform_as_cross_data)
{
    StreamingInput input("vcd");

    input.send(makeDsViewAlternatingVcdFixture(1024));
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_GE(input.logicPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicSamples(), 1024);
    BOOST_CHECK_EQUAL(input.samplerate(), 1000000);
    BOOST_CHECK_EQUAL(input.channelCount(), 8);
    BOOST_CHECK_EQUAL(input.enabledChannelCount(), 8);
    BOOST_CHECK(input.sawEnd());

    BOOST_REQUIRE_GE(test_input_observer_logic_prefix_len(), 256U);
    const uint8_t *samples = test_input_observer_logic_prefix();
    for (size_t i = 0; i < 256; i++)
        BOOST_CHECK_EQUAL(samples[i], 0xaa);
}

BOOST_AUTO_TEST_CASE(wav_input_streams_analog_packets)
{
    StreamingInput input("wav");

    input.send(makeWavFixture());
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_EQUAL(input.analogPackets(), 1);
    BOOST_CHECK_EQUAL(input.analogSamples(), 4);
    BOOST_CHECK_EQUAL(input.samplerate(), 8000);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(raw_analog_input_streams_analog_packets)
{
    StreamingInput input("raw_analog", options({
        {"numchannels", g_variant_new_int32(1)},
        {"samplerate", g_variant_new_uint64(1234)},
        {"format", g_variant_new_string("U8 (0..255)")},
    }));

    input.send(QByteArray::fromHex("0080ff"));
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_EQUAL(input.analogPackets(), 1);
    BOOST_CHECK_EQUAL(input.analogSamples(), 3);
    BOOST_CHECK_EQUAL(input.samplerate(), 1234);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(isf_input_streams_analog_packets)
{
    StreamingInput input("isf");

    input.send(makeIsfImportFixture());
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_EQUAL(input.analogPackets(), 1);
    BOOST_CHECK_EQUAL(input.analogSamples(), 3);
    BOOST_CHECK_EQUAL(input.samplerate(), 2);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(csv_input_streams_logic_packets)
{
    StreamingInput input("csv", options({
        {"single_column", g_variant_new_uint32(1)},
        {"logic_channels", g_variant_new_uint32(2)},
        {"single_format", g_variant_new_string("hex")},
        {"header", g_variant_new_boolean(FALSE)},
        {"samplerate", g_variant_new_uint64(1000000)},
    }));

    input.send("0\n1\n3\n2\n");
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_GE(input.metaPackets(), 1);
    BOOST_CHECK_GE(input.logicPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicSamples(), 4);
    BOOST_CHECK_EQUAL(input.samplerate(), 1000000);
    BOOST_CHECK_EQUAL(input.sampleLimit(), 4);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(csv_input_reports_total_sample_count)
{
    StreamingInput input("csv", options({
        {"single_column", g_variant_new_uint32(1)},
        {"logic_channels", g_variant_new_uint32(2)},
        {"single_format", g_variant_new_string("hex")},
        {"header", g_variant_new_boolean(FALSE)},
        {"samplerate", g_variant_new_uint64(1000000)},
    }));

    input.send("0\n1\n3\n2\n");
    input.end();

    BOOST_CHECK_EQUAL(input.sampleLimit(), 4);
}

BOOST_AUTO_TEST_CASE(csv_input_accepts_libsigrok_export_layout_by_default)
{
    StreamingInput input("csv");

    input.send(
        "; CSV, generated by libsigrok 0.2.0 on Wed Jul 22 10:23:11 2026\n"
        "; Channels (2/2)\n"
        "; Sample rate: 1 MHz\n"
        "; Sample count: 4  Samples\n"
        "Time(s), D0, D1\n"
        "0,0,0\n"
        "1e-06,1,1\n"
        "2e-06,0,0\n"
        "3e-06,1,1\n");
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_GE(input.metaPackets(), 1);
    BOOST_CHECK_GE(input.logicPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicSamples(), 4);
    BOOST_CHECK_EQUAL(input.samplerate(), 1000000);
    BOOST_CHECK_EQUAL(input.sampleLimit(), 4);
    BOOST_CHECK_EQUAL(input.channelCount(), 2);
    BOOST_CHECK_EQUAL(input.enabledChannelCount(), 2);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(csv_import_preserves_dsview_export_samplerate_and_limit)
{
    const QByteArray csv =
        "; CSV, generated by libsigrok 0.2.0 on Wed Jul 22 10:23:11 2026\n"
        "; Channels (2/2)\n"
        "; Sample rate: 1 MHz\n"
        "; Sample count: 4 Samples\n"
        "Time(s), D0, D1\n"
        "0,0,0\n"
        "1e-06,1,1\n"
        "2e-06,0,0\n"
        "3e-06,1,1\n";
    const QString fileName = writeFixtureFile(csv, ".csv");

    pv::data::CsvImportPlan plan;
    BOOST_REQUIRE(pv::data::estimateDsViewCsvImportPlan(fileName, plan));
    BOOST_CHECK_EQUAL(plan.sampleRate, 1000000ULL);
    BOOST_CHECK_EQUAL(plan.sampleLimit, 4ULL);
    BOOST_CHECK_EQUAL(plan.logicChannelCount, 2ULL);

    QFile::remove(fileName);
}

BOOST_AUTO_TEST_CASE(csv_import_detects_dsview_export_channel_count)
{
    const QString fileName = writeFixtureFile(makeDsViewAlternatingCsvFixture(128), ".csv");

    pv::data::CsvImportPlan plan;
    BOOST_REQUIRE(pv::data::estimateDsViewCsvImportPlan(fileName, plan));
    BOOST_CHECK_EQUAL(plan.sampleRate, 1000000ULL);
    BOOST_CHECK_EQUAL(plan.sampleLimit, 128ULL);
    BOOST_CHECK_EQUAL(plan.logicChannelCount, 8ULL);

    QFile::remove(fileName);
}

BOOST_AUTO_TEST_CASE(csv_import_plan_applies_to_csv_input_option_types)
{
    const QByteArray csv =
        "; CSV, generated by libsigrok 0.2.0 on Wed Jul 22 10:23:11 2026\n"
        "; Channels (8/8)\n"
        "; Sample rate: 1 MHz\n"
        "; Sample count: 128 Samples\n"
        "Time(s), D0, D1, D2, D3, D4, D5, D6, D7\n"
        "0,0,0,0,0,0,0,0,0\n"
        "1e-06,1,1,1,1,1,1,1,1\n";
    const QString fileName = writeFixtureFile(csv, ".csv");

    const sr_input_module *module = sr_input_find("csv");
    BOOST_REQUIRE(module != nullptr);

    const sr_option **definitions = sr_input_options_get(module);
    BOOST_REQUIRE(definitions != nullptr);

    pv::data::IoOptions options(definitions);

    pv::data::CsvImportPlan plan;
    BOOST_REQUIRE(pv::data::estimateDsViewCsvImportPlan(fileName, plan));

    BOOST_CHECK_NO_THROW(pv::data::applyDsViewCsvImportPlan(options, plan));
    BOOST_CHECK_EQUAL(options.value("samplerate").value<guint64>(), 1000000ULL);
    BOOST_CHECK_EQUAL(options.value("logic_channels").value<guint32>(), 8U);
    BOOST_CHECK(options.value("dsview_cross_data").toBool());

    sr_input_options_free(definitions);
    QFile::remove(fileName);
}

BOOST_AUTO_TEST_CASE(csv_import_plan_streams_dsview_export_file)
{
    const sr_input_module *module = sr_input_find("csv");
    BOOST_REQUIRE(module != nullptr);

    const sr_option **definitions = sr_input_options_get(module);
    BOOST_REQUIRE(definitions != nullptr);

    pv::data::IoOptions options(definitions);

    const QString fileName = writeFixtureFile(makeDsViewAlternatingCsvFixture(128), ".csv");
    pv::data::CsvImportPlan plan;
    BOOST_REQUIRE(pv::data::estimateDsViewCsvImportPlan(fileName, plan));
    pv::data::applyDsViewCsvImportPlan(options, plan);

    QFile file(fileName);
    BOOST_REQUIRE(file.open(QIODevice::ReadOnly));

    StreamingInput input("csv", options.toGHashTable());
    input.send(file.readAll());
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_GE(input.metaPackets(), 1);
    BOOST_CHECK_GE(input.logicPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicSamples(), 128);
    BOOST_CHECK_EQUAL(input.sampleLimit(), 128);
    BOOST_CHECK_EQUAL(input.samplerate(), 1000000);
    BOOST_CHECK_EQUAL(input.channelCount(), 8);
    BOOST_CHECK_EQUAL(input.enabledChannelCount(), 8);
    BOOST_CHECK(input.sawEnd());

    sr_input_options_free(definitions);
    QFile::remove(fileName);
}

BOOST_AUTO_TEST_CASE(csv_import_streams_expected_dsview_export_waveform_bytes)
{
    const sr_input_module *module = sr_input_find("csv");
    BOOST_REQUIRE(module != nullptr);

    const sr_option **definitions = sr_input_options_get(module);
    BOOST_REQUIRE(definitions != nullptr);

    pv::data::IoOptions options(definitions);

    const QString fileName = writeFixtureFile(makeDsViewAlternatingCsvFixture(128), ".csv");
    pv::data::CsvImportPlan plan;
    BOOST_REQUIRE(pv::data::estimateDsViewCsvImportPlan(fileName, plan));
    pv::data::applyDsViewCsvImportPlan(options, plan);

    QFile file(fileName);
    BOOST_REQUIRE(file.open(QIODevice::ReadOnly));

    StreamingInput input("csv", options.toGHashTable());
    input.send(file.readAll());
    input.end();

    BOOST_REQUIRE_EQUAL(input.logicSamples(), 128);
    BOOST_REQUIRE_GE(test_input_observer_logic_prefix_len(), 128U);
    const uint8_t *samples = test_input_observer_logic_prefix();
    for (size_t i = 0; i < 128; i++)
        BOOST_CHECK_EQUAL(samples[i], 0xaa);

    sr_input_options_free(definitions);
    QFile::remove(fileName);
}

BOOST_AUTO_TEST_CASE(csv_import_streams_logic_as_cross_data)
{
    const QString fileName = writeFixtureFile(makeDsViewAlternatingCsvFixture(1024), ".csv");

    const sr_input_module *module = sr_input_find("csv");
    BOOST_REQUIRE(module != nullptr);

    const sr_option **definitions = sr_input_options_get(module);
    BOOST_REQUIRE(definitions != nullptr);

    pv::data::IoOptions options(definitions);

    pv::data::CsvImportPlan plan;
    BOOST_REQUIRE(pv::data::estimateDsViewCsvImportPlan(fileName, plan));
    pv::data::applyDsViewCsvImportPlan(options, plan);

    QFile file(fileName);
    BOOST_REQUIRE(file.open(QIODevice::ReadOnly));

    StreamingInput input("csv", options.toGHashTable());
    input.send(file.readAll());
    input.end();

    BOOST_REQUIRE_EQUAL(input.logicSamples(), 1024);
    BOOST_REQUIRE_GE(test_input_observer_logic_prefix_len(), 256U);
    const uint8_t *samples = test_input_observer_logic_prefix();
    for (size_t i = 0; i < 256; i++)
        BOOST_CHECK_EQUAL(samples[i], 0xaa);

    sr_input_options_free(definitions);
    QFile::remove(fileName);
}

BOOST_AUTO_TEST_CASE(saleae_logic1_input_streams_logic_packets)
{
    StreamingInput input("saleae", options({
        {"format", g_variant_new_string("logic2-digital")},
        {"changed", g_variant_new_boolean(FALSE)},
        {"wordsize", g_variant_new_uint32(8)},
        {"samplerate", g_variant_new_uint64(1000000)},
    }));

    input.send(makeSaleaeLogicFixture());
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_GE(input.logicPackets(), 1);
    BOOST_CHECK_GE(input.logicSamples(), 3);
    BOOST_CHECK_EQUAL(input.samplerate(), 1000000);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(protocoldata_uart_input_streams_logic_packets)
{
    StreamingInput input("protocoldata", options({
        {"protocol", g_variant_new_string("uart")},
    }));

    input.send(QByteArray(1, static_cast<char>(0x41)));
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_GE(input.logicPackets(), 1);
    BOOST_CHECK_GT(input.logicSamples(), 0);
    BOOST_CHECK_EQUAL(input.samplerate(), 1000000);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(logicport_scan_buffer_selects_logicport_input_module)
{
    const QByteArray fixture = makeLogicportScanFixture();
    const sr_input *input = nullptr;
    GString *buffer = g_string_new_len(fixture.constData(), fixture.size());

    BOOST_REQUIRE_EQUAL(sr_input_scan_buffer(buffer, &input), SR_OK);
    BOOST_REQUIRE(input != nullptr);
    BOOST_CHECK_EQUAL(std::string(sr_input_module_get(input)->id), "logicport");

    sr_input_free(input);
    g_string_free(buffer, TRUE);
}

BOOST_AUTO_TEST_CASE(chronovu_scan_file_selects_chronovu_input_module)
{
    const sr_input *input = nullptr;
    const QString fileName = createSparseFile((8 * 1024 * 1024) + 5, ".kdt");

    BOOST_REQUIRE_EQUAL(sr_input_scan_file(fileName.toUtf8().constData(), &input), SR_OK);
    BOOST_REQUIRE(input != nullptr);
    BOOST_CHECK_EQUAL(std::string(sr_input_module_get(input)->id), "chronovu-la8");

    sr_input_free(input);
    QFile::remove(fileName);
}

BOOST_AUTO_TEST_CASE(trace32_scan_buffer_selects_trace32_input_module)
{
    const sr_input *input = nullptr;
    GString *buffer = g_string_new_len(makeTrace32HeaderFixture().constData(),
        makeTrace32HeaderFixture().size());

    BOOST_REQUIRE_EQUAL(sr_input_scan_buffer(buffer, &input), SR_OK);
    BOOST_REQUIRE(input != nullptr);
    BOOST_CHECK_EQUAL(std::string(sr_input_module_get(input)->id), "trace32_ad");

    sr_input_free(input);
    g_string_free(buffer, TRUE);
}

BOOST_AUTO_TEST_CASE(trace32_input_streams_logic_packets)
{
    StreamingInput input("trace32_ad");

    input.send(makeTrace32ImportFixture());
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicSamples(), 64);
    BOOST_CHECK_EQUAL(input.samplerate(), 200000000);
    BOOST_CHECK(input.sawEnd());
}

BOOST_AUTO_TEST_CASE(stf_scan_file_selects_stf_input_module)
{
    const sr_input *input = nullptr;
    const QByteArray fixture = makeStfScanFixture();
    const QString fileName = writeFixtureFile(fixture, ".stf");

    BOOST_REQUIRE_EQUAL(sr_input_scan_file(fileName.toUtf8().constData(), &input), SR_OK);
    BOOST_REQUIRE(input != nullptr);
    BOOST_CHECK_EQUAL(std::string(sr_input_module_get(input)->id), "stf");

    sr_input_free(input);
    QFile::remove(fileName);
}

BOOST_AUTO_TEST_CASE(stf_input_streams_logic_packets)
{
    StreamingInput input("stf");

    input.send(makeStfImportFixture());
    input.end();

    BOOST_CHECK_EQUAL(input.headerPackets(), 1);
    BOOST_CHECK_EQUAL(input.metaPackets(), 1);
    BOOST_CHECK_GE(input.logicPackets(), 1);
    BOOST_CHECK_EQUAL(input.logicSamples(), 448);
    BOOST_CHECK_EQUAL(input.samplerate(), 50000000);
    BOOST_CHECK(input.sawEnd());
}
