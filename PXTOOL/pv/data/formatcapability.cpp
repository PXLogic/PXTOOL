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

#include "formatcapability.h"

#include <QStringList>

extern "C" {
#include "libsigrok.h"
}

namespace pv {
namespace data {

namespace {

QString inputDescription(const sr_input_format *format)
{
    return QString::fromUtf8(format->description ? format->description : format->id);
}

QString outputDescription(const sr_output_module *module)
{
    return QString::fromUtf8(module->desc ? module->desc : module->id);
}

QString extensionFromId(const char *id)
{
    if (!id || !*id)
        return "*";
    return QString("*.%1").arg(QString::fromUtf8(id));
}

FormatCapability makeImportCapability(const sr_input_format *format)
{
    const QString id = QString::fromUtf8(format->id);
    const QString description = inputDescription(format);
    const QString extension = extensionFromId(format->id);

    FormatCapability capability;
    capability.kind = FormatKind::Import;
    capability.id = id;
    capability.description = description;
    capability.dialogFilter = QString("%1 (%2)").arg(description, extension);
    capability.menuText = QString("Import %1...").arg(description);
    return capability;
}

FormatCapability makeExportCapability(const sr_output_module *module)
{
    const QString id = QString::fromUtf8(module->id);
    const QString description = outputDescription(module);
    const QString extension = extensionFromId(module->id);

    FormatCapability capability;
    capability.kind = FormatKind::Export;
    capability.id = id;
    capability.description = description;
    capability.dialogFilter = QString("%1 (%2)").arg(description, extension);
    capability.menuText = QString("Export %1...").arg(description);
    return capability;
}

} // namespace

QVector<FormatCapability> importFormats()
{
    QVector<FormatCapability> formats;
    sr_input_format **modules = sr_input_list();
    for (int i = 0; modules && modules[i]; i++) {
        if (!modules[i]->id)
            continue;
        formats.push_back(makeImportCapability(modules[i]));
    }
    return formats;
}

QVector<FormatCapability> exportFormats()
{
    QVector<FormatCapability> formats;
    const sr_output_module **modules = sr_output_list();
    for (int i = 0; modules && modules[i]; i++) {
        if (!modules[i]->id)
            continue;
        formats.push_back(makeExportCapability(modules[i]));
    }
    return formats;
}

QString openDialogFilter()
{
    QStringList filters;
    filters << "DSView Data (*.dsl)";
    const QVector<FormatCapability> imports = importFormats();
    for (const FormatCapability &format : imports)
        filters << format.dialogFilter;
    return filters.join(";;");
}

QString saveDialogFilter(const QVector<FormatCapability> &formats)
{
    QStringList filters;
    for (const FormatCapability &format : formats)
        filters << format.dialogFilter;
    return filters.join(";;");
}

const FormatCapability *findFormatById(const QVector<FormatCapability> &formats,
                                       const QString &id)
{
    for (const FormatCapability &format : formats) {
        if (format.id == id)
            return &format;
    }
    return nullptr;
}

} // namespace data
} // namespace pv
