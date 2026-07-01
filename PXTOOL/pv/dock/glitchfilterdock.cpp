/*
 * This file is part of the PXTOOL project.
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "glitchfilterdock.h"
#include "../sigsession.h"
#include "../data/logicsnapshot.h"
#include "../config/appconfig.h"
#include "../ui/dscombobox.h"
#include "../ui/fn.h"
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
    _hdr_polarity  = new QLabel(_ch_group);
    _hdr_filter    = new QLabel(_ch_group);
    _hdr_enable    = new QLabel(_ch_group);
    _hdr_invert    = new QLabel(_ch_group);
    _hdr_threshold = new QLabel(_ch_group);
    _hdr_unit      = new QLabel(_ch_group); // kept but not in grid; unit shown via spinbox suffix
    _hdr_mode      = new QLabel(_ch_group);
    _col_splitter = new QFrame(_ch_group);
    _col_splitter->setFrameShape(QFrame::NoFrame);
    _col_splitter->setFixedWidth(1);
    _col_splitter->setStyleSheet("background-color: rgba(255,255,255,0.45);");
    _hdr_ch->setAlignment(Qt::AlignCenter);
    _hdr_polarity->setAlignment(Qt::AlignCenter);
    _hdr_filter->setAlignment(Qt::AlignCenter);
    _hdr_invert->setAlignment(Qt::AlignCenter);
    _hdr_enable->setAlignment(Qt::AlignCenter);
    _hdr_threshold->setAlignment(Qt::AlignCenter);
    _hdr_mode->setAlignment(Qt::AlignCenter);
    _ch_grid->setColumnMinimumWidth(3, 10); // spacer between splitter and Enable group

    // Group row
    _ch_grid->addWidget(_hdr_ch,        0, 0);
    _ch_grid->addWidget(_hdr_polarity,  0, 1);
    _ch_grid->addWidget(_hdr_filter,    0, 4, 1, 3);

    // Column header row
    _ch_grid->addWidget(_hdr_invert,    1, 1);
    _ch_grid->addWidget(_hdr_enable,    1, 4);
    _ch_grid->addWidget(_hdr_threshold, 1, 5);
    _ch_grid->addWidget(_hdr_mode,      1, 6);

    for (int i = 0; i < MaxChannels; i++) {
        ChannelRow row;
        row.ch_label     = new QLabel(QString("D%1").arg(i), _ch_group);
        row.enable_cb    = new QCheckBox(_ch_group);
        row.invert_cb    = new QCheckBox(_ch_group);
        row.threshold_sb = new QSpinBox(_ch_group);
        row.threshold_sb->setRange(1, 9999);
        row.threshold_sb->setValue(3);
        row.threshold_sb->setSuffix(tr(" smp")); // unit embedded in spinbox
        row.threshold_sb->setEnabled(false);
        row.unit_label   = new QLabel(_ch_group); // kept but not in grid
        row.mode_cb      = new DsComboBox(_ch_group);
        // Items are re-set in retranslateUi(); indices match FilterMode enum.
        row.mode_cb->addItem(tr("Both"),      static_cast<int>(data::FilterMode::Both));
        row.mode_cb->addItem(tr("High only"), static_cast<int>(data::FilterMode::HighOnly));
        row.mode_cb->addItem(tr("Low only"),  static_cast<int>(data::FilterMode::LowOnly));
        row.mode_cb->setPopupFitContents(true);
        row.mode_cb->setEnabled(false);

        row.ch_label->setAlignment(Qt::AlignCenter);
        _ch_grid->addWidget(row.ch_label,     i + 2, 0, 1, 1, Qt::AlignCenter);
        _ch_grid->addWidget(row.invert_cb,    i + 2, 1, 1, 1, Qt::AlignCenter);
        _ch_grid->addWidget(row.enable_cb,    i + 2, 4, 1, 1, Qt::AlignCenter);
        _ch_grid->addWidget(row.threshold_sb, i + 2, 5, 1, 1, Qt::AlignCenter);
        _ch_grid->addWidget(row.mode_cb,      i + 2, 6, 1, 1, Qt::AlignCenter);

        connect(row.enable_cb, &QCheckBox::toggled, this,
                [this, i](bool checked) {
                    _ch_rows[i].threshold_sb->setEnabled(checked);
                    _ch_rows[i].mode_cb->setEnabled(checked);
                    updateApplyEnabled();
                });
        connect(row.invert_cb, &QCheckBox::toggled, this,
                [this](bool) { updateApplyEnabled(); });

        _ch_rows.push_back(row);
    }
    updateChannelGridRows(_session
        ? static_cast<int>(_session->get_ch_num(SR_CHANNEL_LOGIC)) : 0);
    main_layout->addWidget(_ch_group);
    _ch_group->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

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
    _help_label->setText(tr(
        "Per channel: check Invert to flip the entire waveform polarity "
        "(e.g. when sampled logic is opposite to the bus). Check Enable to "
        "remove glitches (pulses no longer than Threshold, in sample periods); "
        "use Mode to limit removal to high-only or low-only short pulses. You "
        "may use Invert alone or together with glitch removal. Apply writes "
        "changes to the capture; Clear restores the original data."));
    _ch_group->setTitle(tr("Channel Settings"));
    _hdr_ch->setText(tr("Ch"));
    _hdr_polarity->setText(tr("Polarity"));
    _hdr_filter->setText(tr("Glitch filter"));
    _hdr_invert->setText(tr("Invert"));
    _hdr_enable->setText(tr("Enable"));
    _hdr_threshold->setText(tr("Threshold"));
    _hdr_mode->setText(tr("Mode"));

    for (auto &row : _ch_rows) {
        // Refresh spinbox suffix and preserve the combo selection on language change.
        row.threshold_sb->setSuffix(tr(" smp"));
        int cur = row.mode_cb->currentIndex();
        row.mode_cb->setItemText(0, tr("Both"));
        row.mode_cb->setItemText(1, tr("High only"));
        row.mode_cb->setItemText(2, tr("Low only"));
        row.mode_cb->setCurrentIndex(cur);
        row.mode_cb->refreshPopupLayout();
    }

    _apply_btn->setText(tr("Apply"));
    _clear_btn->setText(tr("Clear"));

    setStatus(_status);
}

void GlitchFilterDock::updateChannelGridRows(int total_ch)
{
    const int row_count = total_ch + 2; // 2 header rows + visible channel rows

    _ch_grid->removeWidget(_col_splitter);
    _ch_grid->addWidget(_col_splitter, 0, 2, row_count, 1);

    for (int r = total_ch + 2; r < MaxChannels + 2; r++)
        _ch_grid->setRowMinimumHeight(r, 0);

    _ch_group->updateGeometry();
}

void GlitchFilterDock::updateApplyEnabled()
{
    bool busy = (_session && _session->is_working());

    // Only enable Apply when a capture/file has actually produced samples.
    bool has_data = false;
    if (_session && !busy) {
        pv::data::LogicSnapshot *snap = _session->get_view_logic_snapshot();
        has_data = snap && snap->have_data();
    }

    if (!_session || busy || _worker != nullptr || !has_data) {
        _apply_btn->setEnabled(false);
        return;
    }

    // Build the config the UI currently represents.
    data::GlitchFilterConfig ui_cfg{};
    for (int i = 0; i < (int)_ch_rows.size() && i < GLITCH_FILTER_MAX_CH; i++) {
        ui_cfg.enabled[i]   = _ch_rows[i].enable_cb->isChecked();
        ui_cfg.invert[i]    = _ch_rows[i].invert_cb->isChecked();
        ui_cfg.threshold[i] = static_cast<uint32_t>(_ch_rows[i].threshold_sb->value());
        ui_cfg.mode[i]      = static_cast<data::FilterMode>(_ch_rows[i].mode_cb->currentIndex());
    }

    // Enable Apply if:
    //   (a) the user has selected at least one channel/invert, OR
    //   (b) a filter is already applied and the current UI config differs from
    //       the applied config (e.g. user unchecked everything to clear it).
    bool any = ui_cfg.any_enabled();
    bool applied_and_changed = _session->is_glitch_filter_applied()
                               && (ui_cfg != _session->glitch_filter_config());
    _apply_btn->setEnabled(any || applied_and_changed);
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
        case StatusState::NoChannels:      _status_label->setText(tr("No channels selected")); break;
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
        _pending_cfg.invert[i]    = _ch_rows[i].invert_cb->isChecked();
        _pending_cfg.threshold[i] = static_cast<uint32_t>(_ch_rows[i].threshold_sb->value());
        _pending_cfg.mode[i]      = static_cast<data::FilterMode>(_ch_rows[i].mode_cb->currentIndex());
    }
    // If the user cleared all channels/inverts but there is an active filter,
    // treat Apply as "undo only" — restore the original data and mark clear.
    if (!_pending_cfg.any_enabled()) {
        if (_session->is_glitch_filter_applied()) {
            _session->apply_glitch_filter_undo();
            _session->clear_glitch_filter();
            setStatus(StatusState::NotApplied);
            updateApplyEnabled();
            _clear_btn->setEnabled(false);
        } else {
            setStatus(StatusState::NoChannels);
        }
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
        _ch_rows[i].invert_cb->setChecked(cfg.invert[i]);
        _ch_rows[i].threshold_sb->setValue(cfg.threshold[i] > 0 ? cfg.threshold[i] : 3);
        _ch_rows[i].threshold_sb->setEnabled(cfg.enabled[i]);
        _ch_rows[i].mode_cb->setCurrentIndex(static_cast<int>(cfg.mode[i]));
        _ch_rows[i].mode_cb->setEnabled(cfg.enabled[i]);
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
        _ch_rows[i].invert_cb->setVisible(visible);
        _ch_rows[i].threshold_sb->setVisible(visible);
        _ch_rows[i].mode_cb->setVisible(visible);
        if (!visible) {
            _ch_rows[i].enable_cb->setChecked(false);
            _ch_rows[i].invert_cb->setChecked(false);
        }
    }
    updateChannelGridRows(total_ch);
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

void GlitchFilterDock::UpdateFont()
{
    QFont font = this->font();
    font.setPixelSize(qRound(AppConfig::Instance().appOptions.fontSize));
    ui::set_form_font(this, font);

    font.setPixelSize(font.pixelSize() + 1);
    if (parentWidget())
        parentWidget()->setFont(font);
}

} // namespace dock
} // namespace pv
