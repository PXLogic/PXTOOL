/*
 * This file is part of the DSView project.
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DSVIEW_PV_DOCK_GLITCHFILTERDOCK_H
#define DSVIEW_PV_DOCK_GLITCHFILTERDOCK_H

#include <QScrollArea>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <vector>

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

    // IUiWindow
    void UpdateLanguage() override;
    void UpdateTheme()    override;
    void UpdateFont()     override;

private slots:
    void onApply();
    void onClear();
    void onWorkerFinished();

private:
    void buildUi();
    void retranslateUi();
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
    QLabel      *_hdr_enable;
    QLabel      *_hdr_threshold;
    QLabel      *_hdr_unit;

    struct ChannelRow {
        QCheckBox *enable_cb;
        QSpinBox  *threshold_sb;
        QLabel    *unit_label;
        QLabel    *ch_label;
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
