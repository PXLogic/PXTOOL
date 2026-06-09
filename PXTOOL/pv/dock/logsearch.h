/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef DSVIEW_PV_DOCK_LOGSEARCH_H
#define DSVIEW_PV_DOCK_LOGSEARCH_H

#include <QString>
#include <QVector>

namespace pv {
namespace dock {

struct LogSearchMatch
{
    int position;
    int length;
};

QVector<LogSearchMatch> findLogSearchMatches(const QString &text,
                                             const QString &query,
                                             bool exactMatch);

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_DOCK_LOGSEARCH_H
