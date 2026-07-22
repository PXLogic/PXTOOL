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
 * along with this program.  If not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef DSVIEW_PV_DATA_INPUTIMPORTER_H
#define DSVIEW_PV_DATA_INPUTIMPORTER_H

#include <stdint.h>

#include <limits>
#include <stdexcept>

#include <QFile>
#include <QIODevice>
#include <QString>
#include <QVariant>

#include "iooptions.h"

namespace pv {

class SigSession;

namespace data {

struct ImportResult {
    bool ok = false;
    QString error;
    uint64_t sampleRate = 0;
    uint64_t sampleLimit = 0;
};

struct CsvImportPlan {
    uint64_t sampleRate = 1;
    uint64_t sampleLimit = 1;
    uint64_t logicChannelCount = 0;
};

inline bool estimateDsViewCsvImportPlan(const QString &fileName,
                                        CsvImportPlan &plan)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    auto parse_natural_value = [](const QString &text, uint64_t &value) -> bool {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty())
            return false;

        int pos = 0;
        while (pos < trimmed.size()) {
            const QChar ch = trimmed.at(pos);
            if (ch.isDigit() || ch == QLatin1Char('.')) {
                ++pos;
                continue;
            }
            break;
        }
        if (pos == 0)
            return false;

        bool ok = false;
        const double number = trimmed.left(pos).toDouble(&ok);
        if (!ok || number < 0.0)
            return false;

        const QString unit = trimmed.mid(pos).trimmed();
        double scale = 1.0;
        if (!unit.isEmpty()) {
            const QChar prefix = unit.at(0);
            if (prefix == QLatin1Char('G'))
                scale = 1e9;
            else if (prefix == QLatin1Char('M'))
                scale = 1e6;
            else if (prefix == QLatin1Char('K') || prefix == QLatin1Char('k'))
                scale = 1e3;
        }

        const double converted = number * scale;
        if (converted < 1.0)
            return false;
        value = static_cast<uint64_t>(converted + 0.5);
        return value != 0;
    };

    bool saw_header = false;
    bool saw_samplerate = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty())
            continue;

        if (line.startsWith(QLatin1Char(';'))) {
            const QString body = line.mid(1).trimmed();
            if (body.startsWith(QStringLiteral("Sample rate:"), Qt::CaseInsensitive)) {
                saw_samplerate = parse_natural_value(
                    body.mid(QStringLiteral("Sample rate:").size()), plan.sampleRate);
            } else if (body.startsWith(QStringLiteral("Sample count:"), Qt::CaseInsensitive)) {
                (void)parse_natural_value(
                    body.mid(QStringLiteral("Sample count:").size()), plan.sampleLimit);
            }
            continue;
        }

        if (line.startsWith(QStringLiteral("Time(s)"), Qt::CaseInsensitive)) {
            const QStringList columns = line.split(QLatin1Char(','));
            if (columns.size() > 1)
                plan.logicChannelCount = static_cast<uint64_t>(columns.size() - 1);
            saw_header = true;
        }
        break;
    }

    return saw_header && saw_samplerate;
}

inline void applyDsViewCsvImportPlan(IoOptions &options,
                                     const CsvImportPlan &plan)
{
    if (plan.sampleRate > 0)
        options.set(QStringLiteral("samplerate"),
                    QVariant::fromValue(static_cast<guint64>(plan.sampleRate)));

    if (plan.logicChannelCount > 0) {
        if (plan.logicChannelCount > (std::numeric_limits<guint32>::max)())
            throw std::invalid_argument("Invalid CSV logic channel count");

        options.set(QStringLiteral("first_column"),
                    QVariant::fromValue(static_cast<guint32>(2)));
        options.set(QStringLiteral("logic_channels"),
                    QVariant::fromValue(static_cast<guint32>(plan.logicChannelCount)));
        options.set(QStringLiteral("dsview_cross_data"), QVariant(true));
    }
}

class InputImporter final {
public:
    static ImportResult importFile(SigSession &session,
                                   const QString &formatId,
                                   const QString &fileName,
                                   const IoOptions &options);
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_INPUTIMPORTER_H
