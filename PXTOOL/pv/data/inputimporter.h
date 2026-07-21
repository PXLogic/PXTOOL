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

#include <QString>

#include "iooptions.h"

namespace pv {

class SigSession;

namespace data {

struct ImportResult {
    bool ok = false;
    QString error;
};

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
