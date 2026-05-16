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

#ifndef DSVIEW_PV_DOCK_LOGDOCK_H
#define DSVIEW_PV_DOCK_LOGDOCK_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QList>
#include "../ui/uimanager.h"
#include "../ui/dscombobox.h"
#include "../log.h"

namespace pv {
namespace dock {

class LogDock : public QWidget, public IUiWindow
{
    Q_OBJECT

public:
    explicit LogDock(QWidget *parent);
    ~LogDock();

    // IUiWindow
    void UpdateLanguage() override;
    void UpdateTheme()    override;
    void UpdateFont()     override;

private slots:
    void onTimer();
    void onClear();
    void onLevelChanged(int index);

private:
    void appendEntries(const QList<UiLogEntry> &entries);
    void rerenderAll();

    QPlainTextEdit     *_text;
    QLabel             *_level_label = nullptr;
    DsComboBox         *_level_combo;
    QPushButton        *_clear_btn;
    QTimer             *_timer;
    int                 _filter_level;
    QList<UiLogEntry>   _history;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_DOCK_LOGDOCK_H
