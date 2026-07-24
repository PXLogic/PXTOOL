/*
 * This file is part of the PXTOOL project.
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DSVIEW_PV_DOCK_GLITCHFILTERDOCK_H
#define DSVIEW_PV_DOCK_GLITCHFILTERDOCK_H

#include <QScrollArea>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <vector>

#include "../ui/dscombobox.h"
#include "../ui/uimanager.h"
#include "../data/glitchfilter.h"

namespace pv {

class SigSession;

namespace dock {

class GlitchFilterDock : public QScrollArea, public IUiWindow
{
    Q_OBJECT

public:
    static const int MaxChannels = 64;

    GlitchFilterDock(QWidget *parent, SigSession *session);
    ~GlitchFilterDock();

    void setSession(SigSession *session);
    void reload();

    // Lightweight refresh hook for SigSession data-updated events. Intentionally
    // does NOT touch checkbox/spinbox state (would clobber the user's pending
    // selection mid-interaction); only refreshes the Apply button's enabled
    // state so it tracks data presence (e.g. enables itself once a capture
    // produces samples after the user has already chosen channels/thresholds).
    void onDataUpdated();

    // IUiWindow
    void UpdateLanguage() override;
    void UpdateTheme()    override;
    void UpdateFont()     override;

private slots:
    void onApply();
    void onClear();
    void onWorkerFinished();

protected:
    // Refresh channel rows whenever the panel becomes visible. The dock is
    // typically constructed before any data is loaded, so the channel count
    // reported by SigSession::get_ch_num() may be 0 at construction time.
    // showEvent() ensures the rows are populated once the user switches to
    // the Filter tab after a capture/file is loaded.
    void showEvent(QShowEvent *event) override;

private:
    void buildUi();
    void retranslateUi();
    void updateChannelGridRows(int total_ch);
    void updateApplyEnabled();

    SigSession *_session;

    QWidget     *_content_widget;
    QGroupBox   *_help_group;
    QLabel      *_help_label;
    QGroupBox   *_ch_group;
    QGridLayout *_ch_grid;
    QPushButton *_apply_btn;
    QPushButton *_clear_btn;
    QLabel      *_status_label;

    QLabel      *_hdr_ch;
    QLabel      *_hdr_polarity;
    QLabel      *_hdr_filter;
    QLabel      *_hdr_enable;
    QLabel      *_hdr_invert;
    QLabel      *_hdr_threshold;
    QLabel      *_hdr_unit;
    QLabel      *_hdr_mode;
    QFrame      *_col_splitter;

    struct ChannelRow {
        QCheckBox *enable_cb;
        QCheckBox *invert_cb;
        QSpinBox  *threshold_sb;
        QLabel    *unit_label;
        QLabel    *ch_label;
        // Polarity selector: index maps to FilterMode enum value
        // (0=Both, 1=HighOnly, 2=LowOnly). Disabled when enable_cb is off.
        DsComboBox *mode_cb;
    };
    std::vector<ChannelRow> _ch_rows;

    enum class StatusState {
        NotApplied,
        Applied,
        Applying,
        NoData,
        NoChannels,
        CaptureRunning,
        LoopUnsupported,
        NoGlitches,
        Discarded,
    };

    void setStatus(StatusState s);

    data::GlitchFilterWorker     *_worker;
    SigSession                   *_worker_session = nullptr; // nullptr unless _worker is running
    // Set in onApply() right before launching the worker; consumed by
    // onWorkerFinished() to forward to SigSession::finalize_glitch_filter.
    // Read/written only on the UI thread.
    data::GlitchFilterConfig      _pending_cfg;
    StatusState                   _status = StatusState::NotApplied;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_DOCK_GLITCHFILTERDOCK_H
