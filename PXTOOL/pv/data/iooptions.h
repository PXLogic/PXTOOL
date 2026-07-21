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

#ifndef DSVIEW_PV_DATA_IOOPTIONS_H
#define DSVIEW_PV_DATA_IOOPTIONS_H

#include <QMap>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariant>

extern "C" {
#include "libsigrok.h"
}

namespace pv {
namespace data {

class IoOptions {
public:
    explicit IoOptions(const sr_option *const *options);
    static IoOptions fromValues(const QMap<QString, QVariant> &values);

    QVariant value(const QString &id) const;
    void set(const QString &id, const QVariant &value);
    bool empty() const;
    GHashTable *toGHashTable() const;

private:
    struct Entry {
        QByteArray typeSignature;
        QList<QVariant> allowedValues;
        QVariant value;
    };

    QMap<QString, Entry> entries_;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_IOOPTIONS_H
