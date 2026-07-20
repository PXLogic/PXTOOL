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

bool matchesType(GVariant *definition, const QVariant &value)
{
    const int type = value.metaType().id();

    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_BOOLEAN))
        return type == QMetaType::Bool;
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_BYTE))
        return type == QMetaType::UChar;
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_INT16))
        return type == QMetaType::Short;
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_UINT16))
        return type == QMetaType::UShort;
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_INT32))
        return type == QMetaType::Int;
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_UINT32))
        return type == QMetaType::UInt;
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_INT64))
        return type == QMetaType::LongLong;
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_UINT64))
        return type == QMetaType::ULongLong;
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_DOUBLE))
        return type == QMetaType::Double;
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_STRING))
        return type == QMetaType::QString;

    throw std::invalid_argument("Unsupported libsigrok option type");
}

GVariant *qVariantToVariant(GVariant *definition, const QVariant &value)
{
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_BOOLEAN))
        return g_variant_new_boolean(value.toBool());
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_BYTE))
        return g_variant_new_byte(value.value<guchar>());
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_INT16))
        return g_variant_new_int16(value.value<gint16>());
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_UINT16))
        return g_variant_new_uint16(value.value<guint16>());
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_INT32))
        return g_variant_new_int32(value.toInt());
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_UINT32))
        return g_variant_new_uint32(value.toUInt());
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_INT64))
        return g_variant_new_int64(value.toLongLong());
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_UINT64))
        return g_variant_new_uint64(value.toULongLong());
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_DOUBLE))
        return g_variant_new_double(value.toDouble());
    if (g_variant_is_of_type(definition, G_VARIANT_TYPE_STRING))
        return g_variant_new_string(value.toString().toUtf8().constData());

    throw std::invalid_argument("Unsupported libsigrok option type");
}

gint compareVariant(gconstpointer left, gconstpointer right)
{
    return g_variant_equal((GVariant *)left, (GVariant *)right) ? 0 : 1;
}

bool isEnumeratedValue(const sr_option *option, const QVariant &value)
{
    if (!option->values)
        return true;

    GVariant *candidate = g_variant_ref_sink(qVariantToVariant(option->def, value));
    const bool found = g_slist_find_custom(option->values, candidate, compareVariant) != nullptr;
    g_variant_unref(candidate);
    return found;
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

        entries_.insert(id, Entry{ option, variantToQVariant(option->def) });
    }
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
    if (!matchesType(entry->definition->def, value))
        throw std::invalid_argument("Invalid libsigrok option value type");
    if (!isEnumeratedValue(entry->definition, value))
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
            GVariant *value = qVariantToVariant(entry->definition->def, entry->value);
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
