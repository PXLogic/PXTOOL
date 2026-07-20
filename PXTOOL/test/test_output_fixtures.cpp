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

    BOOST_CHECK_EQUAL(sr_output_free(output), SR_OK);
    return exported;
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

BOOST_AUTO_TEST_CASE(null_output_writes_no_payload)
{
    BOOST_CHECK(export_logic_fixture("null", {0x00, 0x01}).isEmpty());
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
