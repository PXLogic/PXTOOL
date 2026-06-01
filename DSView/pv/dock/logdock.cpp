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

#include "logdock.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <log/xlog.h>

namespace pv {
namespace dock {

LogDock::LogDock(QWidget *parent)
    : QWidget(parent)
    , _filter_level(XLOG_LEVEL_INFO)
{
    setObjectName("log_dock");

    // Toolbar row
    auto *toolbar = new QWidget(this);
    toolbar->setObjectName("log_toolbar");
    auto *tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(8, 4, 8, 4);
    tbLayout->setSpacing(8);

    _level_label = new QLabel(tr("Level:"), toolbar);
    _level_label->setObjectName("log_level_label");
    auto *levelLabel = _level_label;

    _level_combo = new DsComboBox(toolbar);
    _level_combo->setObjectName("log_level_combo");
    _level_combo->addItem(tr("Error"),   XLOG_LEVEL_ERR);
    _level_combo->addItem(tr("Warning"), XLOG_LEVEL_WARN);
    _level_combo->addItem(tr("Info"),    XLOG_LEVEL_INFO);
    _level_combo->addItem(tr("Debug"),   XLOG_LEVEL_DBG);
    _level_combo->addItem(tr("Verbose"), XLOG_LEVEL_DETAIL);
    _level_combo->setCurrentIndex(2); // Info

    _clear_btn = new QPushButton(tr("Clear"), toolbar);
    _clear_btn->setObjectName("log_clear_btn");
    _clear_btn->setFixedHeight(24);

    tbLayout->addWidget(levelLabel,  0, Qt::AlignVCenter);
    tbLayout->addWidget(_level_combo, 0, Qt::AlignVCenter);
    tbLayout->addStretch();
    tbLayout->addWidget(_clear_btn,  0, Qt::AlignVCenter);

    // Log text area
    _text = new QPlainTextEdit(this);
    _text->setObjectName("log_text");
    _text->setReadOnly(true);
    _text->setWordWrapMode(QTextOption::NoWrap);
    QFont f = _text->font();
    f.setFamily("Monospace");
    f.setPixelSize(10);
    _text->setFont(f);

    // Main layout
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(toolbar);
    mainLayout->addWidget(_text, 1);

    // Timer: drain UI log buffer every 100 ms
    _timer = new QTimer(this);
    _timer->setInterval(100);
    _timer->start();

    connect(_timer,       &QTimer::timeout,              this, &LogDock::onTimer);
    connect(_clear_btn,   &QPushButton::clicked,         this, &LogDock::onClear);
    connect(_level_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogDock::onLevelChanged);

    ADD_UI(this);
}

LogDock::~LogDock()
{
    REMOVE_UI(this);
}

void LogDock::appendEntries(const QList<UiLogEntry> &entries)
{
    if (entries.isEmpty())
        return;

    bool atBottom = (_text->verticalScrollBar()->value() >=
                     _text->verticalScrollBar()->maximum() - 4);

    QTextCursor cursor = _text->textCursor();
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat fmt;

    for (const UiLogEntry &e : entries) {
        if (e.level > _filter_level)
            continue;

        switch (e.level) {
        case XLOG_LEVEL_ERR:
            fmt.setForeground(QColor("#e06c75"));
            break;
        case XLOG_LEVEL_WARN:
            fmt.setForeground(QColor("#e5c07b"));
            break;
        case XLOG_LEVEL_DBG:
        case XLOG_LEVEL_DETAIL:
            fmt.setForeground(QColor("#5c6370"));
            break;
        default:
            fmt.setForeground(_text->palette().color(QPalette::Text));
            break;
        }
        cursor.insertText(e.text + "\n", fmt);
    }

    _text->setTextCursor(cursor);
    if (atBottom)
        _text->verticalScrollBar()->setValue(_text->verticalScrollBar()->maximum());
}

void LogDock::rerenderAll()
{
    _text->clear();
    appendEntries(_history);
}

void LogDock::onTimer()
{
    QList<UiLogEntry> entries = dsv_take_ui_logs();
    if (entries.isEmpty())
        return;

    for (const UiLogEntry &e : entries) {
        if (_history.size() >= 2000)
            _history.removeFirst();
        _history.append(e);
    }

    appendEntries(entries);
}

void LogDock::onClear()
{
    dsv_take_ui_logs(); // drain pending buffer
    _history.clear();
    _text->clear();
}

void LogDock::onLevelChanged(int index)
{
    _filter_level = _level_combo->itemData(index).toInt();
    dsv_set_ui_log_level(_filter_level);
    rerenderAll();
}

void LogDock::UpdateLanguage()
{
    if (_level_label)
        _level_label->setText(tr("Level:"));
    _clear_btn->setText(tr("Clear"));
    int idx = _level_combo->currentIndex();
    _level_combo->clear();
    _level_combo->addItem(tr("Error"),   XLOG_LEVEL_ERR);
    _level_combo->addItem(tr("Warning"), XLOG_LEVEL_WARN);
    _level_combo->addItem(tr("Info"),    XLOG_LEVEL_INFO);
    _level_combo->addItem(tr("Debug"),   XLOG_LEVEL_DBG);
    _level_combo->addItem(tr("Verbose"), XLOG_LEVEL_DETAIL);
    _level_combo->setCurrentIndex(idx);
}

void LogDock::UpdateTheme()  {}
void LogDock::UpdateFont()   {}

} // namespace dock
} // namespace pv
