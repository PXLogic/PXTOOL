/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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

#ifndef DSVIEW_PV_WIDGETS_SEARCHEDGEFLAGEDIT_H
#define DSVIEW_PV_WIDGETS_SEARCHEDGEFLAGEDIT_H

#include <QLineEdit>

namespace pv {
namespace widgets {

/**
 * Single-character QLineEdit used to enter X / 0 / 1 / R / F / C pattern flags.
 *
 * Selects all text on focus so the user can immediately overtype the existing
 * character (originally lived in pv::dialogs alongside the SearchOptions dialog;
 * moved here so the inline editor inside SearchDock can reuse it without taking
 * a reverse dependency on the dialog).
 */
class SearchEdgeFlagEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit SearchEdgeFlagEdit(QWidget *parent);

protected:
    void focusInEvent(QFocusEvent *e) override;
};

} // namespace widgets
} // namespace pv

#endif // DSVIEW_PV_WIDGETS_SEARCHEDGEFLAGEDIT_H
