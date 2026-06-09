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

#include "logsearch.h"

namespace pv {
namespace dock {

QVector<LogSearchMatch> findLogSearchMatches(const QString &text,
                                             const QString &query,
                                             bool exactMatch)
{
    QVector<LogSearchMatch> matches;
    if (text.isEmpty() || query.isEmpty())
        return matches;

    const Qt::CaseSensitivity caseSensitivity =
        exactMatch ? Qt::CaseSensitive : Qt::CaseInsensitive;

    int pos = 0;
    while (pos < text.size()) {
        const int found = text.indexOf(query, pos, caseSensitivity);
        if (found < 0)
            break;

        const int queryLength = static_cast<int>(query.size());
        matches.push_back(LogSearchMatch{found, queryLength});
        pos = found + queryLength;
    }

    return matches;
}

} // namespace dock
} // namespace pv
