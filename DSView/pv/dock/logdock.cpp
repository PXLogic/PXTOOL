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
#include "logsearch.h"
#include <QCheckBox>
#include <QColor>
#include <QEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QToolButton>
#include <log/xlog.h>

namespace pv {
namespace dock {

LogDock::LogDock(QWidget *parent)
    : QWidget(parent)
    , _filter_level(XLOG_LEVEL_INFO)
    , _current_search_match(-1)
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

    _search_btn = new QPushButton(tr("Search"), toolbar);
    _search_btn->setObjectName("log_search_btn");
    _search_btn->setFixedHeight(24);

    _search_edit = new QLineEdit(toolbar);
    _search_edit->setObjectName("log_search_edit");
    _search_edit->setPlaceholderText(tr("Search log"));
    _search_edit->setClearButtonEnabled(true);
    _search_edit->setFixedHeight(24);
    _search_edit->setEnabled(true);
    _search_edit->setReadOnly(false);
    _search_edit->setFocusPolicy(Qt::StrongFocus);
    _search_edit->setAttribute(Qt::WA_InputMethodEnabled, true);
    _search_edit->installEventFilter(this);

    _exact_check = new QCheckBox(tr("Exact"), toolbar);
    _exact_check->setObjectName("log_search_exact");

    _search_prev_btn = new QToolButton(toolbar);
    _search_prev_btn->setObjectName("log_search_prev");
    _search_prev_btn->setText("<");
    _search_prev_btn->setFixedSize(24, 24);

    _search_next_btn = new QToolButton(toolbar);
    _search_next_btn->setObjectName("log_search_next");
    _search_next_btn->setText(">");
    _search_next_btn->setFixedSize(24, 24);

    _search_counter = new QLabel("0/0", toolbar);
    _search_counter->setObjectName("log_search_counter");

    _clear_btn = new QPushButton(tr("Clear"), toolbar);
    _clear_btn->setObjectName("log_clear_btn");
    _clear_btn->setFixedHeight(24);

    tbLayout->addWidget(levelLabel,  0, Qt::AlignVCenter);
    tbLayout->addWidget(_level_combo, 0, Qt::AlignVCenter);
    tbLayout->addWidget(_search_btn, 0, Qt::AlignVCenter);
    tbLayout->addWidget(_search_edit, 1, Qt::AlignVCenter);
    tbLayout->addWidget(_exact_check, 0, Qt::AlignVCenter);
    tbLayout->addWidget(_search_prev_btn, 0, Qt::AlignVCenter);
    tbLayout->addWidget(_search_next_btn, 0, Qt::AlignVCenter);
    tbLayout->addWidget(_search_counter, 0, Qt::AlignVCenter);
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
    connect(_search_btn, &QPushButton::clicked,
            this, &LogDock::onSearchClicked);
    connect(_search_edit, &QLineEdit::textChanged,
            this, &LogDock::onSearchTextChanged);
    connect(_exact_check, &QCheckBox::toggled,
            this, &LogDock::onExactSearchChanged);
    connect(_search_prev_btn, &QToolButton::clicked,
            this, &LogDock::onSearchPrevious);
    connect(_search_next_btn, &QToolButton::clicked,
            this, &LogDock::onSearchNext);

    ADD_UI(this);
}

LogDock::~LogDock()
{
    REMOVE_UI(this);
}

bool LogDock::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _search_edit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier)
                onSearchPrevious();
            else
                onSearchNext();
            return true;
        }

        return false;
    }

    return QWidget::eventFilter(watched, event);
}

void LogDock::appendEntries(const QList<UiLogEntry> &entries)
{
    if (entries.isEmpty())
        return;

    bool atBottom = (_text->verticalScrollBar()->value() >=
                     _text->verticalScrollBar()->maximum() - 4);

    const QTextCursor originalCursor = _text->textCursor();
    const int originalScroll = _text->verticalScrollBar()->value();

    QTextCursor cursor(_text->document());
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

    if (atBottom)
        _text->verticalScrollBar()->setValue(_text->verticalScrollBar()->maximum());

    refreshSearch();
    if (atBottom) {
        _text->verticalScrollBar()->setValue(_text->verticalScrollBar()->maximum());
    } else {
        _text->setTextCursor(originalCursor);
        _text->verticalScrollBar()->setValue(originalScroll);
    }
}

void LogDock::rerenderAll()
{
    bool atBottom = (_text->verticalScrollBar()->value() >=
                     _text->verticalScrollBar()->maximum() - 4);
    _text->clear();
    appendEntries(_history);
    refreshSearch();
    if (atBottom)
        _text->verticalScrollBar()->setValue(_text->verticalScrollBar()->maximum());
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
    _search_matches.clear();
    _current_search_match = -1;
    updateSearchHighlights();
    updateSearchCounter();
}

void LogDock::onLevelChanged(int index)
{
    _filter_level = _level_combo->itemData(index).toInt();
    dsv_set_ui_log_level(_filter_level);
    rerenderAll();
}

void LogDock::onSearchClicked()
{
    if (_search_edit)
        _search_edit->setFocus();
}

void LogDock::onSearchTextChanged(const QString &)
{
    refreshSearch();
}

void LogDock::onExactSearchChanged(bool)
{
    refreshSearch();
}

void LogDock::onSearchNext()
{
    if (_search_matches.isEmpty())
        return;

    int next = _current_search_match + 1;
    if (next >= _search_matches.size())
        next = 0;
    gotoSearchMatch(next);
}

void LogDock::onSearchPrevious()
{
    if (_search_matches.isEmpty())
        return;

    int previous = _current_search_match - 1;
    if (previous < 0)
        previous = _search_matches.size() - 1;
    gotoSearchMatch(previous);
}

void LogDock::refreshSearch()
{
    if (!_text || !_search_edit || !_exact_check) {
        return;
    }

    const int previousMatch = _current_search_match;
    _search_matches = findLogSearchMatches(_text->toPlainText(),
                                           _search_edit->text(),
                                           _exact_check->isChecked());

    if (_search_matches.isEmpty())
        _current_search_match = -1;
    else if (previousMatch >= 0 && previousMatch < _search_matches.size())
        _current_search_match = previousMatch;
    else
        _current_search_match = 0;

    updateSearchHighlights();
    updateSearchCounter();
}

void LogDock::updateSearchHighlights()
{
    if (!_text)
        return;

    QList<QTextEdit::ExtraSelection> selections;
    QTextCharFormat matchFormat;
    matchFormat.setBackground(QColor(255, 230, 120, 140));

    QTextCharFormat currentFormat;
    currentFormat.setBackground(QColor(255, 170, 40, 190));

    for (int i = 0; i < _search_matches.size(); ++i) {
        const LogSearchMatch &match = _search_matches.at(i);
        QTextCursor cursor(_text->document());
        cursor.setPosition(match.position);
        cursor.setPosition(match.position + match.length, QTextCursor::KeepAnchor);

        QTextEdit::ExtraSelection selection;
        selection.cursor = cursor;
        selection.format = (i == _current_search_match) ? currentFormat : matchFormat;
        selections.append(selection);
    }

    _text->setExtraSelections(selections);
}

void LogDock::updateSearchCounter()
{
    if (!_search_counter)
        return;

    if (_search_matches.isEmpty() || _current_search_match < 0)
        _search_counter->setText("0/0");
    else
        _search_counter->setText(QString("%1/%2")
                                 .arg(_current_search_match + 1)
                                 .arg(_search_matches.size()));

    const bool hasMatches = !_search_matches.isEmpty();
    if (_search_prev_btn)
        _search_prev_btn->setEnabled(hasMatches);
    if (_search_next_btn)
        _search_next_btn->setEnabled(hasMatches);
}

void LogDock::gotoSearchMatch(int index)
{
    if (!_text || index < 0 || index >= _search_matches.size())
        return;

    _current_search_match = index;
    const LogSearchMatch &match = _search_matches.at(index);

    QTextCursor cursor(_text->document());
    cursor.setPosition(match.position);
    cursor.setPosition(match.position + match.length, QTextCursor::KeepAnchor);
    _text->setTextCursor(cursor);
    _text->ensureCursorVisible();

    updateSearchHighlights();
    updateSearchCounter();
}

void LogDock::UpdateLanguage()
{
    if (_level_label)
        _level_label->setText(tr("Level:"));
    if (_search_btn)
        _search_btn->setText(tr("Search"));
    if (_search_edit)
        _search_edit->setPlaceholderText(tr("Search log"));
    if (_exact_check)
        _exact_check->setText(tr("Exact"));
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
