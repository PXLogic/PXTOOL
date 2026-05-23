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
    QWidget(parent),
    _session(session),
    _view(&view)
{
    setObjectName("search_dock_widget");

    // Shared validator reused across rebuilds.
    QRegularExpression value_rx("[10XRFCxrfc]+");
    _value_validator = new QRegularExpressionValidator(value_rx, this);

    // Outer layout: controls scroll on top, results list expands below.
    _outer_layout = new QVBoxLayout(this);
    auto *outer = _outer_layout;
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // --- Controls scroll area (nav bar + toggle + editor grid) ---
    _controls_scroll = new QScrollArea(this);
    _controls_scroll->setObjectName("search_dock_scroll");
    _controls_scroll->setWidgetResizable(true);
    _controls_scroll->setFrameShape(QFrame::NoFrame);
    _controls_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _controls_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _controls_scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    auto *host = new QWidget(_controls_scroll);
    host->setObjectName("search_dock_host");
    auto *root = new QVBoxLayout(host);
    root->setContentsMargins(16, 12, 16, 8);
    root->setSpacing(8);

    build_nav_bar(host);
    build_toggle_row(host);
    build_editor_container(host);
    root->addStretch(1);

    _controls_scroll->setWidget(host);
    outer->addWidget(_controls_scroll, 0);

    // --- Results panel (outside scroll area so its scrollbar works) ---
    build_results_panel();
    outer->addWidget(_results_panel, 1);

    connect(&_pre_button, SIGNAL(clicked()), this, SLOT(on_previous()));
    connect(&_nxt_button, SIGNAL(clicked()), this, SLOT(on_next()));
    connect(_session->device_event_object(), SIGNAL(device_updated()),
            this, SLOT(on_device_updated()));
    connect(_search_all_btn, SIGNAL(clicked()), this, SLOT(on_search_all()));

    ADD_UI(this);

    rebuild_channel_rows();
    refresh_pattern_summary();
    update_results_size();
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

    _search_all_btn = new QPushButton(tr("Search All"), host);
    _search_all_btn->setObjectName("search_all_btn");
    _search_all_btn->setFixedHeight(28);
    _search_all_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    root->addWidget(nav_bar);
    root->addWidget(_search_all_btn);

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

void SearchDock::build_results_panel()
{
    _results_panel = new QWidget(this);
    _results_panel->setObjectName("search_results_panel");
    _results_panel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto *vl = new QVBoxLayout(_results_panel);
    vl->setContentsMargins(16, 4, 16, 8);
    vl->setSpacing(4);

    // Header row: label + count
    auto *header = new QWidget(_results_panel);
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);

    _results_title_lbl = new QLabel(tr("Results"), header);
    _results_title_lbl->setObjectName("search_results_title");

    _result_count_lbl = new QLabel(tr("–"), header);
    _result_count_lbl->setObjectName("search_result_count");

    hl->addWidget(_results_title_lbl);
    hl->addWidget(_result_count_lbl);
    hl->addStretch(1);

    _result_table = new QTableWidget(0, 3, _results_panel);
    _result_table->setObjectName("search_result_table");
    _result_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _result_table->setSelectionMode(QAbstractItemView::SingleSelection);
    _result_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _result_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _result_table->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    _result_table->setShowGrid(false);
    _result_table->setAlternatingRowColors(true);
    _result_table->verticalHeader()->setVisible(false);
    _result_table->verticalHeader()->setDefaultSectionSize(22);

    _result_table->setHorizontalHeaderLabels({tr("#"), tr("Time"), tr("Sample")});
    auto *hdr = _result_table->horizontalHeader();
    hdr->setSectionResizeMode(0, QHeaderView::Stretch);
    hdr->setSectionResizeMode(1, QHeaderView::Stretch);
    hdr->setSectionResizeMode(2, QHeaderView::Stretch);
    hdr->setHighlightSections(false);

    vl->addWidget(header);
    vl->addWidget(_result_table, 1);

    connect(_result_table, &QTableWidget::cellClicked, this, [this](int row, int /*col*/) {
        if (row >= 0 && row < _result_positions.size())
            navigate_to_result(row);
    });
}

void SearchDock::update_results_size()
{
    if (_editor_expanded) {
        // Editor expanded: controls_scroll and results list split the space.
        // stretch=1 / stretch=1 so both grow proportionally.
        _controls_scroll->setMaximumHeight(QWIDGETSIZE_MAX);
        _controls_scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        _result_table->setMaximumHeight(220);
        _result_table->setMinimumHeight(60);
        _outer_layout->setStretch(0, 1);
        _outer_layout->setStretch(1, 0);
    } else {
        // Editor collapsed: controls area is compact (nav + search_all_btn + toggle only).
        // results table fills all remaining space.
        _controls_scroll->setMaximumHeight(104);
        _controls_scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        _result_table->setMaximumHeight(QWIDGETSIZE_MAX);
        _result_table->setMinimumHeight(60);
        _outer_layout->setStretch(0, 0);
        _outer_layout->setStretch(1, 1);
    }
    _controls_scroll->updateGeometry();
    _results_panel->updateGeometry();
}

void SearchDock::run_full_search()
{
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    if (!snapshot) return;
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot *>(snapshot);
    if (!logic_snapshot || logic_snapshot->empty()) {
        MsgBox::Show(tr("No Sample data!"));
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    std::vector<int64_t> raw_results;

    QFuture<void> future = QtConcurrent::run([&]{
        logic_snapshot->pattern_search_all(0, end, _pattern, raw_results, 10000);
    });

    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Searching…"), tr("Cancel"), 0, 0, this, flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    dlg.setCancelButton(nullptr);
    QFutureWatcher<void> watcher;
    connect(&watcher, SIGNAL(finished()), &dlg, SLOT(cancel()));
    watcher.setFuture(future);
    dlg.exec();

    _result_positions.clear();
    _result_current = -1;
    for (int64_t pos : raw_results)
        _result_positions.push_back(pos);

    populate_result_table();

    if (_result_positions.isEmpty())
        MsgBox::Show(tr("Pattern not found!"));
    else
        navigate_to_result(0);
}

void SearchDock::populate_result_table()
{
    _result_table->setUpdatesEnabled(false);
    _result_table->setRowCount(0);

    const uint64_t sr = _session->cur_snap_samplerate();
    _result_table->setRowCount(_result_positions.size());
    for (int i = 0; i < _result_positions.size(); ++i) {
        int64_t pos = _result_positions[i];

        auto *idx_item  = new QTableWidgetItem(QString::number(i + 1));
        auto *time_item = new QTableWidgetItem(format_time(pos, sr));
        auto *smp_item  = new QTableWidgetItem(QString::number(pos));

        idx_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        time_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        smp_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        _result_table->setItem(i, 0, idx_item);
        _result_table->setItem(i, 1, time_item);
        _result_table->setItem(i, 2, smp_item);
    }
    _result_table->setUpdatesEnabled(true);

    if (_result_positions.isEmpty()) {
        _result_count_lbl->setText(tr("–"));
    } else {
        _result_count_lbl->setText(tr("%1 results").arg(_result_positions.size()));
        _result_table->updateGeometry();
        _result_table->update();
        if (_result_current >= 0 && _result_current < _result_positions.size()) {
            _result_table->selectRow(_result_current);
            _result_table->scrollTo(_result_table->model()->index(_result_current, 0),
                                    QAbstractItemView::EnsureVisible);
        }
    }
}

void SearchDock::clear_results()
{
    _result_table->setRowCount(0);
    _result_positions.clear();
    _result_current = -1;
    _result_count_lbl->setText(tr("–"));
}

void SearchDock::navigate_to_result(int idx)
{
    if (idx < 0 || idx >= _result_positions.size()) return;
    _result_current = idx;
    _result_table->selectRow(idx);
    _result_table->scrollTo(_result_table->model()->index(idx, 0),
                            QAbstractItemView::EnsureVisible);
    _view->zoom_to_search_pos((uint64_t)_result_positions[idx]);
}

/* static */ QString SearchDock::format_time(int64_t sample, uint64_t samplerate)
{
    if (samplerate == 0) return QString::number(sample);
    double time_s = (double)sample / (double)samplerate;
    if (time_s < 1e-6)
        return QString("%1 ns").arg(time_s * 1e9, 0, 'f', 1);
    if (time_s < 1e-3)
        return QString("%1 µs").arg(time_s * 1e6, 0, 'f', 2);
    if (time_s < 1.0)
        return QString("%1 ms").arg(time_s * 1e3, 0, 'f', 3);
    return QString("%1 s").arg(time_s, 0, 'f', 6);
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
    update_results_size();
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
        _view->set_search_pos(_view->get_search_pos(), false);
        _pattern = new_pattern;
        refresh_pattern_summary();
        clear_results();
    }
}

void SearchDock::save_state(SigSession *s)
{
    if (!s) return;
    SearchState &st     = _session_states[s];
    st.pattern          = _pattern;
    st.result_positions = _result_positions;
    st.result_current   = _result_current;
    st.editor_expanded  = _editor_expanded;
}

void SearchDock::restore_state(SigSession *s)
{
    if (!s) return;
    if (!_session_states.contains(s)) {
        rebuild_channel_rows();
        refresh_pattern_summary();
        clear_results();
        return;
    }
    const SearchState &st = _session_states[s];
    _pattern              = st.pattern;
    _result_positions     = st.result_positions;
    _result_current       = st.result_current;

    // Rebuild channel rows using the restored _pattern values.
    rebuild_channel_rows();
    refresh_pattern_summary();

    // Restore toggle state without re-triggering the toggled() signal.
    {
        QSignalBlocker blocker(_toggle_btn);
        _toggle_btn->setChecked(st.editor_expanded);
    }
    on_toggle_editor(false);

    // Repopulate the results table (no search needed).
    populate_result_table();
}

void SearchDock::setSession(SigSession *session)
{
    if (!session || _session == session)
        return;
    save_state(_session);
    disconnect(_session->device_event_object(), SIGNAL(device_updated()),
               this, SLOT(on_device_updated()));
    _session = session;
    connect(_session->device_event_object(), SIGNAL(device_updated()),
            this, SLOT(on_device_updated()));
    restore_state(_session);
}

void SearchDock::setView(view::View *view)
{
    if (view && _view != view)
        _view = view;
}

void SearchDock::on_device_updated()
{
    // Device/channel layout changed — stale saved state for this session is no
    // longer valid (channel count or indices may differ).
    _session_states.remove(_session);
    rebuild_channel_rows();
    refresh_pattern_summary();
    clear_results();
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
    if (_search_all_btn)
        _search_all_btn->setText(tr("Search All"));
    if (_results_title_lbl)
        _results_title_lbl->setText(tr("Results"));
    if (_result_table) {
        _result_table->setHorizontalHeaderLabels(
            {tr("#"), tr("Time"), tr("Sample")});
    }
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
    // If the results list is populated, navigate it.
    if (!_result_positions.isEmpty()) {
        int idx = (_result_current > 0) ? _result_current - 1 : 0;
        if (idx == _result_current) {
            MsgBox::Show(tr("Already at the first result."));
            return;
        }
        navigate_to_result(idx);
        return;
    }

    // Fall back to single-step backward search.
    bool ret;
    int64_t last_pos;
    bool last_hit;
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    assert(snapshot);
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot);

    if (logic_snapshot == NULL || logic_snapshot->empty()) {
        MsgBox::Show(tr("No Sample data!"));
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    last_pos = _view->get_search_pos();
    last_hit = _view->get_search_hit();
    if (last_pos == 0) {
        MsgBox::Show(tr("Search cursor at the start position!"));
        return;
    }

    QFuture<void> future = QtConcurrent::run([&]{
        last_pos -= last_hit;
        ret = logic_snapshot->pattern_search(0, end, last_pos, _pattern, false);
    });
    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Search Previous..."), tr("Cancel"), 0, 0, this, flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                       Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    dlg.setCancelButton(nullptr);
    QFutureWatcher<void> watcher;
    connect(&watcher, SIGNAL(finished()), &dlg, SLOT(cancel()));
    watcher.setFuture(future);
    dlg.exec();

    if (!ret) {
        MsgBox::Show(tr("Pattern not found!"));
    } else {
        _view->set_search_pos(last_pos, true);
    }
}

void SearchDock::on_next()
{
    // If the results list is populated, navigate it.
    if (!_result_positions.isEmpty()) {
        int idx = (_result_current >= 0) ? _result_current + 1 : 0;
        if (idx >= _result_positions.size()) {
            MsgBox::Show(tr("Already at the last result."));
            return;
        }
        navigate_to_result(idx);
        return;
    }

    // Fall back to single-step forward search.
    bool ret;
    int64_t last_pos;
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    assert(snapshot);
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot);

    if (logic_snapshot == NULL || logic_snapshot->empty()) {
        MsgBox::Show(tr("No Sample data!"));
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    last_pos = _view->get_search_pos() + _view->get_search_hit();
    if (last_pos >= end) {
        MsgBox::Show(tr("Search cursor at the end position!"));
        return;
    }

    QFuture<void> future = QtConcurrent::run([&]{
        ret = logic_snapshot->pattern_search(0, end, last_pos, _pattern, true);
    });
    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Search Next..."), tr("Cancel"), 0, 0, this, flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                       Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    dlg.setCancelButton(nullptr);
    QFutureWatcher<void> watcher;
    connect(&watcher, SIGNAL(finished()), &dlg, SLOT(cancel()));
    watcher.setFuture(future);
    dlg.exec();

    if (!ret) {
        MsgBox::Show(tr("Pattern not found!"));
    } else {
        _view->set_search_pos(last_pos, true);
    }
}

void SearchDock::on_search_all()
{
    run_full_search();
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
    if (_result_table) _result_table->setFont(monoFont);
    if (_result_count_lbl) _result_count_lbl->setFont(font);
}

} // namespace dock
} // namespace pv
