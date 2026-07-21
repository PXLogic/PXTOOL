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

#include "iooptions.h"

#include <stdexcept>

#include <QMetaType>

namespace pv {
namespace data {

namespace {

QVariant variantToQVariant(GVariant *value)
{
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN))
        return QVariant(g_variant_get_boolean(value) != FALSE);
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_BYTE))
        return QVariant::fromValue(g_variant_get_byte(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT16))
        return QVariant::fromValue(g_variant_get_int16(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT16))
        return QVariant::fromValue(g_variant_get_uint16(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32))
        return QVariant(g_variant_get_int32(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
        return QVariant::fromValue(g_variant_get_uint32(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64))
        return QVariant::fromValue(g_variant_get_int64(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64))
        return QVariant::fromValue(g_variant_get_uint64(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_DOUBLE))
        return QVariant(g_variant_get_double(value));
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING))
        return QVariant(QString::fromUtf8(g_variant_get_string(value, nullptr)));

    throw std::invalid_argument("Unsupported libsigrok option type");
}

bool matchesType(const QByteArray &typeSignature, const QVariant &value)
{
    const int type = value.metaType().id();

    if (typeSignature == "b")
        return type == QMetaType::Bool;
    if (typeSignature == "y")
        return type == QMetaType::UChar;
    if (typeSignature == "n")
        return type == QMetaType::Short;
    if (typeSignature == "q")
        return type == QMetaType::UShort;
    if (typeSignature == "i")
        return type == QMetaType::Int;
    if (typeSignature == "u")
        return type == QMetaType::UInt;
    if (typeSignature == "x")
        return type == QMetaType::LongLong;
    if (typeSignature == "t")
        return type == QMetaType::ULongLong;
    if (typeSignature == "d")
        return type == QMetaType::Double;
    if (typeSignature == "s")
        return type == QMetaType::QString;

    throw std::invalid_argument("Unsupported libsigrok option type");
}

GVariant *qVariantToVariant(const QByteArray &typeSignature, const QVariant &value)
{
    if (typeSignature == "b")
        return g_variant_new_boolean(value.toBool());
    if (typeSignature == "y")
        return g_variant_new_byte(value.value<guchar>());
    if (typeSignature == "n")
        return g_variant_new_int16(value.value<gint16>());
    if (typeSignature == "q")
        return g_variant_new_uint16(value.value<guint16>());
    if (typeSignature == "i")
        return g_variant_new_int32(value.toInt());
    if (typeSignature == "u")
        return g_variant_new_uint32(value.toUInt());
    if (typeSignature == "x")
        return g_variant_new_int64(value.toLongLong());
    if (typeSignature == "t")
        return g_variant_new_uint64(value.toULongLong());
    if (typeSignature == "d")
        return g_variant_new_double(value.toDouble());
    if (typeSignature == "s")
        return g_variant_new_string(value.toString().toUtf8().constData());

    throw std::invalid_argument("Unsupported libsigrok option type");
}

bool isEnumeratedValue(const QList<QVariant> &allowedValues,
                       const QVariant &value)
{
    if (allowedValues.isEmpty())
        return true;
    return allowedValues.contains(value);
}

QByteArray typeSignatureForVariant(const QVariant &value)
{
    switch (value.metaType().id()) {
    case QMetaType::Bool:
        return "b";
    case QMetaType::UChar:
        return "y";
    case QMetaType::Short:
        return "n";
    case QMetaType::UShort:
        return "q";
    case QMetaType::Int:
        return "i";
    case QMetaType::UInt:
        return "u";
    case QMetaType::LongLong:
        return "x";
    case QMetaType::ULongLong:
        return "t";
    case QMetaType::Double:
        return "d";
    case QMetaType::QString:
        return "s";
    default:
        throw std::invalid_argument("Unsupported libsigrok option type");
    }
}

} // namespace

IoOptions::IoOptions(const sr_option *const *options)
{
    if (!options)
        return;

    for (int index = 0; options[index]; index++) {
        const sr_option *option = options[index];
        if (!option->id || !option->def)
            throw std::invalid_argument("Invalid libsigrok option definition");

        const QString id = QString::fromUtf8(option->id);
        if (id.isEmpty() || entries_.contains(id))
            throw std::invalid_argument("Duplicate libsigrok option ID");

        Entry entry;
        entry.typeSignature = QByteArray(g_variant_get_type_string(option->def));
        entry.value = variantToQVariant(option->def);
        for (const GSList *allowed = option->values; allowed; allowed = allowed->next)
            entry.allowedValues.append(
                variantToQVariant(static_cast<GVariant *>(allowed->data)));
        entries_.insert(id, entry);
    }
}

IoOptions IoOptions::fromValues(const QMap<QString, QVariant> &values)
{
    IoOptions options(nullptr);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        Entry entry;
        entry.typeSignature = typeSignatureForVariant(it.value());
        entry.value = it.value();
        options.entries_.insert(it.key(), entry);
    }
    return options;
}

QVariant IoOptions::value(const QString &id) const
{
    const auto entry = entries_.constFind(id);
    if (entry == entries_.cend())
        throw std::invalid_argument("Unknown libsigrok option ID");
    return entry->value;
}

void IoOptions::set(const QString &id, const QVariant &value)
{
    auto entry = entries_.find(id);
    if (entry == entries_.end())
        throw std::invalid_argument("Unknown libsigrok option ID");
    if (!matchesType(entry->typeSignature, value))
        throw std::invalid_argument("Invalid libsigrok option value type");
    if (!isEnumeratedValue(entry->allowedValues, value))
        throw std::invalid_argument("Invalid libsigrok option value");

    entry->value = value;
}

bool IoOptions::empty() const
{
    return entries_.isEmpty();
}

GHashTable *IoOptions::toGHashTable() const
{
    GHashTable *options = g_hash_table_new_full(g_str_hash, g_str_equal,
        g_free, reinterpret_cast<GDestroyNotify>(g_variant_unref));

    try {
        for (auto entry = entries_.cbegin(); entry != entries_.cend(); ++entry) {
            GVariant *value = qVariantToVariant(entry->typeSignature, entry->value);
            g_hash_table_insert(options, g_strdup(entry.key().toUtf8().constData()),
                g_variant_ref_sink(value));
        }
    } catch (...) {
        g_hash_table_destroy(options);
        throw;
    }

    return options;
}

} // namespace data
} // namespace pv
