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

#include "searchdock.h"
#include "../sigsession.h"
#include "../view/cursor.h"
#include "../view/view.h"
#include "../view/timemarker.h"
#include "../view/ruler.h"
#include "../view/logicsignal.h"
#include "../data/snapshot.h"
#include "../data/logicsnapshot.h"
#include "../dialogs/dsmessagebox.h"
#include "../log.h"

#include <QObject>
#include <QPainter> 
#include <QRect>
#include <QFontMetrics>
#include <QFrame>
#include <QMouseEvent>
#include <QFuture>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>
#include <stdint.h> 
#include "../config/appconfig.h"
#include "../ui/msgbox.h"
#include "../appcontrol.h"
#include "../ui/fn.h"

namespace pv {
namespace dock {

using namespace pv::view;
using namespace pv::widgets;

SearchDock::SearchDock(QWidget *parent, View &view, SigSession *session) :
    QScrollArea(parent),
    _session(session),
    _view(view)
{
    setObjectName("search_dock_scroll");
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Shared validator reused across rebuilds to avoid leaking validator
    // instances on every device_updated() callback.
    QRegularExpression value_rx("[10XRFCxrfc]+");
    _value_validator = new QRegularExpressionValidator(value_rx, this);

    auto *host = new QWidget(this);
    host->setObjectName("search_dock_host");
    auto *root = new QVBoxLayout(host);
    root->setContentsMargins(16, 12, 16, 16);
    root->setSpacing(8);

    build_nav_bar(host);
    build_toggle_row(host);
    build_editor_container(host);
    root->addStretch(1);

    setWidget(host);

    connect(&_pre_button, SIGNAL(clicked()), this, SLOT(on_previous()));
    connect(&_nxt_button, SIGNAL(clicked()), this, SLOT(on_next()));
    connect(_session->device_event_object(), SIGNAL(device_updated()),
            this, SLOT(on_device_updated()));

    ADD_UI(this);

    // Build channel rows once at start; will be rebuilt on device_updated().
    rebuild_channel_rows();
    refresh_pattern_summary();
}

SearchDock::~SearchDock()
{
    REMOVE_UI(this);
}

void SearchDock::build_nav_bar(QWidget *host)
{
    auto *root = qobject_cast<QVBoxLayout *>(host->layout());

    // Disabled search icon (inside pattern container).
    _search_button = new QPushButton(host);
    _search_button->setFixedSize(14, 14);
    _search_button->setDisabled(true);
    _search_button->setObjectName("search_icon_btn");

    // Hidden QLineEdit parent — FakeLineEdit needs a QLineEdit parent for paint
    // forwarding; not visible to the user.
    QLineEdit *summary_parent = new QLineEdit(host);
    summary_parent->setVisible(false);
    _search_value = new widgets::FakeLineEdit(summary_parent);
    _search_value->setObjectName("search_pattern_edit");
    _search_value->setReadOnly(true);
    _search_value->setFrame(false);

    auto *pattern_container = new QWidget(host);
    pattern_container->setObjectName("search_pattern_container");
    auto *pc_layout = new QHBoxLayout(pattern_container);
    pc_layout->setContentsMargins(6, 2, 6, 2);
    pc_layout->setSpacing(4);
    pc_layout->addWidget(_search_button);
    pc_layout->addWidget(_search_value, 1);

    _pre_button.setParent(host);
    _pre_button.setObjectName("search_nav_btn");
    _pre_button.setFixedSize(24, 24);
    _pre_button.setIconSize(QSize(16, 16));
    _nxt_button.setParent(host);
    _nxt_button.setObjectName("search_nav_btn");
    _nxt_button.setFixedSize(24, 24);
    _nxt_button.setIconSize(QSize(16, 16));

    auto *nav_bar = new QWidget(host);
    nav_bar->setObjectName("search_nav_bar");
    auto *nb = new QHBoxLayout(nav_bar);
    nb->setContentsMargins(0, 0, 0, 0);
    nb->setSpacing(8);
    nb->addWidget(&_pre_button);
    nb->addWidget(pattern_container, 1);
    nb->addWidget(&_nxt_button);

    root->addWidget(nav_bar);

    // Clicking the summary expands the editor (preserves the old "click pattern
    // to configure" muscle memory).
    connect(_search_value, &widgets::FakeLineEdit::trigger, this,
            [this]() { on_toggle_editor(/*force_expand=*/true); });
}

void SearchDock::build_toggle_row(QWidget *host)
{
    auto *root = qobject_cast<QVBoxLayout *>(host->layout());

    auto *row = new QWidget(host);
    row->setObjectName("search_editor_toggle");
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);

    _toggle_btn = new QToolButton(row);
    _toggle_btn->setObjectName("search_editor_toggle_btn");
    _toggle_btn->setCheckable(true);
    _toggle_btn->setChecked(false);
    _toggle_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _toggle_btn->setText(tr("Edit per-channel"));
    _toggle_btn->setIcon(QIcon(":/icons/sidebar/chevron-right.svg"));
    _toggle_btn->setIconSize(QSize(12, 12));
    _toggle_btn->setAutoRaise(true);

    connect(_toggle_btn, &QToolButton::toggled, this,
            [this](bool /*on*/) { on_toggle_editor(/*force_expand=*/false); });

    h->addWidget(_toggle_btn);
    h->addStretch(1);
    root->addWidget(row);
}

void SearchDock::build_editor_container(QWidget *host)
{
    auto *root = qobject_cast<QVBoxLayout *>(host->layout());

    _editor_container = new QWidget(host);
    _editor_container->setObjectName("search_editor_container");
    _editor_container->setVisible(false);

    // Horizontal split: channel grid on the left (tightly packed against the
    // panel edge), legend pinned to the right side.
    auto *h = new QHBoxLayout(_editor_container);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(16);

    // Wrap the grid so the three columns claim only their natural width
    // instead of expanding to fill the full panel width.
    auto *grid_wrap = new QWidget(_editor_container);
    grid_wrap->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    auto *gv = new QVBoxLayout(grid_wrap);
    gv->setContentsMargins(0, 0, 0, 0);
    gv->setSpacing(0);

    _channel_grid = new QGridLayout();
    _channel_grid->setVerticalSpacing(4);
    _channel_grid->setHorizontalSpacing(8);
    _channel_grid->setContentsMargins(0, 0, 0, 0);
    // No column stretch — keep name / idx / input snug together on the left.
    _channel_grid->setColumnStretch(0, 0);
    _channel_grid->setColumnStretch(1, 0);
    _channel_grid->setColumnStretch(2, 0);
    gv->addLayout(_channel_grid);
    gv->addStretch(1);

    _legend_lbl = new QLabel(
        tr("X: Don't care\n0: Low level\n1: High level\n"
           "R: Rising edge\nF: Falling edge\nC: Rising/Falling edge"),
        _editor_container);
    _legend_lbl->setObjectName("search_dock_legend");
    _legend_lbl->setWordWrap(false);
    _legend_lbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    h->addWidget(grid_wrap, 0, Qt::AlignTop | Qt::AlignLeft);
    h->addStretch(1);
    h->addWidget(_legend_lbl, 0, Qt::AlignTop);

    root->addWidget(_editor_container);
}

void SearchDock::rebuild_channel_rows()
{
    assert(_channel_grid);

    // Snapshot user's current pattern values by channel index so we can carry
    // them across the rebuild.
    std::map<uint16_t, QString> snapshot = _pattern;

    // Wipe out previous rows (widgets and items).
    QLayoutItem *it = nullptr;
    while ((it = _channel_grid->takeAt(0)) != nullptr) {
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }
    _channel_edits.clear();
    _channel_indices.clear();

    QFont monoFont("Monaco");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setFixedPitch(true);

    int row = 0;
    std::map<uint16_t, QString> new_pattern;
    for (auto s : _session->get_signals()) {
        if (s->signal_type() != SR_CHANNEL_LOGIC)
            continue;

        auto *logicSig = static_cast<view::LogicSignal *>(s);
        const uint16_t idx = logicSig->get_index();

        auto *name_lbl = new QLabel(logicSig->get_name() + ":", _editor_container);
        name_lbl->setObjectName("search_dock_ch_name");

        auto *idx_lbl = new QLabel(QString::number(idx), _editor_container);
        idx_lbl->setObjectName("search_dock_ch_idx");

        auto *edit = new widgets::SearchEdgeFlagEdit(_editor_container);
        edit->setObjectName("search_dock_ch_edit");
        const QString initial = (snapshot.find(idx) != snapshot.end())
            ? snapshot[idx]
            : QStringLiteral("X");
        edit->setText(initial);
        edit->setValidator(_value_validator);
        edit->setMaxLength(1);
        edit->setInputMask("X");
        edit->setFont(monoFont);
        edit->setAlignment(Qt::AlignCenter);
        edit->setFixedWidth(60);

        connect(edit, SIGNAL(editingFinished()),
                this, SLOT(on_channel_edit_finished()));

        _channel_grid->addWidget(name_lbl, row, 0, Qt::AlignRight | Qt::AlignVCenter);
        _channel_grid->addWidget(idx_lbl,  row, 1, Qt::AlignRight | Qt::AlignVCenter);
        _channel_grid->addWidget(edit,     row, 2);

        _channel_edits.push_back(edit);
        _channel_indices.push_back(idx);

        new_pattern[idx] = initial;
        ++row;
    }

    if (row == 0) {
        dsv_info("SearchDock: no logic signals; editor stays empty");
    }

    // Replace _pattern with the rebuilt one (purges stale keys, adds new defaults).
    _pattern = new_pattern;
}

void SearchDock::refresh_pattern_summary()
{
    QString label;
    for (auto &kv : _pattern) {
        QString v = kv.second;
        v.remove(QChar(' '), Qt::CaseInsensitive);
        v = v.toUpper();
        label.push_back(v);
    }
    _search_value->setText(label);

    QFontMetrics fm = this->fontMetrics();
    const int tw = fm.boundingRect(label).width();
    _search_value->setFixedWidth(tw + _search_button->width() + 20);
}

void SearchDock::on_toggle_editor(bool force_expand)
{
    if (force_expand) {
        _editor_expanded = true;
    } else {
        _editor_expanded = _toggle_btn->isChecked();
    }
    // Sync the toggle button state (without re-emitting toggled()).
    if (_toggle_btn->isChecked() != _editor_expanded) {
        QSignalBlocker blocker(_toggle_btn);
        _toggle_btn->setChecked(_editor_expanded);
    }
    _toggle_btn->setIcon(QIcon(_editor_expanded
        ? ":/icons/sidebar/chevron-down.svg"
        : ":/icons/sidebar/chevron-right.svg"));
    _editor_container->setVisible(_editor_expanded);
}

void SearchDock::on_channel_edit_finished()
{
    auto *src = qobject_cast<QLineEdit *>(sender());
    if (src) src->setText(src->text().toUpper());

    std::map<uint16_t, QString> new_pattern;
    for (int i = 0; i < _channel_edits.size(); ++i) {
        new_pattern[_channel_indices[i]] = _channel_edits[i]->text();
    }
    if (new_pattern != _pattern) {
        _view.set_search_pos(_view.get_search_pos(), false);
        _pattern = new_pattern;
        refresh_pattern_summary();
    }
}

void SearchDock::on_device_updated()
{
    rebuild_channel_rows();
    refresh_pattern_summary();
}

void SearchDock::retranslateUi()
{
    if (_search_value)
        _search_value->setPlaceholderText(tr("search"));
    if (_toggle_btn)
        _toggle_btn->setText(tr("Edit per-channel"));
    if (_legend_lbl)
        _legend_lbl->setText(
            tr("X: Don't care\n0: Low level\n1: High level\n"
               "R: Rising edge\nF: Falling edge\nC: Rising/Falling edge"));
}

void SearchDock::reStyle()
{
    _pre_button.setIcon(QIcon(":/icons/sidebar/chevron-left.svg"));
    _nxt_button.setIcon(QIcon(":/icons/sidebar/chevron-right.svg"));
    if (_search_button) {
        _search_button->setIcon(QIcon(":/icons/sidebar/search.svg"));
        _search_button->setIconSize(QSize(14, 14));
    }
    if (_toggle_btn) {
        _toggle_btn->setIcon(QIcon(_editor_expanded
            ? ":/icons/sidebar/chevron-down.svg"
            : ":/icons/sidebar/chevron-right.svg"));
    }
}

void SearchDock::on_previous()
{
    bool ret;
    int64_t last_pos;
    bool last_hit;
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    assert(snapshot);
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot);

    if (logic_snapshot == NULL || logic_snapshot->empty()) {
        QString strMsg(tr("No Sample data!"));
        MsgBox::Show(strMsg);        
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    last_pos = _view.get_search_pos();
    last_hit = _view.get_search_hit();
    if (last_pos == 0) {
        QString strMsg(tr("Search cursor at the start position!"));
        MsgBox::Show(strMsg);
        return;
    }
    else {
        QFuture<void> future;
        future = QtConcurrent::run([&]{
            last_pos -= last_hit;
            ret = logic_snapshot->pattern_search(0, end, last_pos, _pattern, false);
        });
        Qt::WindowFlags flags = Qt::CustomizeWindowHint;
        QProgressDialog dlg(tr("Search Previous..."),
                            tr("Cancel"),0,0,this,flags);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                           Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        dlg.setCancelButton(NULL);

        QFutureWatcher<void> watcher;
        connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
        watcher.setFuture(future);
        dlg.exec();

        if (!ret) {
            QString strMsg(tr("Pattern not found!"));
            MsgBox::Show(strMsg);
            return;
        } else {
            _view.set_search_pos(last_pos, true);
        }
    }
}

void SearchDock::on_next()
{
    bool ret;
    int64_t last_pos;
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    assert(snapshot);
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot);

    if (logic_snapshot == NULL || logic_snapshot->empty()) {
        QString strMsg(tr("No Sample data!"));
        MsgBox::Show(strMsg);
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    last_pos = _view.get_search_pos() + _view.get_search_hit();
    if (last_pos >= end) {
        QString strMsg(tr("Search cursor at the end position!"));
        MsgBox::Show(strMsg);
        return;
    } else {
        QFuture<void> future;
        future = QtConcurrent::run([&]{
            ret = logic_snapshot->pattern_search(0, end, last_pos, _pattern, true);
        });
        Qt::WindowFlags flags = Qt::CustomizeWindowHint;
        QProgressDialog dlg(tr("Search Next..."),
                            tr("Cancel"),0,0,this,flags);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                           Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        dlg.setCancelButton(NULL);

        QFutureWatcher<void> watcher;
        connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
        watcher.setFuture(future);
        dlg.exec();

        if (!ret) {
            QString strMsg(tr("Pattern not found!"));
            MsgBox::Show(strMsg);
            return;
        } else {
            _view.set_search_pos(last_pos, true);
        }
    }
}

void SearchDock::UpdateLanguage()
{
    retranslateUi();
}

void SearchDock::UpdateTheme()
{
    reStyle();
}

void SearchDock::UpdateFont()
{
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    if (_search_value) _search_value->setFont(font);
    if (_toggle_btn)   _toggle_btn->setFont(font);
    if (_legend_lbl)   _legend_lbl->setFont(font);

    QFont monoFont("Monaco");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setFixedPitch(true);
    monoFont.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    for (auto *e : _channel_edits) {
        if (e) e->setFont(monoFont);
    }
}

} // namespace dock
} // namespace pv
