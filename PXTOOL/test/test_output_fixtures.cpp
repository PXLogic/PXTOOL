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

#include <cstring>
#include <initializer_list>

#include <QByteArray>
#include <QDir>
#include <QTemporaryFile>
#include <glib.h>

extern "C" {
#include "libsigrok-internal.h"

struct sr_dev_inst *make_test_sdi(void);
struct sr_dev_inst *make_test_sdi_with_samplerate(uint64_t samplerate);
}

#include "../pv/data/iooptions.h"
#include "../pv/data/analogpacketadapter.h"

namespace {

QByteArray export_logic_fixture(const char *format,
                                std::initializer_list<unsigned char> samples,
                                const sr_dev_inst *sdi = make_test_sdi())
{
    const sr_output_module *module = sr_output_find(const_cast<char *>(format));
    BOOST_REQUIRE(module != nullptr);

    const sr_output *output = sr_output_new(module, nullptr, sdi);
    BOOST_REQUIRE(output != nullptr);

    sr_datafeed_header header{};
    header.feed_version = 1;

    sr_datafeed_packet header_packet{};
    header_packet.type = SR_DF_HEADER;
    header_packet.status = SR_PKT_OK;
    header_packet.payload = &header;

    GString *chunk = nullptr;
    BOOST_REQUIRE_EQUAL(sr_output_send(output, &header_packet, &chunk), SR_OK);

    QByteArray exported;
    if (chunk) {
        exported.append(chunk->str, static_cast<qsizetype>(chunk->len));
        g_string_free(chunk, TRUE);
    }

    QByteArray source(reinterpret_cast<const char *>(samples.begin()),
                      static_cast<qsizetype>(samples.size()));
    sr_datafeed_logic logic{};
    logic.data = reinterpret_cast<uint8_t *>(source.data());
    logic.length = static_cast<uint64_t>(source.size());
    logic.unitsize = 1;

    sr_datafeed_packet packet{};
    packet.type = SR_DF_LOGIC;
    packet.status = SR_PKT_OK;
    packet.payload = &logic;

    chunk = nullptr;
    BOOST_REQUIRE_EQUAL(sr_output_send(output, &packet, &chunk), SR_OK);

    if (chunk) {
        exported.append(chunk->str, static_cast<qsizetype>(chunk->len));
        g_string_free(chunk, TRUE);
    }

    sr_datafeed_packet end_packet{};
    end_packet.type = SR_DF_END;
    end_packet.status = SR_PKT_OK;
    BOOST_REQUIRE_EQUAL(sr_output_send(output, &end_packet, &chunk), SR_OK);
    if (chunk) {
        exported.append(chunk->str, static_cast<qsizetype>(chunk->len));
        g_string_free(chunk, TRUE);
    }

    BOOST_CHECK_EQUAL(sr_output_free(output), SR_OK);
    return exported;
}

QByteArray send_output_packet(const sr_output *output,
                              const sr_datafeed_packet &packet)
{
    GString *chunk = nullptr;
    BOOST_REQUIRE_EQUAL(sr_output_send(output, &packet, &chunk), SR_OK);

    QByteArray exported;
    if (chunk) {
        exported.append(chunk->str, static_cast<qsizetype>(chunk->len));
        g_string_free(chunk, TRUE);
    }

    return exported;
}

void send_samplerate_meta(const sr_output *output, uint64_t samplerate)
{
    sr_config config{};
    config.key = SR_CONF_SAMPLERATE;
    config.data = g_variant_ref_sink(g_variant_new_uint64(samplerate));

    sr_datafeed_meta meta{};
    meta.config = g_slist_append(nullptr, &config);

    sr_datafeed_packet packet{};
    packet.type = SR_DF_META;
    packet.status = SR_PKT_OK;
    packet.payload = &meta;

    send_output_packet(output, packet);
    g_slist_free(meta.config);
    g_variant_unref(config.data);
}

QByteArray export_analog_fixture(const char *format)
{
    sr_channel channel0{};
    channel0.index = 0;
    channel0.type = SR_CHANNEL_ANALOG;
    channel0.enabled = TRUE;
    channel0.name = const_cast<char *>("A0");

    sr_channel channel1{};
    channel1.index = 1;
    channel1.type = SR_CHANNEL_ANALOG;
    channel1.enabled = TRUE;
    channel1.name = const_cast<char *>("A1");

    sr_dev_inst sdi{};
    channel0.sdi = &sdi;
    channel1.sdi = &sdi;
    sdi.channels = g_slist_append(sdi.channels, &channel0);
    sdi.channels = g_slist_append(sdi.channels, &channel1);

    std::vector<float> samples;
    for (int index = 0; index < 12; ++index) {
        samples.push_back(static_cast<float>(index) / 12.0F);
        samples.push_back(-static_cast<float>(index) / 12.0F);
    }
    auto analog = pv::data::makeAnalogPacket(
        {pv::data::AnalogChannelRef(&channel0),
         pv::data::AnalogChannelRef(&channel1)},
        samples, 12, 48'000, SR_MQ_VOLTAGE, SR_UNIT_VOLT);

    const sr_output_module *module = sr_output_find(const_cast<char *>(format));
    BOOST_REQUIRE(module != nullptr);
    const sr_output *output = sr_output_new(module, nullptr, &sdi);
    BOOST_REQUIRE(output != nullptr);

    QByteArray exported;
    sr_datafeed_packet packet{};
    packet.status = SR_PKT_OK;

    sr_datafeed_header header{};
    header.feed_version = 1;
    packet.type = SR_DF_HEADER;
    packet.payload = &header;
    exported += send_output_packet(output, packet);

    packet.type = SR_DF_FRAME_BEGIN;
    packet.payload = nullptr;
    exported += send_output_packet(output, packet);

    packet.type = SR_DF_META;
    packet.payload = &analog.meta;
    exported += send_output_packet(output, packet);

    exported += send_output_packet(output, analog.packet);

    packet.type = SR_DF_FRAME_END;
    packet.payload = nullptr;
    exported += send_output_packet(output, packet);

    packet.type = SR_DF_END;
    exported += send_output_packet(output, packet);

    BOOST_CHECK_EQUAL(sr_output_free(output), SR_OK);
    g_slist_free(sdi.channels);
    return exported;
}

quint16 read_le16(const QByteArray &bytes, int offset)
{
    BOOST_REQUIRE_GE(bytes.size(), offset + 2);
    return static_cast<quint16>(
        static_cast<unsigned char>(bytes.at(offset)) |
        (static_cast<unsigned char>(bytes.at(offset + 1)) << 8));
}

quint32 read_le32(const QByteArray &bytes, int offset)
{
    BOOST_REQUIRE_GE(bytes.size(), offset + 4);
    return static_cast<quint32>(
        static_cast<unsigned char>(bytes.at(offset)) |
        (static_cast<unsigned char>(bytes.at(offset + 1)) << 8) |
        (static_cast<unsigned char>(bytes.at(offset + 2)) << 16) |
        (static_cast<unsigned char>(bytes.at(offset + 3)) << 24));
}

float read_le_float(const QByteArray &bytes, int offset)
{
    const quint32 bits = read_le32(bytes, offset);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void verify_wav_fixture(const QByteArray &wav)
{
    const int headerSize = 46;
    const int channelCount = 2;
    const int sampleRate = 48000;
    const int samplesPerChannel = 12;

    BOOST_REQUIRE_GE(wav.size(), headerSize);
    BOOST_CHECK(wav.startsWith("RIFF"));
    BOOST_CHECK_EQUAL(read_le32(wav, 4), 0xffffffffU);
    BOOST_CHECK_EQUAL(wav.mid(8, 4).toStdString(), "WAVE");
    BOOST_CHECK_EQUAL(wav.mid(12, 4).toStdString(), "fmt ");
    BOOST_CHECK_EQUAL(read_le32(wav, 16), 0x12U);
    BOOST_CHECK_EQUAL(read_le16(wav, 20), 0x0003U);
    BOOST_CHECK_EQUAL(read_le16(wav, 22), channelCount);
    BOOST_CHECK_EQUAL(read_le32(wav, 24), sampleRate);
    BOOST_CHECK_EQUAL(read_le32(wav, 28), sampleRate * channelCount * 4);
    BOOST_CHECK_EQUAL(read_le16(wav, 32), channelCount * 4);
    BOOST_CHECK_EQUAL(read_le16(wav, 34), 32);
    BOOST_CHECK_EQUAL(read_le16(wav, 36), 0);
    BOOST_CHECK_EQUAL(wav.mid(38, 4).toStdString(), "data");
    BOOST_CHECK_EQUAL(read_le32(wav, 42), 0xffffffffU);
    BOOST_REQUIRE_EQUAL(wav.size(),
                        headerSize + samplesPerChannel * channelCount * 4);

    for (int sample = 0; sample < samplesPerChannel; ++sample) {
        const float positive = static_cast<float>(sample) / 12.0F;
        const float negative = -static_cast<float>(sample) / 12.0F;
        const int offset = headerSize + sample * channelCount * 4;
        BOOST_CHECK_CLOSE(read_le_float(wav, offset), positive, 0.001);
        BOOST_CHECK_CLOSE(read_le_float(wav, offset + 4), negative, 0.001);
    }
}

} // namespace

BOOST_AUTO_TEST_SUITE(io_migration_output_fixtures)

BOOST_AUTO_TEST_CASE(binary_output_preserves_nul_bytes)
{
    const QByteArray bytes = export_logic_fixture(
        "binary", {0x00, 0x7f, 0x80, 0xff});
    BOOST_REQUIRE_EQUAL(bytes.size(), 4);
    BOOST_CHECK_EQUAL(static_cast<unsigned char>(bytes.at(0)), 0x00);
    BOOST_CHECK_EQUAL(static_cast<unsigned char>(bytes.at(3)), 0xff);
}

BOOST_AUTO_TEST_CASE(chronovu_output_writes_fixed_size_la8_container)
{
    const QByteArray source("\x00\x7f\x80\xff", 4);
    const QByteArray exported = export_logic_fixture(
        "chronovu-la8", {0x00, 0x7f, 0x80, 0xff},
        make_test_sdi_with_samplerate(50000000));

    const int data_size = 8 * 1024 * 1024;
    BOOST_REQUIRE_EQUAL(exported.size(), data_size + 5);
    BOOST_CHECK(exported.startsWith(source));
    BOOST_CHECK_EQUAL(static_cast<unsigned char>(exported.at(source.size())), 0x00);
    BOOST_CHECK_EQUAL(static_cast<unsigned char>(exported.at(data_size)), 0x01);
    BOOST_CHECK_EQUAL(read_le32(exported, data_size + 1), 0U);
}

BOOST_AUTO_TEST_CASE(exports_logic_formats)
{
    struct LogicOutputExpectation {
        const char *id;
        QByteArray prefix;
        QString suffix;
    };

    const LogicOutputExpectation expectations[] = {
        {"ascii", QByteArray(""), "txt"},
        {"binary", QByteArray("\x00\x01\x03", 3), ""},
        {"bits", QByteArray(""), "txt"},
        {"chronovu-la8", QByteArray(), "kdt"},
        {"hex", QByteArray(""), "txt"},
        {"ols", QByteArray(), "ols"},
        {"vcd", QByteArray("$date"), "vcd"},
        {"wavedrom", QByteArray("{"), "wavedrom"},
    };

    for (const LogicOutputExpectation &expectation : expectations) {
        const sr_output_module *module =
            sr_output_find(const_cast<char *>(expectation.id));
        BOOST_REQUIRE_MESSAGE(module != nullptr,
            "Missing output module: " << expectation.id);

        const char *const *extensions = sr_output_extensions_get(module);
        if (expectation.suffix.isEmpty()) {
            BOOST_CHECK(extensions == nullptr);
        } else {
            BOOST_REQUIRE(extensions != nullptr);
            BOOST_REQUIRE(extensions[0] != nullptr);
            BOOST_CHECK_EQUAL(QString::fromUtf8(extensions[0]).toStdString(),
                              expectation.suffix.toStdString());
        }

        const QByteArray exported = export_logic_fixture(
            expectation.id, {0x00, 0x01, 0x03});
        QString file_template = QDir::tempPath() + "/dsview-output-XXXXXX";
        if (!expectation.suffix.isEmpty())
            file_template += "." + expectation.suffix;
        QTemporaryFile file(file_template);
        BOOST_REQUIRE(file.open());
        BOOST_REQUIRE_EQUAL(file.write(exported), exported.size());
        file.flush();
        BOOST_CHECK(file.exists());
        if (!expectation.suffix.isEmpty())
            BOOST_CHECK(file.fileName().endsWith("." + expectation.suffix));

        if (!expectation.prefix.isEmpty())
            BOOST_CHECK(exported.startsWith(expectation.prefix));
    }
}

BOOST_AUTO_TEST_CASE(null_output_writes_no_payload)
{
    BOOST_CHECK(export_logic_fixture("null", {0x00, 0x01}).isEmpty());
}

BOOST_AUTO_TEST_CASE(vcd_output_writes_final_timestamp_on_end)
{
    const sr_output_module *module = sr_output_find(const_cast<char *>("vcd"));
    BOOST_REQUIRE(module != nullptr);

    const sr_output *output = sr_output_new(module, nullptr, make_test_sdi());
    BOOST_REQUIRE(output != nullptr);

    send_samplerate_meta(output, 1000000);

    uint8_t sample = 0x01;
    sr_datafeed_logic logic{};
    logic.data = &sample;
    logic.length = 1;
    logic.unitsize = 1;

    sr_datafeed_packet logic_packet{};
    logic_packet.type = SR_DF_LOGIC;
    logic_packet.status = SR_PKT_OK;
    logic_packet.payload = &logic;
    send_output_packet(output, logic_packet);

    sr_datafeed_packet end_packet{};
    end_packet.type = SR_DF_END;
    end_packet.status = SR_PKT_OK;
    const QByteArray end = send_output_packet(output, end_packet);

    BOOST_CHECK_EQUAL(end.toStdString(), "#1\n");
    BOOST_CHECK_EQUAL(sr_output_free(output), SR_OK);
}

BOOST_AUTO_TEST_CASE(csv_selected_range_uses_requested_start_sample_index)
{
    const sr_output_module *module = sr_output_find(const_cast<char *>("csv"));
    BOOST_REQUIRE(module != nullptr);

    const sr_option **definitions = sr_output_options_get(module);
    pv::data::IoOptions options(definitions);
    options.set("type",
        QVariant::fromValue(static_cast<qint16>(SR_CHANNEL_LOGIC)));
    GHashTable *values = options.toGHashTable();
    sr_output_options_free(definitions);

    const sr_output *output = sr_output_new_with_start_sample_index(
        module, values, make_test_sdi(), 5);
    g_hash_table_destroy(values);
    BOOST_REQUIRE(output != nullptr);

    send_samplerate_meta(output, 1000000);

    uint8_t sample = 0x01;
    sr_datafeed_logic logic{};
    logic.data = &sample;
    logic.length = 1;
    logic.unitsize = 1;

    sr_datafeed_packet packet{};
    packet.type = SR_DF_LOGIC;
    packet.status = SR_PKT_OK;
    packet.payload = &logic;
    const QByteArray csv = send_output_packet(output, packet);

    BOOST_CHECK(csv.contains("5e-06,1\n"));
    BOOST_CHECK_EQUAL(sr_output_free(output), SR_OK);
}

BOOST_AUTO_TEST_CASE(csv_output_declares_typed_channel_type_option)
{
    const sr_output_module *module = sr_output_find(const_cast<char *>("csv"));
    BOOST_REQUIRE(module != nullptr);

    const sr_option **options = sr_output_options_get(module);
    BOOST_REQUIRE(options != nullptr);
    BOOST_REQUIRE(options[0] != nullptr);
    BOOST_CHECK_EQUAL(std::string(options[0]->id), "type");
    BOOST_CHECK(g_variant_is_of_type(options[0]->def, G_VARIANT_TYPE_INT16));
    sr_output_options_free(options);
}

BOOST_AUTO_TEST_CASE(srzip_output_accepts_io_options_filename)
{
    const sr_output_module *module = sr_output_find(const_cast<char *>("srzip"));
    BOOST_REQUIRE(module != nullptr);

    const sr_option **definitions = sr_output_options_get(module);
    pv::data::IoOptions options(definitions);
    options.set("filename", QStringLiteral("/tmp/dsview-output-fixture.sr"));
    GHashTable *values = options.toGHashTable();
    sr_output_options_free(definitions);

    const sr_output *output = sr_output_new(module, values, make_test_sdi());
    g_hash_table_destroy(values);
    BOOST_REQUIRE(output != nullptr);
    BOOST_CHECK_EQUAL(sr_output_free(output), SR_OK);
}

BOOST_AUTO_TEST_CASE(analog_outputs_accept_standard_packets)
{
    const sr_output_module *analog_module =
        sr_output_find(const_cast<char *>("analog"));
    const sr_output_module *wav_module =
        sr_output_find(const_cast<char *>("wav"));
    BOOST_REQUIRE(analog_module != nullptr);
    BOOST_REQUIRE(wav_module != nullptr);
    BOOST_CHECK(sr_output_extensions_get(analog_module) == nullptr);
    BOOST_REQUIRE(sr_output_extensions_get(wav_module) != nullptr);
    BOOST_CHECK_EQUAL(std::string(sr_output_extensions_get(wav_module)[0]),
                      "wav");

    const sr_option **analog_options = sr_output_options_get(analog_module);
    const sr_option **wav_options = sr_output_options_get(wav_module);
    BOOST_REQUIRE(analog_options != nullptr);
    BOOST_REQUIRE(wav_options != nullptr);
    BOOST_CHECK_EQUAL(std::string(analog_options[0]->id), "digits");
    BOOST_CHECK(g_variant_is_of_type(analog_options[0]->def,
                                     G_VARIANT_TYPE_STRING));
    BOOST_CHECK_EQUAL(std::string(wav_options[0]->id), "scale");
    BOOST_CHECK(g_variant_is_of_type(wav_options[0]->def,
                                     G_VARIANT_TYPE_DOUBLE));
    sr_output_options_free(analog_options);
    sr_output_options_free(wav_options);

    const QByteArray analog = export_analog_fixture("analog");
    const QByteArray wav = export_analog_fixture("wav");
    BOOST_CHECK(analog.startsWith("FRAME-BEGIN"));
    BOOST_CHECK(analog.contains("META samplerate: 48000"));
    verify_wav_fixture(wav);

    QTemporaryFile analog_file(
        QDir::tempPath() + "/dsview-analog-output-XXXXXX");
    QTemporaryFile wav_file(
        QDir::tempPath() + "/dsview-analog-output-XXXXXX.wav");
    BOOST_REQUIRE(analog_file.open());
    BOOST_REQUIRE(wav_file.open());
    BOOST_REQUIRE_EQUAL(analog_file.write(analog), analog.size());
    BOOST_REQUIRE_EQUAL(wav_file.write(wav), wav.size());
    BOOST_CHECK(!analog_file.fileName().endsWith(".analog"));
    BOOST_CHECK(wav_file.fileName().endsWith(".wav"));
}

BOOST_AUTO_TEST_CASE(wav_output_recreates_options_after_cleanup)
{
    const sr_output_module *module = sr_output_find(const_cast<char *>("wav"));
    BOOST_REQUIRE(module != nullptr);

    const sr_output *first_output = sr_output_new(module, nullptr, make_test_sdi());
    BOOST_REQUIRE(first_output != nullptr);
    BOOST_CHECK_EQUAL(sr_output_free(first_output), SR_OK);

    const sr_option **options = sr_output_options_get(module);
    BOOST_REQUIRE(options != nullptr);
    BOOST_REQUIRE(options[0] != nullptr);
    BOOST_CHECK_EQUAL(std::string(options[0]->id), "scale");
    BOOST_REQUIRE(options[0]->def != nullptr);
    BOOST_CHECK(g_variant_is_of_type(options[0]->def, G_VARIANT_TYPE_DOUBLE));
    BOOST_CHECK_CLOSE(g_variant_get_double(options[0]->def), 1.0, 0.001);
    sr_output_options_free(options);

    const QByteArray wav = export_analog_fixture("wav");
    verify_wav_fixture(wav);
}

BOOST_AUTO_TEST_SUITE_END()
