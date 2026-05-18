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


#ifndef DSVIEW_PV_SEARCH_H
#define DSVIEW_PV_SEARCH_H

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QGridLayout>

#include "../sigsession.h"
#include "../toolbars/titlebar.h"
#include "../widgets/searchedgeflagedit.h"
#include "dsdialog.h"

namespace pv {
namespace dialogs {

// Alias for backwards compatibility; the actual class now lives in pv::widgets so
// SearchDock can reuse it without taking a dependency on this dialog header.
using SearchEdgeFlagEdit = pv::widgets::SearchEdgeFlagEdit;

/**
 * @deprecated since 2026-05-18.
 * The "Search Options" UI has been merged into SearchDock; this dialog is
 * retained only as a fallback / for tests. New code should NOT instantiate it.
 *
 * See docs/superpowers/specs/2026-05-18-search-options-merge-into-dock.md
 */
class Search : public DSDialog
{
    Q_OBJECT

public:

    Search(QWidget *parent, SigSession *session, std::map<uint16_t, QString> pattern);
    ~Search();

    std::map<uint16_t, QString> get_pattern();

protected:
    void accept();

signals:
    
private slots:
    void format();
    
private:
    SigSession *_session;

    toolbars::TitleBar *_titlebar;
    QVector<QLineEdit *> _search_lineEdit_vec;
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_SEARCH_H
