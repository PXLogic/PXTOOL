/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#include "searchedgeflagedit.h"

#include <QTimer>

namespace pv {
namespace widgets {

SearchEdgeFlagEdit::SearchEdgeFlagEdit(QWidget *parent)
    : QLineEdit(parent)
{
}

void SearchEdgeFlagEdit::focusInEvent(QFocusEvent *e)
{
    QLineEdit::focusInEvent(e);

    // Defer selectAll() so it wins over the platform's default click-to-place-cursor
    // behaviour that fires immediately after focusInEvent on some Qt versions.
    QTimer::singleShot(50, this, [this]() {
        selectAll();
    });
}

} // namespace widgets
} // namespace pv
