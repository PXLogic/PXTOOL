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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <boost/test/unit_test.hpp>

#include <algorithm>

#include <QStringList>

#include "../pv/data/formatcapability.h"

using pv::data::FormatCapability;
using pv::data::FormatKind;

namespace {
QStringList ids(const QVector<FormatCapability> &formats)
{
    QStringList result;
    for (const FormatCapability &format : formats)
        result.append(format.id);
    return result;
}

FormatCapability find_id(const QVector<FormatCapability> &formats, const QString &id)
{
    auto it = std::find_if(formats.begin(), formats.end(),
        [&](const FormatCapability &format) { return format.id == id; });
    BOOST_REQUIRE(it != formats.end());
    return *it;
}

} // namespace

BOOST_AUTO_TEST_SUITE(formatcapability)

BOOST_AUTO_TEST_CASE(enumerates_current_input_formats)
{
    const QVector<FormatCapability> inputs = pv::data::importFormats();
    const QStringList expected = {
        "binary", "chronovu-la8", "csv", "isf", "logicport", "null",
        "protocoldata", "raw_analog", "saleae", "stf", "trace32_ad",
        "vcd", "wav"
    };

    BOOST_CHECK(ids(inputs) == expected);

    const FormatCapability vcd = find_id(inputs, "vcd");
    BOOST_CHECK(vcd.kind == FormatKind::Import);
    BOOST_CHECK_EQUAL(vcd.description.toStdString(), "Value Change Dump data");
    BOOST_CHECK(vcd.dialogFilter.contains("*.vcd"));
    BOOST_CHECK(vcd.menuText.contains("Value Change Dump data"));
}

BOOST_AUTO_TEST_CASE(enumerates_current_output_formats)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();

    BOOST_CHECK(find_id(outputs, "csv").id == "csv");
    BOOST_CHECK(find_id(outputs, "vcd").id == "vcd");
    BOOST_CHECK(find_id(outputs, "gnuplot").id == "gnuplot");
    BOOST_CHECK(find_id(outputs, "srzip").id == "srzip");

    const FormatCapability csv = find_id(outputs, "csv");
    BOOST_CHECK(csv.kind == FormatKind::Export);
    BOOST_CHECK_EQUAL(csv.description.toStdString(), "Comma-separated values");
    BOOST_CHECK(csv.dialogFilter.contains("*.csv"));
    BOOST_CHECK(csv.menuText.contains("Comma-separated values"));
}

BOOST_AUTO_TEST_CASE(native_open_filter_precedes_import_filters)
{
    const QString filter = pv::data::openDialogFilter();

    BOOST_CHECK(filter.startsWith("DSView Data (*.dsl)"));
    BOOST_CHECK(filter.contains("Comma-separated values"));
    BOOST_CHECK(filter.contains("Value Change Dump data (*.vcd)"));
    BOOST_CHECK(filter.contains("Microsoft WAV file format data (*.wav)"));
}

BOOST_AUTO_TEST_CASE(import_capabilities_report_option_availability)
{
    const QVector<FormatCapability> inputs = pv::data::importFormats();

    BOOST_CHECK(find_id(inputs, "csv").hasOptions);
    BOOST_CHECK(find_id(inputs, "vcd").hasOptions);
    BOOST_CHECK(find_id(inputs, "binary").hasOptions);
    BOOST_CHECK(!find_id(inputs, "null").hasOptions);
}

BOOST_AUTO_TEST_CASE(import_menu_labels_are_stable)
{
    const QVector<FormatCapability> inputs = pv::data::importFormats();

    const FormatCapability vcd = find_id(inputs, "vcd");
    const FormatCapability wav = find_id(inputs, "wav");

    BOOST_CHECK_EQUAL(vcd.menuText.toStdString(), "Import Value Change Dump data...");
    BOOST_CHECK_EQUAL(wav.menuText.toStdString(), "Import Microsoft WAV file format data...");
}

BOOST_AUTO_TEST_CASE(export_menu_labels_are_stable)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();

    BOOST_CHECK_EQUAL(find_id(outputs, "csv").menuText.toStdString(),
                      "Export Comma-separated values...");
    BOOST_CHECK_EQUAL(find_id(outputs, "vcd").menuText.toStdString(),
                      "Export Value Change Dump...");
}

BOOST_AUTO_TEST_CASE(export_menu_order_keeps_dsview_formats_first)
{
    const QStringList expected = {
        "csv", "vcd", "gnuplot", "srzip", "analog", "ascii", "binary",
        "bits", "chronovu-la8", "hex", "null", "ols", "wav", "wavedrom"
    };

    const QStringList actual = pv::data::exportMenuIds();
    BOOST_CHECK(actual == expected);
}

BOOST_AUTO_TEST_CASE(option_dialog_is_required_only_for_optioned_formats)
{
    BOOST_CHECK(pv::data::formatRequiresOptions("ascii"));
    BOOST_CHECK(pv::data::formatRequiresOptions("bits"));
    BOOST_CHECK(pv::data::formatRequiresOptions("hex"));
    BOOST_CHECK(pv::data::formatRequiresOptions("analog"));
    BOOST_CHECK(pv::data::formatRequiresOptions("wav"));
    BOOST_CHECK(!pv::data::formatRequiresOptions("csv"));
    BOOST_CHECK(!pv::data::formatRequiresOptions("srzip"));
    BOOST_CHECK(!pv::data::formatRequiresOptions("binary"));
    BOOST_CHECK(!pv::data::formatRequiresOptions("null"));
}

BOOST_AUTO_TEST_CASE(export_capabilities_describe_data_compatibility)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();

    const FormatCapability csv = find_id(outputs, "csv");
    BOOST_CHECK(csv.supportsLogic);
    BOOST_CHECK(csv.supportsAnalog);
    BOOST_CHECK(!csv.acceptsAnyData);

    const FormatCapability vcd = find_id(outputs, "vcd");
    BOOST_CHECK(vcd.supportsLogic);
    BOOST_CHECK(!vcd.supportsAnalog);

    const FormatCapability wav = find_id(outputs, "wav");
    BOOST_CHECK(!wav.supportsLogic);
    BOOST_CHECK(wav.supportsAnalog);

    const FormatCapability null_output = find_id(outputs, "null");
    BOOST_CHECK(null_output.acceptsAnyData);
}

BOOST_AUTO_TEST_CASE(export_compatibility_errors_name_format_and_data_type)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();
    const FormatCapability vcd = find_id(outputs, "vcd");
    const FormatCapability wav = find_id(outputs, "wav");
    const FormatCapability null_output = find_id(outputs, "null");

    const QString analog_error = pv::data::exportCompatibilityError(
        vcd, pv::data::ExportDataType::Analog);
    BOOST_CHECK(analog_error.contains(vcd.description));
    BOOST_CHECK(analog_error.contains("analog", Qt::CaseInsensitive));

    const QString logic_error = pv::data::exportCompatibilityError(
        wav, pv::data::ExportDataType::Logic);
    BOOST_CHECK(logic_error.contains(wav.description));
    BOOST_CHECK(logic_error.contains("logic", Qt::CaseInsensitive));

    const QString dso_error = pv::data::exportCompatibilityError(
        wav, pv::data::ExportDataType::Dso);
    BOOST_CHECK(dso_error.contains(wav.description));
    BOOST_CHECK(dso_error.contains("DSO"));

    BOOST_CHECK(pv::data::exportCompatibilityError(
        null_output, pv::data::ExportDataType::Dso).isEmpty());
}

BOOST_AUTO_TEST_CASE(generic_export_resolves_format_from_final_dialog_filter)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();
    const FormatCapability wav = find_id(outputs, "wav");

    const FormatCapability *resolved = pv::data::resolveExportFormatSelection(
        outputs, QString(), wav.dialogFilter, QString());

    BOOST_REQUIRE(resolved != nullptr);
    BOOST_CHECK_EQUAL(resolved->id.toStdString(), "wav");
}

BOOST_AUTO_TEST_CASE(generic_export_resolves_format_from_final_suffix)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();

    const FormatCapability *resolved = pv::data::resolveExportFormatSelection(
        outputs, QString(), QString(), QString("vcd"));

    BOOST_REQUIRE(resolved != nullptr);
    BOOST_CHECK_EQUAL(resolved->id.toStdString(), "vcd");
}

BOOST_AUTO_TEST_CASE(generic_import_prefers_suffix_over_dialog_filter)
{
    const QVector<FormatCapability> inputs = pv::data::importFormats();
    const FormatCapability binary = find_id(inputs, "binary");

    const FormatCapability *resolved = pv::data::resolveImportFormatSelection(
        inputs, QString(), binary.dialogFilter, QString("csv"));

    BOOST_REQUIRE(resolved != nullptr);
    BOOST_CHECK_EQUAL(resolved->id.toStdString(), "csv");
}

BOOST_AUTO_TEST_CASE(generic_import_falls_back_to_dialog_filter)
{
    const QVector<FormatCapability> inputs = pv::data::importFormats();
    const FormatCapability csv = find_id(inputs, "csv");

    const FormatCapability *resolved = pv::data::resolveImportFormatSelection(
        inputs, QString(), csv.dialogFilter, QString());

    BOOST_REQUIRE(resolved != nullptr);
    BOOST_CHECK_EQUAL(resolved->id.toStdString(), "csv");
}

BOOST_AUTO_TEST_CASE(final_dialog_filter_controls_compatibility_validation)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();
    const FormatCapability wav = find_id(outputs, "wav");

    const FormatCapability *resolved = pv::data::resolveExportFormatSelection(
        outputs, QString(), wav.dialogFilter, QString());
    BOOST_REQUIRE(resolved != nullptr);

    const QString error = pv::data::exportCompatibilityError(
        *resolved, pv::data::ExportDataType::Logic);

    BOOST_CHECK(error.contains(wav.description));
    BOOST_CHECK(error.contains("logic", Qt::CaseInsensitive));
}

BOOST_AUTO_TEST_CASE(export_capabilities_keep_declared_extensions)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();

    BOOST_CHECK(find_id(outputs, "gnuplot").extensions == QStringList({"dat"}));
    BOOST_CHECK(find_id(outputs, "srzip").extensions == QStringList({"sr"}));
    BOOST_CHECK(find_id(outputs, "null").extensions.isEmpty());
}

BOOST_AUTO_TEST_CASE(export_capabilities_report_option_availability)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();

    BOOST_CHECK(find_id(outputs, "csv").hasOptions);
    BOOST_CHECK(find_id(outputs, "srzip").hasOptions);
    BOOST_CHECK(!find_id(outputs, "vcd").hasOptions);
    BOOST_CHECK(!find_id(outputs, "null").hasOptions);
}

BOOST_AUTO_TEST_CASE(deferred_output_extensions_remain_unavailable_until_registered)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();

    BOOST_CHECK(find_id(outputs, "chronovu-la8").extensions == QStringList({"kdt"}));
    BOOST_CHECK(find_id(outputs, "ols").extensions == QStringList({"ols"}));
    BOOST_CHECK(find_id(outputs, "wavedrom").extensions ==
                QStringList({"wavedrom", "json"}));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(enumerates_final_export_format_manifest)
{
    const QVector<pv::data::FormatCapability> formats = pv::data::exportFormats();
    const QStringList actual = ids(formats);
    const QStringList expected = {
        "csv", "vcd", "gnuplot", "srzip",
        "analog", "ascii", "binary", "bits", "chronovu-la8",
        "hex", "null", "ols", "wav", "wavedrom"
    };

    BOOST_REQUIRE_EQUAL(actual.size(), expected.size());
    for (int i = 0; i < actual.size(); ++i)
        BOOST_CHECK_EQUAL(actual.at(i).toStdString(), expected.at(i).toStdString());
}
