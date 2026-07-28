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
#include <cstring>
#include <vector>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QTemporaryDir>

#include <zlib.h>

#include "pv/ZipMaker.h"
#include "pv/data/inputimporter.h"
#include "pv/data/formatcapability.h"
#include "pv/data/logicsnapshot.h"
#include "pv/sigsession.h"
#include "pv/storesession.h"

extern "C" {
#include "minilzo/minilzo.h"
}

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
    void frame_ended() override { ++frame_ended_count; }
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
    unsigned int frame_ended_count = 0;
};

class TestSessionDataGetter final : public ISessionDataGetter {
public:
    bool genSessionData(std::string &data) override
    {
        data = "{}";
        return true;
    }
};

struct CanonicalSession {
    HeadlessSessionCallback callback;
    std::unique_ptr<pv::SigSession> session = std::make_unique<pv::SigSession>();
    pv::data::ImportResult result;

};

CanonicalSession &canonicalSession();

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
    (void)canonicalSession();
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

std::unique_ptr<CanonicalSession> importGenerated(const QString &format_id,
                                                   const QString &path,
                                                   QMap<QString, QVariant> values)
{
    (void)canonicalSession();
    auto capture = std::make_unique<CanonicalSession>();
    capture->session->set_callback(&capture->callback);
    capture->session->set_as_current();
    capture->result = pv::data::InputImporter::importFile(
        *capture->session, format_id, path, pv::data::IoOptions::fromValues(values));
    return capture;
}

QString writeFixture(QTemporaryDir &temporary, const QString &name,
                     const QByteArray &bytes)
{
    const QString path = temporary.filePath(name);
    QFile file(path);
    BOOST_REQUIRE(file.open(QIODevice::WriteOnly));
    BOOST_REQUIRE_EQUAL(file.write(bytes), bytes.size());
    return path;
}

QString writeMsoArchive(QTemporaryDir &temporary, const QString &name)
{
    const QString path = temporary.filePath(name);
    const QByteArray header =
        "[version]\n"
        "version = 3\n"
        "[header]\n"
        "device mode = 3\n"
        "capturefile = data\n"
        "total samples = 64\n"
        "mso logic samples = 64\n"
        "mso logic probes = 1\n"
        "mso logic blocks = 1\n"
        "mso analog samples = 4\n"
        "mso analog probes = 1\n"
        "mso analog blocks = 1\n"
        "mso analog bits = 16\n"
        "samplerate = 1 MHz\n"
        "logic probe0 = D0\n"
        "analog probe0 = A0\n";
    const QByteArray logic(8, static_cast<char>(0x55));
    const QByteArray analog = QByteArray::fromHex("34127856bc9af0de");
    const QByteArray session("{}");
    const QByteArray decoders("[]");

    ZipMaker archive;
    const std::string native_path = path.toUtf8().toStdString();
    BOOST_REQUIRE(archive.CreateNew(native_path.c_str(), false));
    BOOST_REQUIRE(archive.AddFromBuffer("header", header.constData(), header.size()));
    BOOST_REQUIRE(archive.AddFromBuffer("session", session.constData(), session.size()));
    BOOST_REQUIRE(archive.AddFromBuffer("decoders", decoders.constData(), decoders.size()));
    BOOST_REQUIRE(archive.AddFromBuffer("L-0/0", logic.constData(), logic.size()));
    BOOST_REQUIRE(archive.AddFromBuffer("A-0/0", analog.constData(), analog.size()));
    BOOST_REQUIRE(archive.Close());
    return path;
}

ds_device_handle addMsoArchiveDevice(const QString &path)
{
    BOOST_REQUIRE_EQUAL(ds_device_from_file(path.toUtf8().constData()), SR_OK);
    ds_device_base_info *devices = nullptr;
    int count = 0;
    BOOST_REQUIRE_EQUAL(ds_get_device_list(&devices, &count), SR_OK);
    BOOST_REQUIRE(devices != nullptr);
    BOOST_REQUIRE(count > 0);
    const ds_device_handle handle = devices[count - 1].handle;
    free(devices);
    return handle;
}

void waitForCaptureEnd(HeadlessSessionCallback &callback)
{
    for (int attempt = 0; attempt < 100 && callback.frame_ended_count == 0; ++attempt) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    BOOST_REQUIRE_EQUAL(callback.frame_ended_count, 1U);
}

void appendU32(QByteArray &data, uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        data.append(static_cast<char>((value >> shift) & 0xff));
}

void appendU64(QByteArray &data, uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8)
        data.append(static_cast<char>((value >> shift) & 0xff));
}

void appendDouble(QByteArray &data, double value)
{
    uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "double must be 64-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    appendU64(data, bits);
}

QByteArray saleaeFixture()
{
    QByteArray data("<SALEAE>");
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

QByteArray isfFixture()
{
    QByteArray data;
    data.append("NR_PT 3;YOFF 0;YZERO 0;YMULT 1;XINCR 0.5;");
    data.append("BYT_NR 1;BYT_OR LSB;BN_FMT RI;ENCDG BIN;");
    data.append("WFID \"CH1,DC\";CURVE #13");
    data.append(QByteArray::fromHex("000102"));
    return data;
}

QByteArray trace32Fixture()
{
    QByteArray data(0x50, '\0');
    const QByteArray name("trace32 power integrator data");
    std::copy(name.begin(), name.end(), data.begin());
    data[0x20] = 0x01;
    data[0x36] = 0x08;
    data[0x38] = 28;
    data[0x3c] = 0x02;
    data[0x40] = 0x02;
    QByteArray first(28, '\0');
    QByteArray second(28, '\0');
    first[0x08] = 0x01;
    second[0x00] = 0x01;
    data.append(first);
    data.append(second);
    return data;
}

QByteArray stfFixture()
{
    QByteArray raw;
    QByteArray compressed(1600, '\0');
    lzo_uint compressed_size = compressed.size();
    std::vector<unsigned char> workmem(LZO1X_1_MEM_COMPRESS);

    appendU32(raw, 0); appendU32(raw, 1); appendU64(raw, 0);
    appendU64(raw, 447); appendU64(raw, 448);
    for (int cluster = 0; cluster < 64; ++cluster)
        appendU64(raw, static_cast<uint64_t>(cluster * 7));
    for (int cluster = 0; cluster < 64; ++cluster)
        for (int sample = 0; sample < 7; ++sample) {
            const uint16_t value = ((cluster * 7 + sample) & 1) ? 1 : 0;
            raw.append(static_cast<char>(value & 0xff));
            raw.append(static_cast<char>(value >> 8));
        }
    BOOST_REQUIRE_EQUAL(raw.size(), 1440);
    BOOST_REQUIRE_EQUAL(lzo1x_1_compress(
        reinterpret_cast<const unsigned char *>(raw.constData()), raw.size(),
        reinterpret_cast<unsigned char *>(compressed.data()), &compressed_size,
        workmem.data()), 0);
    compressed.resize(static_cast<int>(compressed_size));

    QByteArray data("Sigma Test File", 16);
    data.append("TestFirstTS=0\r\nTestLengthTS=447\r\n");
    data.append("Sigma.ClockSource=ClockScheme=0;Period=1\r\n");
    data.append("Sigma.SigmaInputs=A0\r\n");
    data.append("Traces.Traces=Type=Input:Caption=D0:Input0=0\r\n");
    data.append('\0');
    appendU32(data, static_cast<uint32_t>(compressed.size()));
    appendU32(data, crc32(0, reinterpret_cast<const Bytef *>(compressed.constData()), compressed.size()));
    data.append(compressed);
    appendU32(data, 0xffffffffu); appendU32(data, 0);
    return data;
}

QByteArray logicportFixture()
{
    const char dc1 = '\x11';
    QByteArray data;
    data += "Version"; data += dc1; data += "1.2"; data += dc1;
    data += "123"; data += dc1;
    data += " CAUTION: Do not change the contents of this file.\r\n";
    data += "SampleData"; data += dc1; data += "1"; data += dc1; data += "2\r\n";
    data += "{\r\nD0,Count\r\n0,1\r\n1,1\r\n}\r\n";
    data += "AcquiredSamplePeriod"; data += dc1; data += "1e-6\r\n";
    data += "AcquiredChannelList"; data += dc1; data += "True\r\n";
    data += "InvertedChannelList"; data += dc1; data += "False\r\n";
    data += "Signals"; data += dc1; data += "D0\r\n";
    data += "NotesString/"; data += dc1; data += "\r\n";
    data += dc1; data += "/\r\n";
    return data;
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

BOOST_AUTO_TEST_CASE(mso_archive_round_trip_replays_logic_and_analog)
{
    auto &canonical = canonicalSession();
    struct RestoreCurrentSession {
        pv::SigSession *session;
        ~RestoreCurrentSession() { session->set_as_current(); }
    } restore_current{canonical.session.get()};
    QTemporaryDir temporary;
    BOOST_REQUIRE(temporary.isValid());

    HeadlessSessionCallback callback;
    pv::SigSession session;
    session.set_callback(&callback);
    session.set_as_current();
    const ds_device_handle handle = addMsoArchiveDevice(
        writeMsoArchive(temporary, QStringLiteral("mso-source.pxs")));
    BOOST_REQUIRE(session.set_device(handle));
    session.set_as_current();
    BOOST_REQUIRE_EQUAL(session.get_device()->get_work_mode(), MSO);
    BOOST_REQUIRE(session.start_capture(true));
    waitForCaptureEnd(callback);

    BOOST_CHECK_EQUAL(callback.header_count, 1U);
    BOOST_CHECK_EQUAL(session.get_ch_num(SR_CHANNEL_LOGIC), 1);
    BOOST_CHECK_EQUAL(session.get_ch_num(SR_CHANNEL_ANALOG), 1);

    auto *logic = dynamic_cast<pv::data::LogicSnapshot *>(
        session.get_snapshot(SR_CHANNEL_LOGIC));
    auto *analog = dynamic_cast<pv::data::AnalogSnapshot *>(
        session.get_snapshot(SR_CHANNEL_ANALOG));
    BOOST_REQUIRE(logic != nullptr);
    BOOST_REQUIRE(analog != nullptr);
    BOOST_CHECK_EQUAL(logic->get_sample_count(), 64ULL);
    BOOST_CHECK(logic->get_sample(0, 0));
    BOOST_CHECK(!logic->get_sample(1, 0));
    BOOST_CHECK_EQUAL(analog->get_sample_count(), 4ULL);
    BOOST_CHECK_EQUAL(analog->get_unit_bytes(), 2U);
    BOOST_CHECK_EQUAL(analog->sample_as_double(0, 0), 0x1234);

    TestSessionDataGetter getter;
    pv::StoreSession store(&session);
    store._sessionDataGetter = &getter;
    const QString saved_path = temporary.filePath(QStringLiteral("mso-saved.pxs"));
    store.setOutputFileName(saved_path);
    BOOST_REQUIRE_MESSAGE(store.save_start(), store.error().toStdString());
    store.wait();
    BOOST_REQUIRE_MESSAGE(store.error().isEmpty(), store.error().toStdString());
    BOOST_REQUIRE(QFileInfo(saved_path).size() > 0);
    HeadlessSessionCallback restored_callback;
    pv::SigSession restored;
    restored.set_callback(&restored_callback);
    restored.set_as_current();
    BOOST_REQUIRE(restored.set_device(addMsoArchiveDevice(saved_path)));
    restored.set_as_current();
    BOOST_REQUIRE(restored.start_capture(true));
    waitForCaptureEnd(restored_callback);

    BOOST_CHECK_EQUAL(restored.get_ch_num(SR_CHANNEL_LOGIC), 1);
    BOOST_CHECK_EQUAL(restored.get_ch_num(SR_CHANNEL_ANALOG), 1);
    auto *restored_logic = dynamic_cast<pv::data::LogicSnapshot *>(
        restored.get_snapshot(SR_CHANNEL_LOGIC));
    auto *restored_analog = dynamic_cast<pv::data::AnalogSnapshot *>(
        restored.get_snapshot(SR_CHANNEL_ANALOG));
    BOOST_REQUIRE(restored_logic != nullptr);
    BOOST_REQUIRE(restored_analog != nullptr);
    BOOST_CHECK_EQUAL(restored_logic->get_sample_count(), 64ULL);
    BOOST_CHECK(restored_logic->get_sample(0, 0));
    BOOST_CHECK_EQUAL(restored_analog->get_sample_count(), 4ULL);
    BOOST_CHECK_EQUAL(restored_analog->get_unit_bytes(), 2U);
    BOOST_CHECK_EQUAL(restored_analog->sample_as_double(0, 3), 0xdef0);
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

BOOST_AUTO_TEST_CASE(import_only_logic_formats_produce_waveforms)
{
    struct Fixture {
        QString format;
        QString name;
        QByteArray bytes;
        QMap<QString, QVariant> options;
        uint64_t minimum_samples;
        bool sample_zero_high;
        bool verify_sample_one;
        bool sample_one_high;
    };
    const std::array<Fixture, 5> fixtures = {{
        {QStringLiteral("saleae"), QStringLiteral("logic.logic"), saleaeFixture(),
         {{QStringLiteral("format"), QStringLiteral("logic2-digital")},
          {QStringLiteral("changed"), false}, {QStringLiteral("wordsize"), 8U},
          {QStringLiteral("samplerate"), qulonglong(1000000)}}, 3, false, true, true},
        {QStringLiteral("protocoldata"), QStringLiteral("uart.bin"), QByteArray(1, '\x41'),
         {{QStringLiteral("protocol"), QStringLiteral("uart")}}, 2, true, true, true},
        {QStringLiteral("logicport"), QStringLiteral("capture.lpf"), logicportFixture(),
         {}, 2, false, true, true},
        {QStringLiteral("trace32_ad"), QStringLiteral("capture.ad"), trace32Fixture(),
         {}, 1, true, false, false},
        {QStringLiteral("stf"), QStringLiteral("capture.stf"), stfFixture(),
         {}, 448, false, true, true},
    }};

    for (const Fixture &fixture : fixtures) {
        QTemporaryDir temporary;
        BOOST_REQUIRE(temporary.isValid());
        const auto imported = importGenerated(fixture.format,
            writeFixture(temporary, fixture.name, fixture.bytes), fixture.options);
        BOOST_REQUIRE_MESSAGE(imported->result.ok,
            fixture.format.toStdString() + ": " + imported->result.error.toStdString());
        auto *snapshot = dynamic_cast<pv::data::LogicSnapshot *>(
            imported->session->get_snapshot(SR_CHANNEL_LOGIC));
        BOOST_REQUIRE(snapshot != nullptr);
        BOOST_CHECK_GE(snapshot->get_sample_count(), fixture.minimum_samples);
        BOOST_CHECK_MESSAGE(snapshot->get_sample(0, 0) == fixture.sample_zero_high,
            fixture.format.toStdString() + " sample 0 differs from fixture");
        if (fixture.verify_sample_one) {
            BOOST_CHECK_MESSAGE(snapshot->get_sample(1, 0) == fixture.sample_one_high,
                fixture.format.toStdString() + " sample 1 differs from fixture");
        }
    }
}

BOOST_AUTO_TEST_CASE(import_only_analog_formats_produce_samples)
{
    const std::array<QString, 2> formats = {
        QStringLiteral("raw_analog"), QStringLiteral("isf")};
    const std::array<QByteArray, 2> fixtures = {
        QByteArray::fromHex("0080ff"), isfFixture()};
    const QMap<QString, QVariant> raw_options = {
        {QStringLiteral("numchannels"), 1},
        {QStringLiteral("samplerate"), qulonglong(1234)},
        {QStringLiteral("format"), QStringLiteral("U8 (0..255)")}};

    for (size_t index = 0; index < formats.size(); ++index) {
        QTemporaryDir temporary;
        BOOST_REQUIRE(temporary.isValid());
        const auto imported = importGenerated(formats[index],
            writeFixture(temporary, QStringLiteral("capture.bin"), fixtures[index]),
            index == 0 ? raw_options : QMap<QString, QVariant>());
        BOOST_REQUIRE_MESSAGE(imported->result.ok, imported->result.error.toStdString());
        auto *snapshot = dynamic_cast<pv::data::AnalogSnapshot *>(
            imported->session->get_snapshot(SR_CHANNEL_ANALOG));
        BOOST_REQUIRE(snapshot != nullptr);
        BOOST_CHECK_MESSAGE(snapshot->get_sample_count() == 3ULL,
            formats[index].toStdString() + " sample count=" +
            std::to_string(snapshot->get_sample_count()));
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
