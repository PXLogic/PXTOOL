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
#include <QVector>
#include "logsearch.h"
#include "../ui/uimanager.h"
#include "../ui/dscombobox.h"
#include "../log.h"

class QCheckBox;
class QEvent;
class QLineEdit;
class QToolButton;

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

    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onTimer();
    void onClear();
    void onLevelChanged(int index);
    void onSearchClicked();
    void onSearchTextChanged(const QString &text);
    void onExactSearchChanged(bool checked);
    void onSearchNext();
    void onSearchPrevious();

private:
    void appendEntries(const QList<UiLogEntry> &entries);
    void rerenderAll();
    void refreshSearch();
    void updateSearchHighlights();
    void updateSearchCounter();
    void gotoSearchMatch(int index);

    QPlainTextEdit     *_text = nullptr;
    QLabel             *_level_label = nullptr;
    DsComboBox         *_level_combo = nullptr;
    QPushButton        *_search_btn = nullptr;
    QLineEdit          *_search_edit = nullptr;
    QCheckBox          *_exact_check = nullptr;
    QToolButton        *_search_prev_btn = nullptr;
    QToolButton        *_search_next_btn = nullptr;
    QLabel             *_search_counter = nullptr;
    QPushButton        *_clear_btn = nullptr;
    QTimer             *_timer = nullptr;
    int                 _filter_level;
    QList<UiLogEntry>   _history;
    QVector<LogSearchMatch> _search_matches;
    int                 _current_search_match = -1;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_DOCK_LOGDOCK_H
