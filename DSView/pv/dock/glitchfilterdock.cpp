/*
 * This file is part of the DSView project.
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "glitchfilterdock.h"
#include "../sigsession.h"
#include "../data/logicsnapshot.h"
#include <libsigrok.h>
#include <QScrollBar>
#include <QShowEvent>

namespace pv {
namespace dock {

GlitchFilterDock::GlitchFilterDock(QWidget *parent, SigSession *session)
    : QScrollArea(parent)
    , _session(session)
    , _worker(nullptr)
{
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    buildUi();
    ADD_UI(this);
    if (_session) reload();
}

GlitchFilterDock::~GlitchFilterDock()
{
    if (_worker) {
        _worker->disconnect(this);     // I1: drop pending queued events
        _worker->wait();
        delete _worker;
        _worker = nullptr;
    }
    if (_worker_session) {
        _worker_session->set_filter_in_progress(false);
        _worker_session = nullptr;
    }
    REMOVE_UI(this);
}

void GlitchFilterDock::buildUi()
{
    _content_widget = new QWidget(this);
    _content_widget->setObjectName("glitchFilterWidget");
    setWidget(_content_widget);

    auto *main_layout = new QVBoxLayout(_content_widget);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(8);

    _help_group = new QGroupBox(_content_widget);
    auto *help_layout = new QVBoxLayout(_help_group);
    _help_label = new QLabel(_help_group);
    _help_label->setWordWrap(true);
    help_layout->addWidget(_help_label);
    main_layout->addWidget(_help_group);

    _ch_group  = new QGroupBox(_content_widget);
    _ch_grid   = new QGridLayout(_ch_group);
    _ch_grid->setContentsMargins(8, 8, 8, 8);
    _ch_grid->setSpacing(4);

    _hdr_ch        = new QLabel(_ch_group);
    _hdr_enable    = new QLabel(_ch_group);
    _hdr_threshold = new QLabel(_ch_group);
    _hdr_unit      = new QLabel(_ch_group);
    _ch_grid->addWidget(_hdr_ch,        0, 0);
    _ch_grid->addWidget(_hdr_enable,    0, 1);
    _ch_grid->addWidget(_hdr_threshold, 0, 2);
    _ch_grid->addWidget(_hdr_unit,      0, 3);

    for (int i = 0; i < MaxChannels; i++) {
        ChannelRow row;
        row.ch_label     = new QLabel(QString("D%1").arg(i), _ch_group);
        row.enable_cb    = new QCheckBox(_ch_group);
        row.threshold_sb = new QSpinBox(_ch_group);
        row.threshold_sb->setRange(1, 9999);
        row.threshold_sb->setValue(3);
        row.threshold_sb->setEnabled(false);
        row.unit_label   = new QLabel(_ch_group);

        _ch_grid->addWidget(row.ch_label,     i + 1, 0);
        _ch_grid->addWidget(row.enable_cb,    i + 1, 1);
        _ch_grid->addWidget(row.threshold_sb, i + 1, 2);
        _ch_grid->addWidget(row.unit_label,   i + 1, 3);

        connect(row.enable_cb, &QCheckBox::toggled, this,
                [this, i](bool checked) {
                    _ch_rows[i].threshold_sb->setEnabled(checked);
                    updateApplyEnabled();
                });

        _ch_rows.push_back(row);
    }
    main_layout->addWidget(_ch_group);

    _status_label = new QLabel(_content_widget);
    _status_label->setAlignment(Qt::AlignCenter);
    main_layout->addWidget(_status_label);

    auto *btn_layout = new QHBoxLayout();
    _apply_btn = new QPushButton(_content_widget);
    _clear_btn = new QPushButton(_content_widget);
    btn_layout->addWidget(_apply_btn);
    btn_layout->addWidget(_clear_btn);
    main_layout->addLayout(btn_layout);
    main_layout->addStretch(1);

    connect(_apply_btn, &QPushButton::clicked, this, &GlitchFilterDock::onApply);
    connect(_clear_btn, &QPushButton::clicked, this, &GlitchFilterDock::onClear);

    _clear_btn->setEnabled(false);
    retranslateUi();
    updateApplyEnabled();
}

void GlitchFilterDock::retranslateUi()
{
    _help_group->setTitle(tr("How it works"));
    _help_label->setText(tr("Enable a channel and set its threshold (in sample periods). "
                            "Pulses shorter than or equal to the threshold will be flipped "
                            "to the opposite level. Use Clear to revert."));
    _ch_group->setTitle(tr("Channel Settings"));
    _hdr_ch->setText(tr("Ch"));
    _hdr_enable->setText(tr("Enable"));
    _hdr_threshold->setText(tr("Threshold"));
    _hdr_unit->setText(tr("Unit"));

    for (auto &row : _ch_rows)
        row.unit_label->setText(tr("samples"));

    _apply_btn->setText(tr("Apply"));
    _clear_btn->setText(tr("Clear"));

    setStatus(_status);
}

void GlitchFilterDock::updateApplyEnabled()
{
    bool any = false;
    for (const auto &row : _ch_rows) {
        if (row.enable_cb->isChecked()) { any = true; break; }
    }
    bool busy = (_session && _session->is_working());

    // Only enable Apply when a capture/file has actually produced samples.
    // Without this guard, hitting Apply before Start (or before opening a
    // file) yields a confusing "No data" message. We only peek at the
    // snapshot when no concurrent writer is active (busy guard above);
    // get_view_logic_snapshot() asserts !is_working() in debug builds.
    bool has_data = false;
    if (_session && !busy) {
        pv::data::LogicSnapshot *snap = _session->get_view_logic_snapshot();
        has_data = snap && snap->have_data();
    }
    _apply_btn->setEnabled(any && _session != nullptr && !busy
                           && _worker == nullptr && has_data);
}

void GlitchFilterDock::setStatus(StatusState s)
{
    _status = s;
    switch (s) {
        case StatusState::NotApplied:      _status_label->setText(tr("Not applied")); break;
        case StatusState::Applied: {
            // Show how many runs were flipped so the user has immediate
            // verification that the algorithm found and removed pulses.
            size_t n = _session ? _session->glitch_filter_region_count() : 0;
            _status_label->setText(n > 0
                ? tr("Applied (%1 region(s) flipped)").arg(n)
                : tr("Applied"));
            break;
        }
        case StatusState::Applying:        _status_label->setText(tr("Applying...")); break;
        case StatusState::NoData:          _status_label->setText(tr("No data")); break;
        case StatusState::NoChannels:      _status_label->setText(tr("No channels enabled")); break;
        case StatusState::CaptureRunning:  _status_label->setText(tr("Capture is running")); break;
        case StatusState::LoopUnsupported: _status_label->setText(tr("Filter unavailable for loop captures")); break;
        case StatusState::NoGlitches:      _status_label->setText(tr("No glitches found")); break;
        case StatusState::Discarded:       _status_label->setText(tr("Discarded")); break;
    }
}

void GlitchFilterDock::onApply()
{
    if (!_session) return;
    if (_worker) {
        // Worker still running — ignore re-click
        return;
    }
    if (_session->is_working()) {
        setStatus(StatusState::CaptureRunning);
        return;
    }
    pv::data::LogicSnapshot *snap = _session->get_view_logic_snapshot();
    if (!snap || !snap->have_data()) {
        setStatus(StatusState::NoData);
        return;
    }
    if (snap->is_loop()) {
        setStatus(StatusState::LoopUnsupported);
        return;
    }

    // Build config from UI
    _pending_cfg = data::GlitchFilterConfig{};
    for (int i = 0; i < MaxChannels && i < GLITCH_FILTER_MAX_CH; i++) {
        _pending_cfg.enabled[i]   = _ch_rows[i].enable_cb->isChecked();
        _pending_cfg.threshold[i] = static_cast<uint32_t>(_ch_rows[i].threshold_sb->value());
    }
    if (!_pending_cfg.any_enabled()) {
        setStatus(StatusState::NoChannels);
        return;
    }

    // Undo previous filter synchronously so the worker operates on clean data.
    _session->apply_glitch_filter_undo();
    // Note: this undo updates the snapshot but the worker hasn't run yet.
    // Between here and onWorkerFinished()'s finalize_glitch_filter (which
    // calls _callback->data_updated), the view will paint the unfiltered
    // snapshot — this is intentional to avoid an intermediate flicker.

    int total_ch = static_cast<int>(_session->get_ch_num(SR_CHANNEL_LOGIC));

    _apply_btn->setEnabled(false);
    _clear_btn->setEnabled(false);
    setStatus(StatusState::Applying);

    _worker_session = _session;
    _session->set_filter_in_progress(true);

    _worker = new data::GlitchFilterWorker(snap, _pending_cfg, total_ch, this);
    // Auto-resolves to QueuedConnection (sender thread = worker run() thread,
    // receiver thread = UI thread).
    connect(_worker, &data::GlitchFilterWorker::finished_ok,
            this, &GlitchFilterDock::onWorkerFinished);
    _worker->start();
}

void GlitchFilterDock::onWorkerFinished()
{
    if (!_worker) return;
    auto undo_log = _worker->undo_log();   // copy out before deleting

    _worker->wait();
    _worker->deleteLater();
    _worker = nullptr;

    SigSession *target = _worker_session;
    _worker_session = nullptr;
    if (target) target->set_filter_in_progress(false);

    if (target == _session && _session) {
        _session->finalize_glitch_filter(_pending_cfg, std::move(undo_log));
        setStatus(_session->is_glitch_filter_applied()
                  ? StatusState::Applied : StatusState::NoGlitches);
    } else {
        // The dock changed sessions while the worker was running (currently
        // unreachable thanks to setSession's mid-worker refusal, but kept as
        // a safety net). Discard the worker's result.
        setStatus(StatusState::Discarded);
    }
    updateApplyEnabled();
    if (_session && _session->is_glitch_filter_applied())
        _clear_btn->setEnabled(true);
}

void GlitchFilterDock::onClear()
{
    if (!_session) return;
    _session->clear_glitch_filter();
    // reload() pulls the now-cleared cfg from the session into the UI
    // (unchecks every channel, resets thresholds to default, updates status
    // and the Clear button enabled state).
    reload();
}

void GlitchFilterDock::setSession(SigSession *session)
{
    if (session == nullptr) return;        // M3: reject null
    if (session == _session) { reload(); return; }
    if (_worker) {
        // Refuse swap while a worker is running; user must wait or close.
        // The next setSession call after onWorkerFinished will succeed.
        return;
    }
    _session = session;
    reload();
}

void GlitchFilterDock::reload()
{
    if (!_session) return;
    const data::GlitchFilterConfig &cfg = _session->glitch_filter_config();
    for (int i = 0; i < MaxChannels && i < GLITCH_FILTER_MAX_CH; i++) {
        _ch_rows[i].enable_cb->setChecked(cfg.enabled[i]);
        _ch_rows[i].threshold_sb->setValue(cfg.threshold[i] > 0 ? cfg.threshold[i] : 3);
        _ch_rows[i].threshold_sb->setEnabled(cfg.enabled[i]);
    }
    _status = _session->is_glitch_filter_applied() ? StatusState::Applied : StatusState::NotApplied;
    setStatus(_status);
    _clear_btn->setEnabled(_session->is_glitch_filter_applied());
    updateApplyEnabled();

    int total_ch = _session ? static_cast<int>(_session->get_ch_num(SR_CHANNEL_LOGIC)) : 0;
    for (int i = 0; i < MaxChannels; i++) {
        bool visible = (i < total_ch);
        _ch_rows[i].ch_label->setVisible(visible);
        _ch_rows[i].enable_cb->setVisible(visible);
        _ch_rows[i].threshold_sb->setVisible(visible);
        _ch_rows[i].unit_label->setVisible(visible);
        if (!visible) {
            _ch_rows[i].enable_cb->setChecked(false);
        }
    }
}

void GlitchFilterDock::onDataUpdated()
{
    // Called from MainWindow::on_data_updated and on_frame_ended. Cheap
    // refresh that tracks capture data arriving / being cleared.
    //
    // Do NOT call reload() here: reload() resets checkbox/spinbox state from
    // SigSession's saved cfg and would clobber any in-progress user selection
    // (e.g. user has just checked D0/threshold but hasn't pressed Apply yet).
    updateApplyEnabled();
}

void GlitchFilterDock::showEvent(QShowEvent *event)
{
    QScrollArea::showEvent(event);
    // Refresh row visibility from the current channel count. The dock is
    // typically constructed before any data is loaded, so reload() at
    // construction time hides every row. Re-running it on each show picks
    // up the latest channel count without needing an explicit hook from
    // MainWindow when a file/device is loaded.
    if (_session) reload();
}

void GlitchFilterDock::UpdateLanguage() { retranslateUi(); }
void GlitchFilterDock::UpdateTheme()    {}
void GlitchFilterDock::UpdateFont()     {}

} // namespace dock
} // namespace pv
