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

bool contains_id(const QVector<FormatCapability> &formats, const QString &id)
{
    return std::any_of(formats.begin(), formats.end(),
        [&](const FormatCapability &format) { return format.id == id; });
}

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

    BOOST_CHECK(contains_id(inputs, "vcd"));
    BOOST_CHECK(contains_id(inputs, "wav"));
    BOOST_CHECK(contains_id(inputs, "binary"));

    const FormatCapability vcd = find_id(inputs, "vcd");
    BOOST_CHECK(vcd.kind == FormatKind::Import);
    BOOST_CHECK_EQUAL(vcd.description.toStdString(), "Value Change Dump data");
    BOOST_CHECK(vcd.dialogFilter.contains("*.vcd"));
    BOOST_CHECK(vcd.menuText.contains("Value Change Dump data"));
}

BOOST_AUTO_TEST_CASE(enumerates_current_output_formats)
{
    const QVector<FormatCapability> outputs = pv::data::exportFormats();

    BOOST_CHECK(contains_id(outputs, "csv"));
    BOOST_CHECK(contains_id(outputs, "vcd"));
    BOOST_CHECK(contains_id(outputs, "gnuplot"));
    BOOST_CHECK(contains_id(outputs, "srzip"));

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
    BOOST_CHECK(filter.contains("Value Change Dump data (*.vcd)"));
    BOOST_CHECK(filter.contains("Microsoft WAV file format data (*.wav)"));
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
