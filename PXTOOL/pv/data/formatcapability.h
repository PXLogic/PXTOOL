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

#ifndef DSVIEW_PV_DATA_FORMATCAPABILITY_H
#define DSVIEW_PV_DATA_FORMATCAPABILITY_H

#include <QString>
#include <QStringList>
#include <QVector>

namespace pv {
namespace data {

enum class FormatKind {
    Import,
    Export
};

enum class ExportDataType {
    Logic,
    Analog,
    Dso
};

struct FormatCapability {
    FormatKind kind;
    QString id;
    QString description;
    QString dialogFilter;
    QString menuText;
    QStringList extensions;
    bool hasOptions = false;
    bool supportsLogic = false;
    bool supportsAnalog = false;
    bool acceptsAnyData = false;
};

QVector<FormatCapability> importFormats();
QVector<FormatCapability> exportFormats();
QStringList exportMenuIds();
bool formatRequiresOptions(const QString &id);
QString exportCompatibilityError(const FormatCapability &format,
                                 ExportDataType dataType);
const FormatCapability *resolveExportFormatSelection(
    const QVector<FormatCapability> &formats, const QString &formatId,
    const QString &dialogFilter, const QString &suffix);
QString openDialogFilter();
QString saveDialogFilter(const QVector<FormatCapability> &formats);
const FormatCapability *findFormatById(const QVector<FormatCapability> &formats,
                                       const QString &id);

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_FORMATCAPABILITY_H
