/*
 * This file is part of the PXTOOL project.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define BOOST_TEST_MODULE DSViewFormatIntegration
#include <boost/test/included/unit_test.hpp>

#include <array>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "pv/data/inputimporter.h"
#include "pv/data/formatcapability.h"
#include "pv/data/logicsnapshot.h"
#include "pv/sigsession.h"
#include "pv/storesession.h"

namespace {

class HeadlessApplication final {
public:
    HeadlessApplication()
    {
        static int argc = 1;
        static char program_name[] = "DSView-format-integration-test";
        static char *argv[] = {program_name, nullptr};
        application_ = std::make_unique<QCoreApplication>(argc, argv);
    }

private:
    std::unique_ptr<QCoreApplication> application_;
};

HeadlessApplication headless_application;

class HeadlessSessionCallback final : public ISessionCallback {
public:
    void session_error() override {}
    void session_save() override {}
    void data_updated() override { ++data_updated_count; }
    void update_capture() override {}
    void cur_snap_samplerate_changed() override {}
    void signals_changed() override {}
    void receive_trigger(quint64) override {}
    void frame_ended() override {}
    void frame_began() override { ++frame_began_count; }
    void show_region(uint64_t, uint64_t, bool) override {}
    void show_wait_trigger() override {}
    void repeat_hold(int) override {}
    void decode_done() override {}
    void receive_data_len(quint64) override {}
    void receive_header() override { ++header_count; }
    void trigger_message(int) override {}
    void delay_prop_msg(QString) override {}

    unsigned int data_updated_count = 0;
    unsigned int frame_began_count = 0;
    unsigned int header_count = 0;
};

struct CanonicalSession {
    HeadlessSessionCallback callback;
    std::unique_ptr<pv::SigSession> session = std::make_unique<pv::SigSession>();
    pv::data::ImportResult result;

};

std::unique_ptr<CanonicalSession> importCanonicalSession()
{
    auto capture = std::make_unique<CanonicalSession>();
    BOOST_REQUIRE(capture->session->init());
    capture->session->set_callback(&capture->callback);
    capture->session->set_as_current();

    const QString file_name = QStringLiteral(DSVIEW_FORMAT_FIXTURE_PATH);
    pv::data::CsvImportPlan plan;
    BOOST_REQUIRE(pv::data::estimateDsViewCsvImportPlan(file_name, plan));

    const sr_input_module *module = sr_input_find("csv");
    BOOST_REQUIRE(module != nullptr);
    const sr_option **definitions = sr_input_options_get(module);
    BOOST_REQUIRE(definitions != nullptr);
    pv::data::IoOptions options(definitions);
    pv::data::applyDsViewCsvImportPlan(options, plan);
    sr_input_options_free(definitions);

    capture->result = pv::data::InputImporter::importFile(
        *capture->session, QStringLiteral("csv"), file_name, options);
    return capture;
}

std::unique_ptr<CanonicalSession> importGenerated(const QString &format_id,
                                                   const QString &path)
{
    auto capture = std::make_unique<CanonicalSession>();
    capture->session->set_callback(&capture->callback);
    capture->session->set_as_current();
    QMap<QString, QVariant> values;
    values.insert(QStringLiteral("numchannels"), 8);
    values.insert(QStringLiteral("samplerate"), QVariant::fromValue<qulonglong>(1000000));
    capture->result = pv::data::InputImporter::importFile(
        *capture->session, format_id, path, pv::data::IoOptions::fromValues(values));
    return capture;
}

CanonicalSession &canonicalSession()
{
    static std::unique_ptr<CanonicalSession> capture = importCanonicalSession();
    BOOST_REQUIRE_MESSAGE(capture->result.ok, capture->result.error.toStdString());
    return *capture;
}

void assertCanonicalLogic(pv::SigSession &session)
{
    BOOST_CHECK_EQUAL(session.cur_samplerate(), 1000000ULL);
    BOOST_CHECK_EQUAL(session.cur_samplelimits(), 1024ULL);

    auto *snapshot = dynamic_cast<pv::data::LogicSnapshot *>(
        session.get_snapshot(SR_CHANNEL_LOGIC));
    BOOST_REQUIRE(snapshot != nullptr);
    BOOST_CHECK_EQUAL(snapshot->get_sample_count(), 1024ULL);

    const auto &session_signals = session.get_signals();
    BOOST_CHECK_EQUAL(session_signals.size(), 8U);
    for (int channel = 0; channel < 8; ++channel) {
        BOOST_CHECK_EQUAL(snapshot->get_sample(0, channel), false);
        BOOST_CHECK_EQUAL(snapshot->get_sample(1, channel), true);
    }
}

QString exportCanonical(const QString &format_id, QTemporaryDir &temporary)
{
    auto &source = canonicalSession();
    const auto formats = pv::data::exportFormats();
    const auto *format = pv::data::findFormatById(formats, format_id);
    BOOST_REQUIRE(format != nullptr);

    const QString suffix = format->extensions.isEmpty()
        ? format_id
        : format->extensions.first();
    const QString path = temporary.filePath(QStringLiteral("canonical.") + suffix);
    pv::StoreSession store(source.session.get());
    store.setSelectedOutputFormatId(format_id);
    store.setOutputFileName(path);
    BOOST_REQUIRE_MESSAGE(store.export_start(), store.error().toStdString());
    store.wait();
    BOOST_REQUIRE_MESSAGE(store.error().isEmpty(), store.error().toStdString());
    BOOST_REQUIRE(QFileInfo(path).size() > 0);
    return path;
}

} // namespace

BOOST_AUTO_TEST_CASE(store_session_uses_explicit_output_file_name)
{
    pv::StoreSession store(nullptr);
    QTemporaryDir temporary;
    BOOST_REQUIRE(temporary.isValid());

    const QString path = temporary.filePath(QStringLiteral("capture.vcd"));
    store.setOutputFileName(path);

    BOOST_CHECK_EQUAL(store.GetFileName().toStdString(), path.toStdString());
}

BOOST_AUTO_TEST_CASE(canonical_csv_imports_as_expected_logic_capture)
{
    auto &capture = canonicalSession();
    assertCanonicalLogic(*capture.session);
}

BOOST_AUTO_TEST_CASE(binary_export_writes_canonical_capture)
{
    QTemporaryDir temporary;
    BOOST_REQUIRE(temporary.isValid());
    const QString path = exportCanonical(QStringLiteral("binary"), temporary);
    BOOST_CHECK_EQUAL(QFileInfo(path).size(), 1024);
}

BOOST_AUTO_TEST_CASE(binary_export_import_round_trips_canonical_logic)
{
    QTemporaryDir temporary;
    BOOST_REQUIRE(temporary.isValid());
    const QString path = exportCanonical(QStringLiteral("binary"), temporary);
    QFile raw(path);
    BOOST_REQUIRE(raw.open(QIODevice::ReadOnly));
    const QByteArray bytes = raw.read(4);
    BOOST_TEST(static_cast<unsigned int>(static_cast<unsigned char>(bytes[0])) == 0U);
    BOOST_TEST(static_cast<unsigned int>(static_cast<unsigned char>(bytes[1])) == 0xffU);
    const auto imported = importGenerated(QStringLiteral("binary"), path);
    BOOST_REQUIRE_MESSAGE(imported->result.ok, imported->result.error.toStdString());
    BOOST_CHECK_EQUAL(imported->callback.header_count, 1U);
    BOOST_CHECK_EQUAL(imported->callback.frame_began_count, 1U);
    assertCanonicalLogic(*imported->session);
}

BOOST_AUTO_TEST_CASE(chronovu_export_writes_full_la8_container)
{
    QTemporaryDir temporary;
    BOOST_REQUIRE(temporary.isValid());
    const QString path = exportCanonical(QStringLiteral("chronovu-la8"), temporary);
    BOOST_CHECK_EQUAL(QFileInfo(path).size(), 8 * 1024 * 1024 + 5);
    QFile file(path);
    BOOST_REQUIRE(file.open(QIODevice::ReadOnly));
    BOOST_REQUIRE(file.seek(8 * 1024 * 1024));
    const QByteArray header = file.read(5);
    BOOST_REQUIRE_EQUAL(header.size(), 5);
    BOOST_CHECK_EQUAL(static_cast<unsigned char>(header[0]), 99U);
    BOOST_CHECK(header.mid(1) == QByteArray(4, '\0'));
}

BOOST_AUTO_TEST_CASE(chronovu_export_import_preserves_canonical_logic_prefix)
{
    QTemporaryDir temporary;
    BOOST_REQUIRE(temporary.isValid());
    const QString path = exportCanonical(QStringLiteral("chronovu-la8"), temporary);
    const auto imported = importGenerated(QStringLiteral("chronovu-la8"), path);
    BOOST_REQUIRE_MESSAGE(imported->result.ok, imported->result.error.toStdString());

    auto *snapshot = dynamic_cast<pv::data::LogicSnapshot *>(
        imported->session->get_snapshot(SR_CHANNEL_LOGIC));
    BOOST_REQUIRE(snapshot != nullptr);
    BOOST_CHECK_EQUAL(imported->session->cur_samplerate(), 1000000ULL);
    BOOST_CHECK_EQUAL(imported->session->cur_samplelimits(), 8ULL * 1024 * 1024);
    BOOST_CHECK_EQUAL(snapshot->get_sample_count(), 8ULL * 1024 * 1024);
    for (int channel = 0; channel < 8; ++channel) {
        BOOST_CHECK_EQUAL(snapshot->get_sample(0, channel), false);
        BOOST_CHECK_EQUAL(snapshot->get_sample(1, channel), true);
        BOOST_CHECK_EQUAL(snapshot->get_sample(1023, channel), true);
        BOOST_CHECK_EQUAL(snapshot->get_sample(1024, channel), false);
    }
}

BOOST_AUTO_TEST_CASE(logic_export_only_formats_write_artifacts)
{
    const std::array<QString, 7> formats = {
        QStringLiteral("ascii"), QStringLiteral("bits"), QStringLiteral("hex"),
        QStringLiteral("gnuplot"), QStringLiteral("wavedrom"),
        QStringLiteral("ols"), QStringLiteral("srzip")};
    for (const QString &format_id : formats) {
        QTemporaryDir temporary;
        BOOST_REQUIRE(temporary.isValid());
        const QString path = exportCanonical(format_id, temporary);
        QFile file(path);
        BOOST_REQUIRE(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        BOOST_REQUIRE(!bytes.isEmpty());
        if (format_id == QStringLiteral("srzip"))
            BOOST_CHECK(bytes.startsWith("PK\x03\x04"));
        else if (format_id == QStringLiteral("wavedrom"))
            BOOST_CHECK(bytes.contains("signal"));
        else if (format_id == QStringLiteral("gnuplot"))
            BOOST_CHECK(bytes.contains("#"));
        else
            BOOST_CHECK(bytes.contains("0") || bytes.contains("1"));
    }
}
