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

#include <initializer_list>

#include <QByteArray>
#include <QDir>
#include <QTemporaryFile>

extern "C" {
#include "libsigrok-internal.h"

struct sr_dev_inst *make_test_sdi(void);
}

#include "../pv/data/iooptions.h"

namespace {

QByteArray export_logic_fixture(const char *format,
                                std::initializer_list<unsigned char> samples)
{
    const sr_output_module *module = sr_output_find(const_cast<char *>(format));
    BOOST_REQUIRE(module != nullptr);

    const sr_output *output = sr_output_new(module, nullptr, make_test_sdi());
    BOOST_REQUIRE(output != nullptr);

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

    GString *chunk = nullptr;
    BOOST_REQUIRE_EQUAL(sr_output_send(output, &packet, &chunk), SR_OK);

    QByteArray exported;
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

BOOST_AUTO_TEST_SUITE_END()
