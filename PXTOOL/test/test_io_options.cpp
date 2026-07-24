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

#include <cmath>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <vector>

#include <QApplication>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMetaType>
#include <QSpinBox>
#include <QVariant>
#include <QWidget>

extern "C" {
#include "libsigrok-internal.h"

struct sr_dev_inst *make_test_sdi(void);
}

#include "../pv/data/iooptions.h"
#include "../pv/dialogs/inputoutputoptioneditor.h"

namespace {

QApplication &testApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    static int argc = 1;
    static char application_name[] = "DSView-test";
    static char *argv[] = { application_name, nullptr };
    static QApplication application(argc, argv);
    return application;
}

class OptionDefinitions final {
public:
    ~OptionDefinitions()
    {
        for (sr_option &option : options_) {
            if (option.def)
                g_variant_unref(option.def);
            g_slist_free_full(option.values, reinterpret_cast<GDestroyNotify>(g_variant_unref));
        }
    }

    void addInt(const char *id, gint32 value)
    {
        add(id, g_variant_new_int32(value));
    }

    void addUint64(const char *id, guint64 value)
    {
        add(id, g_variant_new_uint64(value));
    }

    void addByte(const char *id, guchar value)
    {
        add(id, g_variant_new_byte(value));
    }

    void addInt16(const char *id, gint16 value)
    {
        add(id, g_variant_new_int16(value));
    }

    void addUint16(const char *id, guint16 value)
    {
        add(id, g_variant_new_uint16(value));
    }

    void addUint32(const char *id, guint32 value)
    {
        add(id, g_variant_new_uint32(value));
    }

    void addInt64(const char *id, gint64 value)
    {
        add(id, g_variant_new_int64(value));
    }

    void addDouble(const char *id, double value)
    {
        add(id, g_variant_new_double(value));
    }

    void addStringEnum(const char *id, const char *value,
                       std::initializer_list<const char *> allowed)
    {
        add(id, g_variant_new_string(value));
        sr_option &option = options_.back();
        for (const char *entry : allowed) {
            option.values = g_slist_append(option.values,
                g_variant_ref_sink(g_variant_new_string(entry)));
        }
    }

    const sr_option *const *data()
    {
        pointers_.clear();
        for (const sr_option &option : options_)
            pointers_.push_back(&option);
        pointers_.push_back(nullptr);
        return pointers_.data();
    }

private:
    void add(const char *id, GVariant *value)
    {
        sr_option option{};
        option.id = id;
        option.name = id;
        option.def = g_variant_ref_sink(value);
        options_.push_back(option);
    }

    std::vector<sr_option> options_;
    std::vector<const sr_option *> pointers_;
};

} // namespace

BOOST_AUTO_TEST_SUITE(io_migration_options)

BOOST_AUTO_TEST_CASE(creates_and_frees_output_with_default_options)
{
    const sr_output_module *module = sr_output_find(const_cast<char *>("null"));
    BOOST_REQUIRE(module != nullptr);

    const sr_output *output = sr_output_new(module, nullptr, make_test_sdi());
    BOOST_REQUIRE(output != nullptr);
    BOOST_CHECK_EQUAL(sr_output_free(output), SR_OK);
}

BOOST_AUTO_TEST_CASE(selected_output_options_survive_descriptor_cleanup)
{
    const sr_output_module *module = sr_output_find(const_cast<char *>("ascii"));
    BOOST_REQUIRE(module != nullptr);

    const sr_option **definitions = sr_output_options_get(module);
    BOOST_REQUIRE(definitions != nullptr);

    pv::data::IoOptions selected(definitions);
    selected.set("width", QVariant::fromValue(static_cast<guint32>(123)));
    sr_output_options_free(definitions);

    pv::data::IoOptions routed = selected;
    GHashTable *values = nullptr;
    BOOST_CHECK_NO_THROW(values = routed.toGHashTable());
    BOOST_REQUIRE(values != nullptr);

    GVariant *width = static_cast<GVariant *>(
        g_hash_table_lookup(values, "width"));
    BOOST_REQUIRE(width != nullptr);
    BOOST_CHECK_EQUAL(g_variant_get_uint32(width), 123U);
    g_hash_table_destroy(values);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(option_values_use_module_defaults)
{
    OptionDefinitions definitions;
    definitions.addInt("channels", 8);
    definitions.addUint64("samplerate", 0);

    pv::data::IoOptions values(definitions.data());
    BOOST_CHECK_EQUAL(values.value("channels").toInt(), 8);
    BOOST_CHECK_EQUAL(values.value("samplerate").toULongLong(), 0ULL);
}

BOOST_AUTO_TEST_CASE(option_values_reject_wrong_variant_type)
{
    OptionDefinitions definitions;
    definitions.addInt("channels", 8);

    pv::data::IoOptions values(definitions.data());
    BOOST_CHECK_THROW(values.set("channels", QVariant("eight")), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(option_values_require_enum_membership)
{
    OptionDefinitions definitions;
    definitions.addStringEnum("label", "units", { "units", "channel", "off" });

    pv::data::IoOptions values(definitions.data());
    values.set("label", QVariant("channel"));
    BOOST_CHECK_EQUAL(values.value("label").toString().toStdString(), "channel");
    BOOST_CHECK_THROW(values.set("label", QVariant("invalid")), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(input_output_small_integer_editors_use_spin_boxes)
{
    Q_UNUSED(testApplication());

    OptionDefinitions definitions;
    definitions.addByte("byte", (std::numeric_limits<guchar>::max)());
    definitions.addInt16("int16", (std::numeric_limits<gint16>::min)());
    definitions.addUint16("uint16", (std::numeric_limits<guint16>::max)());
    const sr_option *const *options = definitions.data();

    pv::data::IoOptions values(options);
    QWidget parent;

    auto *byte = qobject_cast<QSpinBox *>(pv::dialogs::detail::makeInputOutputOptionEditor(
        options[0], values.value("byte"), &parent));
    BOOST_REQUIRE(byte != nullptr);
    BOOST_CHECK_EQUAL(byte->minimum(), 0);
    BOOST_CHECK_EQUAL(byte->maximum(), (std::numeric_limits<guchar>::max)());
    BOOST_CHECK_EQUAL(byte->value(), (std::numeric_limits<guchar>::max)());

    auto *int16 = qobject_cast<QSpinBox *>(pv::dialogs::detail::makeInputOutputOptionEditor(
        options[1], values.value("int16"), &parent));
    BOOST_REQUIRE(int16 != nullptr);
    BOOST_CHECK_EQUAL(int16->minimum(), (std::numeric_limits<gint16>::min)());
    BOOST_CHECK_EQUAL(int16->maximum(), (std::numeric_limits<gint16>::max)());
    BOOST_CHECK_EQUAL(int16->value(), (std::numeric_limits<gint16>::min)());

    auto *uint16 = qobject_cast<QSpinBox *>(pv::dialogs::detail::makeInputOutputOptionEditor(
        options[2], values.value("uint16"), &parent));
    BOOST_REQUIRE(uint16 != nullptr);
    BOOST_CHECK_EQUAL(uint16->minimum(), 0);
    BOOST_CHECK_EQUAL(uint16->maximum(), (std::numeric_limits<guint16>::max)());
    BOOST_CHECK_EQUAL(uint16->value(), (std::numeric_limits<guint16>::max)());
}

BOOST_AUTO_TEST_CASE(input_output_64_bit_text_rejects_non_decimal_and_whitespace)
{
    Q_UNUSED(testApplication());

    OptionDefinitions definitions;
    definitions.addInt64("signed", 0);
    definitions.addUint64("unsigned", 0);
    const sr_option *const *options = definitions.data();

    pv::data::IoOptions values(options);
    QWidget parent;

    auto *signed_editor = qobject_cast<QLineEdit *>(
        pv::dialogs::detail::makeInputOutputOptionEditor(
            options[0], values.value("signed"), &parent));
    BOOST_REQUIRE(signed_editor != nullptr);
    signed_editor->setText(" 1");
    BOOST_CHECK_THROW(pv::dialogs::detail::inputOutputOptionEditorValue(
        options[0], signed_editor), std::invalid_argument);
    signed_editor->setText("1 ");
    BOOST_CHECK_THROW(pv::dialogs::detail::inputOutputOptionEditorValue(
        options[0], signed_editor), std::invalid_argument);
    signed_editor->setText("0x10");
    BOOST_CHECK_THROW(pv::dialogs::detail::inputOutputOptionEditorValue(
        options[0], signed_editor), std::invalid_argument);
    signed_editor->setText("1.0");
    BOOST_CHECK_THROW(pv::dialogs::detail::inputOutputOptionEditorValue(
        options[0], signed_editor), std::invalid_argument);
    signed_editor->setText(QString::number((std::numeric_limits<gint64>::min)()));
    BOOST_CHECK_EQUAL(pv::dialogs::detail::inputOutputOptionEditorValue(
        options[0], signed_editor).toLongLong(), (std::numeric_limits<gint64>::min)());
    signed_editor->setText("-9223372036854775809");
    BOOST_CHECK_THROW(pv::dialogs::detail::inputOutputOptionEditorValue(
        options[0], signed_editor), std::invalid_argument);

    auto *unsigned_editor = qobject_cast<QLineEdit *>(
        pv::dialogs::detail::makeInputOutputOptionEditor(
            options[1], values.value("unsigned"), &parent));
    BOOST_REQUIRE(unsigned_editor != nullptr);
    unsigned_editor->setText("-1");
    BOOST_CHECK_THROW(pv::dialogs::detail::inputOutputOptionEditorValue(
        options[1], unsigned_editor), std::invalid_argument);
    unsigned_editor->setText(QString::number((std::numeric_limits<guint64>::max)()));
    BOOST_CHECK_EQUAL(pv::dialogs::detail::inputOutputOptionEditorValue(
        options[1], unsigned_editor).toULongLong(), (std::numeric_limits<guint64>::max)());
    unsigned_editor->setText("18446744073709551616");
    BOOST_CHECK_THROW(pv::dialogs::detail::inputOutputOptionEditorValue(
        options[1], unsigned_editor), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(input_output_uint32_editor_covers_full_range_from_zero_default)
{
    Q_UNUSED(testApplication());

    OptionDefinitions definitions;
    definitions.addUint32("offset", 0);
    const sr_option *const *options = definitions.data();

    pv::data::IoOptions values(options);
    QWidget parent;
    auto *editor = qobject_cast<QDoubleSpinBox *>(
        pv::dialogs::detail::makeInputOutputOptionEditor(
            options[0], values.value("offset"), &parent));
    BOOST_REQUIRE(editor != nullptr);
    BOOST_CHECK_EQUAL(editor->minimum(), 0);
    BOOST_CHECK_EQUAL(editor->maximum(), (std::numeric_limits<guint32>::max)());
    BOOST_CHECK_EQUAL(editor->decimals(), 0);

    editor->setValue((std::numeric_limits<guint32>::max)());
    const QVariant value = pv::dialogs::detail::inputOutputOptionEditorValue(
        options[0], editor);
    BOOST_CHECK_EQUAL(value.type(), QVariant::UInt);
    BOOST_CHECK_EQUAL(value.value<guint32>(), (std::numeric_limits<guint32>::max)());
}

BOOST_AUTO_TEST_CASE(input_output_double_editor_preserves_default_precision)
{
    Q_UNUSED(testApplication());

    const double default_value = std::nextafter(1.0, 2.0);
    OptionDefinitions definitions;
    definitions.addDouble("scale", default_value);
    const sr_option *const *options = definitions.data();

    pv::data::IoOptions values(options);
    QWidget parent;
    auto *editor = qobject_cast<QDoubleSpinBox *>(
        pv::dialogs::detail::makeInputOutputOptionEditor(
            options[0], values.value("scale"), &parent));
    BOOST_REQUIRE(editor != nullptr);
    BOOST_CHECK_EQUAL(editor->decimals(), std::numeric_limits<double>::max_digits10);
    BOOST_CHECK(editor->minimum() <= -(std::numeric_limits<double>::max)());
    BOOST_CHECK(editor->maximum() >= (std::numeric_limits<double>::max)());
    BOOST_CHECK_EQUAL(editor->value(), default_value);
    BOOST_CHECK_EQUAL(pv::dialogs::detail::inputOutputOptionEditorValue(
        options[0], editor).toDouble(), default_value);
}
