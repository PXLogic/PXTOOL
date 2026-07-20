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
#include <QVector>

namespace pv {
namespace data {

enum class FormatKind {
    Import,
    Export
};

struct FormatCapability {
    FormatKind kind;
    QString id;
    QString description;
    QString dialogFilter;
    QString menuText;
};

QVector<FormatCapability> importFormats();
QVector<FormatCapability> exportFormats();
QString openDialogFilter();
QString saveDialogFilter(const QVector<FormatCapability> &formats);
const FormatCapability *findFormatById(const QVector<FormatCapability> &formats,
                                       const QString &id);

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_FORMATCAPABILITY_H
