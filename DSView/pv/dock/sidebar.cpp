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

#include "sidebar.h"
#include <QTimer>
#include <QPointer>
#include "../config/appconfig.h"
#include "triggerdock.h"
#include "dsotriggerdock.h"
#include "protocoldock.h"
#include "measuredock.h"
#include "searchdock.h"
#include "deviceoptionsdock.h"
#include "logdock.h"
#include "glitchfilterdock.h"
#include "../view/view.h"
#include <QLabel>
#include <QIcon>
#include <QSize>
#include <QSizePolicy>
#include <QEvent>
#include <QMainWindow>
#include <QDockWidget>
#include <QtGlobal>

namespace pv {
namespace dock {

SideBar::SideBar(QWidget *parent, view::View &view, SigSession *session)
    : QWidget(parent)
    , _session(session)
    , _active_tab(-1)
    , _panel_visible(false)
    , _saved_panel_width(kDefaultDockWidth)
{
    setObjectName("sidebar");

    // Trigger inner stack: index 0 = logic TriggerDock, index 1 = DSO DsoTriggerDock
    _trigger_stack = new QStackedWidget(this);
    _trigger_widget     = new TriggerDock(_trigger_stack, session);
    _dso_trigger_widget = new DsoTriggerDock(_trigger_stack, session);
    _trigger_stack->addWidget(_trigger_widget);
    _trigger_stack->addWidget(_dso_trigger_widget);

    // Trigger tab wrapper: fixed "Trigger" header above the trigger stack
    auto *trigger_wrap = new QWidget(this);
    trigger_wrap->setObjectName("trigger_wrap");
    auto *tw_layout = new QVBoxLayout(trigger_wrap);
    tw_layout->setContentsMargins(0, 0, 0, 0);
    tw_layout->setSpacing(0);
    auto *tw_header = new QWidget(trigger_wrap);
    tw_header->setObjectName("trigger_header_bar");
    auto *tw_hlay = new QHBoxLayout(tw_header);
    tw_hlay->setContentsMargins(12, 8, 12, 8);
    _title_trigger = new QLabel(tr("Trigger Setting"), trigger_wrap);
    _title_trigger->setObjectName("trigger_panel_title");
    auto *tw_title = _title_trigger;
    tw_hlay->addWidget(tw_title);
    tw_hlay->addStretch();
    tw_layout->addWidget(tw_header);
    tw_layout->addWidget(_trigger_stack, 1);

    _protocol_widget = new ProtocolDock(this, view, session);
    _measure_widget  = new MeasureDock(this, view, session);
    _search_widget   = new SearchDock(this, view, session);

    // Decode tab: fixed "Decode Protocol" header above the scrollable protocol dock
    auto *decode_wrap = new QWidget(this);
    decode_wrap->setObjectName("decode_wrap");
    auto *dw_layout = new QVBoxLayout(decode_wrap);
    dw_layout->setContentsMargins(0, 0, 0, 0);
    dw_layout->setSpacing(0);
    auto *dw_header = new QWidget(decode_wrap);
    dw_header->setObjectName("decode_header_bar");
    auto *dw_hlay = new QHBoxLayout(dw_header);
    dw_hlay->setContentsMargins(12, 8, 12, 8);
    _title_decode = new QLabel(tr("Decode Protocol"), dw_header);
    _title_decode->setObjectName("decode_panel_title");
    auto *dw_title = _title_decode;
    dw_hlay->addWidget(dw_title);
    dw_hlay->addStretch();
    dw_layout->addWidget(dw_header);
    dw_layout->addWidget(_protocol_widget, 1);

    // Measure tab wrapper: fixed "Measurement" header above the scrollable MeasureDock
    auto *measure_wrap = new QWidget(this);
    measure_wrap->setObjectName("measure_wrap");
    auto *mw_layout = new QVBoxLayout(measure_wrap);
    mw_layout->setContentsMargins(0, 0, 0, 0);
    mw_layout->setSpacing(0);
    auto *mw_header = new QWidget(measure_wrap);
    mw_header->setObjectName("measure_header_bar");
    auto *mw_hlay = new QHBoxLayout(mw_header);
    mw_hlay->setContentsMargins(12, 8, 12, 8);
    _title_measure = new QLabel(tr("Measurement"), mw_header);
    _title_measure->setObjectName("measure_panel_title");
    auto *mw_title = _title_measure;
    mw_hlay->addWidget(mw_title);
    mw_hlay->addStretch();
    mw_layout->addWidget(mw_header);
    mw_layout->addWidget(_measure_widget, 1);

    // Search tab wrapper: fixed "Search" header above the SearchDock
    auto *search_wrap = new QWidget(this);
    search_wrap->setObjectName("search_wrap");
    auto *sw_layout = new QVBoxLayout(search_wrap);
    sw_layout->setContentsMargins(0, 0, 0, 0);
    sw_layout->setSpacing(0);
    auto *sw_header = new QWidget(search_wrap);
    sw_header->setObjectName("search_header_bar");
    auto *sw_hlay = new QHBoxLayout(sw_header);
    sw_hlay->setContentsMargins(12, 8, 12, 8);
    _title_search = new QLabel(tr("Search"), sw_header);
    _title_search->setObjectName("search_panel_title");
    auto *sw_title = _title_search;
    sw_hlay->addWidget(sw_title);
    sw_hlay->addStretch();
    sw_layout->addWidget(sw_header);
    sw_layout->addWidget(_search_widget, 1);

    // Options tab wrapper: fixed "Device Options" header above the DeviceOptionsDock
    auto *options_wrap = new QWidget(this);
    options_wrap->setObjectName("options_wrap");
    auto *ow_layout = new QVBoxLayout(options_wrap);
    ow_layout->setContentsMargins(0, 0, 0, 0);
    ow_layout->setSpacing(0);
    auto *ow_header = new QWidget(options_wrap);
    ow_header->setObjectName("options_header_bar");
    auto *ow_hlay = new QHBoxLayout(ow_header);
    ow_hlay->setContentsMargins(12, 8, 12, 8);
    _title_options = new QLabel(tr("Device Options"), ow_header);
    _title_options->setObjectName("options_panel_title");
    auto *ow_title = _title_options;
    ow_hlay->addWidget(ow_title);
    ow_hlay->addStretch();
    _device_options_widget = new DeviceOptionsDock(options_wrap, session);
    ow_layout->addWidget(ow_header);
    ow_layout->addWidget(_device_options_widget, 1);

    // Log tab: LogDock fills the entire panel (has its own internal toolbar)
    _log_widget = new LogDock(this);
    auto *log_wrap = new QWidget(this);
    log_wrap->setObjectName("log_wrap");
    auto *lw_layout = new QVBoxLayout(log_wrap);
    lw_layout->setContentsMargins(0, 0, 0, 0);
    lw_layout->setSpacing(0);
    lw_layout->addWidget(_log_widget, 1);

    // Filter tab: "Glitch Filter" header + GlitchFilterDock
    _glitch_filter_widget = new GlitchFilterDock(this, session);
    auto *filter_wrap = new QWidget(this);
    filter_wrap->setObjectName("filter_wrap");
    auto *fw_layout = new QVBoxLayout(filter_wrap);
    fw_layout->setContentsMargins(0, 0, 0, 0);
    fw_layout->setSpacing(0);
    auto *fw_header = new QWidget(filter_wrap);
    fw_header->setObjectName("filter_header_bar");
    auto *fw_hlay = new QHBoxLayout(fw_header);
    fw_hlay->setContentsMargins(12, 8, 12, 8);
    _title_filter = new QLabel(tr("Glitch Filter"), fw_header);
    _title_filter->setObjectName("filter_panel_title");
    fw_hlay->addWidget(_title_filter);
    fw_hlay->addStretch();
    fw_layout->addWidget(fw_header);
    fw_layout->addWidget(_glitch_filter_widget, 1);

    // Outer content stack: one page per tab
    _stack = new QStackedWidget(this);
    _stack->addWidget(trigger_wrap);      // page 0: trigger (header + inner stack)
    _stack->addWidget(decode_wrap);       // page 1: decodes (header + protocol dock)
    _stack->addWidget(measure_wrap);      // page 2: measures (header + measure dock)
    _stack->addWidget(search_wrap);       // page 3: search (header + search dock)
    _stack->addWidget(options_wrap);      // page 4: device options panel
    _stack->addWidget(log_wrap);          // page 5: log panel
    _stack->addWidget(filter_wrap);       // page 6: glitch filter
    _stack->setVisible(false);
    _stack->setMinimumWidth(kStackMinWidth);

    // Icon strip (far right)
    _icon_strip = new QWidget(this);
    _icon_strip->setObjectName("icon_strip");
    _icon_strip->setFixedWidth(kIconStripWidth);

    auto *stripLayout = new QVBoxLayout(_icon_strip);
    stripLayout->setContentsMargins(0, 8, 0, 8);
    stripLayout->setSpacing(0);

    static const char *iconPaths[TabCount] = {
        ":/icons/sidebar/zap.svg",
        ":/icons/sidebar/layers.svg",
        ":/icons/sidebar/ruler.svg",
        ":/icons/sidebar/search.svg",
        ":/icons/sidebar/settings-2.svg",
        ":/icons/sidebar/log.svg",
        ":/icons/sidebar/filter.svg"
    };
    static const char *labels[TabCount] = {
        QT_TR_NOOP("Trigger"), QT_TR_NOOP("Decode"),
        QT_TR_NOOP("Measure"), QT_TR_NOOP("Search"),
        QT_TR_NOOP("Options"), QT_TR_NOOP("Log"),
        QT_TR_NOOP("Filter")
    };

    for (int i = 0; i < TabCount; i++) {
        _btns[i] = new QToolButton(this);
        _btns[i]->setObjectName("sidebar_tab_btn");
        _btns[i]->setIcon(QIcon(iconPaths[i]));
        _btns[i]->setText(tr(labels[i]));
        _btns[i]->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        _btns[i]->setIconSize(QSize(16, 16));
        _btns[i]->setFixedSize(kIconStripWidth, 46);
        _btns[i]->setCheckable(true);

        stripLayout->addWidget(_btns[i]);
    }
    stripLayout->addStretch(1);

    // Connect buttons using index capture; install event filter to swallow
    // double-click events so a double-click is treated as a single click.
    for (int i = 0; i < TabCount; i++) {
        _btns[i]->installEventFilter(this);
        connect(_btns[i], &QToolButton::clicked, [this, i]() {
            onButtonClicked(i);
        });
    }

    // Main layout: [content panel | icon strip]
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(_stack, 1);
    mainLayout->addWidget(_icon_strip, 0);

    // When Apply commits channel changes, only drop the decoders that bind
    // to a channel that just got disabled. Enabling channels (e.g. Enable All)
    // emits an empty set, so existing decoders are preserved.
    connect(_device_options_widget, &DeviceOptionsDock::sig_channels_applied,
            _protocol_widget, &ProtocolDock::del_protocols_using_channels);

    ADD_UI(this);
}

SideBar::~SideBar()
{
    REMOVE_UI(this);
}

QDockWidget *SideBar::findParentDock() const
{
    QWidget *p = parentWidget();
    while (p && !qobject_cast<QDockWidget*>(p))
        p = p->parentWidget();
    return qobject_cast<QDockWidget*>(p);
}

void SideBar::adjustDockWidth(bool expand)
{
    QDockWidget *dock = findParentDock();
    if (!dock) return;
    QMainWindow *mw = qobject_cast<QMainWindow*>(dock->parentWidget());
    if (!mw) return;

    if (expand) {
        // Panel open.  The dock may be coming out of the locked-collapsed
        // state (min == max == kIconStripWidth), in which case neither
        // releasing the max followed by an inline resizeDocks() nor doing
        // it in a deferred lambda reliably gets the dock back to the
        // configured panel width — QMainWindowLayout caches the old
        // constraint and clamps the request.  Robust recipe: release max
        // first, then *temporarily* pin min to the target width so the
        // layout has to honour the size; resizeDocks() then snaps the dock
        // there.  Once the layout has settled (next event loop iteration),
        // relax min back to the normal floor so the user can drag the
        // QMainWindow separator narrower.
        const int target = _saved_panel_width;

        _stack->setMinimumWidth(kStackMinWidth);
        _stack->updateGeometry();

        dock->setMaximumWidth(QWIDGETSIZE_MAX);
        dock->setMinimumWidth(target);
        mw->resizeDocks({dock}, {target}, Qt::Horizontal);

        QPointer<QDockWidget> dockPtr(dock);
        QTimer::singleShot(0, this, [dockPtr]() {
            if (dockPtr)
                dockPtr->setMinimumWidth(kStackMinWidth + kIconStripWidth);
        });
    } else {
        // Panel collapsed: lock the dock to the icon-strip width so the
        // user cannot drag the QMainWindow separator and leave a blank
        // (invisible-_stack) gap between the waveform area and the icon
        // strip.  Re-opening a tab restores the expanded constraints above.
        _saved_panel_width = qMax(kStackMinWidth + kIconStripWidth, dock->width());
        _stack->setMinimumWidth(0);
        _stack->updateGeometry();
        // Release the min first so we can shrink the dock down to the
        // icon strip width before clamping max.
        dock->setMinimumWidth(0);
        dock->setMaximumWidth(kIconStripWidth);
        dock->setMinimumWidth(kIconStripWidth);
        mw->resizeDocks({dock}, {kIconStripWidth}, Qt::Horizontal);
    }
}

void SideBar::closePanel()
{
    // Reset sidebar to clean state for a new session tab:
    // close the panel, resize dock to icon strip only, and reset the saved
    // panel width so the next open uses the default rather than a stale drag width.
    if (_panel_visible) {
        _panel_visible = false;
        _stack->setVisible(false);
        if (_active_tab >= 0 && _active_tab < TabCount)
            _btns[_active_tab]->setChecked(false);
        _active_tab = -1;
    }
    // Defer resize so Qt finishes the setVisible(false) layout pass before
    // resizeDocks() collapses the dock — otherwise the dock can end up offset
    // when the panel was at a user-dragged width.
    QTimer::singleShot(0, this, [this]() {
        adjustDockWidth(false);
        _saved_panel_width = kDefaultDockWidth;
    });
}

void SideBar::onButtonClicked(int tab)
{
    if (_active_tab == tab && _panel_visible) {
        // Toggle off: hide the panel
        _panel_visible = false;
        _stack->setVisible(false);
        _btns[tab]->setChecked(false);
        _active_tab = -1;
        if (tab == TabSearch)
            emit sig_search_visible(false);
        if (tab == TabOptions)
            _device_options_widget->panel_shown(); // stop timer when hidden
        adjustDockWidth(false);
    } else {
        // Switch to (or re-open) this tab
        bool wasHidden = !_panel_visible;
        bool prevWasSearch = (_active_tab == TabSearch && _panel_visible);
        if (_active_tab >= 0 && _active_tab < TabCount)
            _btns[_active_tab]->setChecked(false);

        _active_tab    = tab;
        _panel_visible = true;
        _stack->setCurrentIndex(tab);
        _stack->setVisible(true);
        _btns[tab]->setChecked(true);

        if (wasHidden) {
            // Always use the configured default width when reopening from the
            // collapsed icon-strip state.
            _saved_panel_width = kDefaultDockWidth;
            adjustDockWidth(true);
        }

        if (tab == TabSearch)
            emit sig_search_visible(true);
        else if (prevWasSearch)
            emit sig_search_visible(false);

        if (tab == TabOptions)
            _device_options_widget->panel_shown();
    }
}

void SideBar::showTab(Tab tab, bool visible)
{
    if (visible) {
        if (_active_tab != (int)tab || !_panel_visible)
            onButtonClicked((int)tab);
    } else {
        if (_active_tab == (int)tab && _panel_visible)
            onButtonClicked((int)tab);
    }
}

void SideBar::setDsoMode(bool isDso)
{
    _trigger_stack->setCurrentIndex(isDso ? 1 : 0);
    if (isDso)
        _dso_trigger_widget->update_view();
    else
        _trigger_widget->update_view();
}

void SideBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // While the panel is open, track the user's live drag width so that
    // a subsequent collapse/reopen restores the actual dragged size rather
    // than the stale default.
    if (_panel_visible) {
        int w = event->size().width();
        if (w > kIconStripWidth + 10)
            _saved_panel_width = w;
    }
}

bool SideBar::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        for (int i = 0; i < TabCount; i++) {
            if (obj == _btns[i])
                return true; // swallow double-click so it doesn't fire a second toggle
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SideBar::setSession(SigSession *session)
{
    _session = session;
    _device_options_widget->setSession(session);
    _protocol_widget->setSession(session);
    if (_glitch_filter_widget)
        _glitch_filter_widget->setSession(session);
    _search_widget->setSession(session);
}

void SideBar::setView(view::View *view)
{
    _search_widget->setView(view);
}

void SideBar::refresh_device_options()
{
    _device_options_widget->on_device_changed();
}

void SideBar::retranslateUi()
{
    static const char *labels[TabCount] = {
        QT_TR_NOOP("Trigger"), QT_TR_NOOP("Decode"),
        QT_TR_NOOP("Measure"), QT_TR_NOOP("Search"),
        QT_TR_NOOP("Options"), QT_TR_NOOP("Log"),
        QT_TR_NOOP("Filter")
    };
    for (int i = 0; i < TabCount; i++)
        _btns[i]->setText(tr(labels[i]));

    if (_title_trigger) _title_trigger->setText(tr("Trigger Setting"));
    if (_title_decode)  _title_decode->setText(tr("Decode Protocol"));
    if (_title_measure) _title_measure->setText(tr("Measurement"));
    if (_title_search)  _title_search->setText(tr("Search"));
    if (_title_options) _title_options->setText(tr("Device Options"));
    if (_title_filter)  _title_filter->setText(tr("Glitch Filter"));
}

void SideBar::UpdateLanguage() { retranslateUi(); }

void SideBar::UpdateTheme()
{
    // Re-apply title styles because color depends on the current theme.
    applyTitleStyle();
}

void SideBar::UpdateFont()
{
    applyTitleStyle();

    const int basePx = qRound(AppConfig::Instance().appOptions.fontSize);

    QFont tabFont = font();
    tabFont.setPixelSize(basePx);
    tabFont.setBold(false);
    for (int i = 0; i < TabCount; i++) {
        if (_btns[i])
            _btns[i]->setFont(tabFont);
    }

    QFont panelTitleFont = font();
    panelTitleFont.setPixelSize(basePx + 1);
    panelTitleFont.setWeight(QFont::DemiBold);
    QLabel *titles[] = {
        _title_trigger, _title_decode, _title_measure,
        _title_search, _title_options, _title_filter
    };
    for (QLabel *lbl : titles) {
        if (lbl)
            lbl->setFont(panelTitleFont);
    }
}

void SideBar::applyTitleStyle()
{
    // Only set theme-dependent color inline; size/weight come from UpdateFont().
    // Do not set font-size here — it overrides QSS and blocks Display font scaling.
    bool dark = AppConfig::Instance().IsDarkStyle();
    QString color = dark ? QStringLiteral("#e0e0e0") : QStringLiteral("#222222");
    QString style = QStringLiteral("color: %1; background: transparent;").arg(color);

    if (_title_trigger) _title_trigger->setStyleSheet(style);
    if (_title_decode)  _title_decode->setStyleSheet(style);
    if (_title_measure) _title_measure->setStyleSheet(style);
    if (_title_search)  _title_search->setStyleSheet(style);
    if (_title_options) _title_options->setStyleSheet(style);
    if (_title_filter)  _title_filter->setStyleSheet(style);
}

} // namespace dock
} // namespace pv
