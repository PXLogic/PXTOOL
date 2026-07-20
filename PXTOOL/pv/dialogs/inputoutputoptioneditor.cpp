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

#include "inputoutputoptioneditor.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>

namespace pv {
namespace dialogs {
namespace detail {

namespace {

bool isType(const sr_option *option, const GVariantType *type)
{
    return g_variant_is_of_type(option->def, type);
}

QVariant optionValue(GVariant *value)
{
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING))
        return QString::fromUtf8(g_variant_get_string(value, nullptr));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN))
        return g_variant_get_boolean(value) != FALSE;
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_BYTE))
        return QVariant::fromValue(g_variant_get_byte(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT16))
        return QVariant::fromValue(g_variant_get_int16(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT16))
        return QVariant::fromValue(g_variant_get_uint16(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32))
        return g_variant_get_int32(value);
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
        return QVariant::fromValue(g_variant_get_uint32(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64))
        return QVariant::fromValue(g_variant_get_int64(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64))
        return QVariant::fromValue(g_variant_get_uint64(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_DOUBLE))
        return g_variant_get_double(value);

    throw std::invalid_argument("Unsupported libsigrok enum option type");
}

bool isStrictDecimal(const QString &text, bool signed_value)
{
    if (text.isEmpty() || text != text.trimmed())
        return false;

    int first_digit = 0;
    if (signed_value && text.at(0) == QLatin1Char('-')) {
        if (text.size() == 1)
            return false;
        first_digit = 1;
    }

    for (int index = first_digit; index < text.size(); index++) {
        const QChar character = text.at(index);
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }

    return true;
}

QVariant strict64BitValue(const sr_option *option, const QString &text)
{
    bool valid = false;
    if (isType(option, G_VARIANT_TYPE_INT64)) {
        if (!isStrictDecimal(text, true))
            throw std::invalid_argument("Invalid libsigrok numeric option value");

        const qlonglong value = text.toLongLong(&valid, 10);
        if (valid)
            return QVariant::fromValue(value);
    } else if (isType(option, G_VARIANT_TYPE_UINT64)) {
        if (!isStrictDecimal(text, false))
            throw std::invalid_argument("Invalid libsigrok numeric option value");

        const qulonglong value = text.toULongLong(&valid, 10);
        if (valid)
            return QVariant::fromValue(value);
    }

    throw std::invalid_argument("Invalid libsigrok numeric option value");
}

} // namespace

QWidget *makeInputOutputOptionEditor(const sr_option *option,
                                     const QVariant &value,
                                     QWidget *parent)
{
    if (option->values) {
        auto *editor = new QComboBox(parent);
        for (GSList *item = option->values; item; item = item->next) {
            GVariant *candidate = static_cast<GVariant *>(item->data);
            const QVariant candidate_value = optionValue(candidate);
            editor->addItem(candidate_value.toString(), candidate_value);
        }
        editor->setCurrentIndex(editor->findData(value));
        return editor;
    }

    if (isType(option, G_VARIANT_TYPE_BOOLEAN)) {
        auto *editor = new QCheckBox(parent);
        editor->setChecked(value.toBool());
        return editor;
    }
    if (isType(option, G_VARIANT_TYPE_BYTE)) {
        auto *editor = new QSpinBox(parent);
        editor->setRange(0, std::numeric_limits<guchar>::max());
        editor->setValue(value.value<guchar>());
        return editor;
    }
    if (isType(option, G_VARIANT_TYPE_INT16)) {
        auto *editor = new QSpinBox(parent);
        editor->setRange(std::numeric_limits<gint16>::min(),
                         std::numeric_limits<gint16>::max());
        editor->setValue(value.value<gint16>());
        return editor;
    }
    if (isType(option, G_VARIANT_TYPE_UINT16)) {
        auto *editor = new QSpinBox(parent);
        editor->setRange(0, std::numeric_limits<guint16>::max());
        editor->setValue(value.value<guint16>());
        return editor;
    }
    if (isType(option, G_VARIANT_TYPE_INT32)) {
        auto *editor = new QSpinBox(parent);
        editor->setRange(std::numeric_limits<gint32>::min(),
                         std::numeric_limits<gint32>::max());
        editor->setValue(value.toInt());
        return editor;
    }
    if (isType(option, G_VARIANT_TYPE_UINT32)) {
        auto *editor = new QDoubleSpinBox(parent);
        editor->setRange(0, std::numeric_limits<guint32>::max());
        editor->setDecimals(0);
        editor->setValue(value.toUInt());
        return editor;
    }
    if (isType(option, G_VARIANT_TYPE_DOUBLE)) {
        auto *editor = new QDoubleSpinBox(parent);
        editor->setRange(-std::numeric_limits<double>::max(),
                         std::numeric_limits<double>::max());
        editor->setDecimals(std::numeric_limits<double>::max_digits10);
        editor->setValue(value.toDouble());
        return editor;
    }
    if (isType(option, G_VARIANT_TYPE_STRING)
        || isType(option, G_VARIANT_TYPE_INT64)
        || isType(option, G_VARIANT_TYPE_UINT64)) {
        auto *editor = new QLineEdit(parent);
        editor->setText(value.toString());
        return editor;
    }

    throw std::invalid_argument("Unsupported libsigrok option editor type");
}

QVariant inputOutputOptionEditorValue(const sr_option *option,
                                      const QWidget *editor)
{
    if (const auto *combo = qobject_cast<const QComboBox *>(editor))
        return combo->currentData();
    if (const auto *checkbox = qobject_cast<const QCheckBox *>(editor))
        return checkbox->isChecked();
    if (const auto *spinbox = qobject_cast<const QSpinBox *>(editor)) {
        if (isType(option, G_VARIANT_TYPE_BYTE))
            return QVariant::fromValue(static_cast<guchar>(spinbox->value()));
        if (isType(option, G_VARIANT_TYPE_INT16))
            return QVariant::fromValue(static_cast<gint16>(spinbox->value()));
        if (isType(option, G_VARIANT_TYPE_UINT16))
            return QVariant::fromValue(static_cast<guint16>(spinbox->value()));
        if (isType(option, G_VARIANT_TYPE_UINT32))
            return QVariant::fromValue(static_cast<guint32>(spinbox->value()));
        if (isType(option, G_VARIANT_TYPE_INT32))
            return QVariant(spinbox->value());
    }
    if (const auto *spinbox = qobject_cast<const QDoubleSpinBox *>(editor)) {
        if (isType(option, G_VARIANT_TYPE_DOUBLE))
            return QVariant(spinbox->value());
        if (isType(option, G_VARIANT_TYPE_UINT32)) {
            const double value = spinbox->value();
            if (value >= 0 && value <= std::numeric_limits<guint32>::max()
                && std::floor(value) == value)
                return QVariant::fromValue(static_cast<guint32>(value));
        }
    }
    if (const auto *line_edit = qobject_cast<const QLineEdit *>(editor)) {
        if (isType(option, G_VARIANT_TYPE_STRING))
            return QVariant(line_edit->text());
        return strict64BitValue(option, line_edit->text());
    }

    throw std::invalid_argument("Invalid libsigrok option editor");
}

} // namespace detail
} // namespace dialogs
} // namespace pv
