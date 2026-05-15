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
#include "triggerdock.h"
#include "dsotriggerdock.h"
#include "protocoldock.h"
#include "measuredock.h"
#include "searchdock.h"
#include "deviceoptionsdock.h"
#include "logdock.h"
#include "../view/view.h"
#include <QLabel>
#include <QIcon>
#include <QSize>
#include <QSizePolicy>
#include <QEvent>

namespace pv {
namespace dock {

SideBar::SideBar(QWidget *parent, view::View &view, SigSession *session)
    : QWidget(parent)
    , _session(session)
    , _active_tab(-1)
    , _panel_visible(false)
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
    auto *tw_title = new QLabel(tr("Trigger Setting"), trigger_wrap);
    tw_title->setObjectName("trigger_panel_title");
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
    auto *dw_title = new QLabel(tr("Decode Protocol"), dw_header);
    dw_title->setObjectName("decode_panel_title");
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
    auto *mw_title = new QLabel(tr("Measurement"), mw_header);
    mw_title->setObjectName("measure_panel_title");
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
    auto *sw_title = new QLabel(tr("Search"), sw_header);
    sw_title->setObjectName("search_panel_title");
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
    auto *ow_title = new QLabel(tr("Device Options"), ow_header);
    ow_title->setObjectName("options_panel_title");
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

    // Outer content stack: one page per tab
    _stack = new QStackedWidget(this);
    _stack->addWidget(trigger_wrap);      // page 0: trigger (header + inner stack)
    _stack->addWidget(decode_wrap);       // page 1: decodes (header + protocol dock)
    _stack->addWidget(measure_wrap);      // page 2: measures (header + measure dock)
    _stack->addWidget(search_wrap);       // page 3: search (header + search dock)
    _stack->addWidget(options_wrap);      // page 4: device options panel
    _stack->addWidget(log_wrap);          // page 5: log panel
    _stack->setVisible(false);
    _stack->setMinimumWidth(200);

    // Icon strip (52px, far right)
    _icon_strip = new QWidget(this);
    _icon_strip->setObjectName("icon_strip");
    _icon_strip->setFixedWidth(52);

    auto *stripLayout = new QVBoxLayout(_icon_strip);
    stripLayout->setContentsMargins(0, 8, 0, 8);
    stripLayout->setSpacing(0);

    static const char *iconPaths[TabCount] = {
        ":/icons/sidebar/zap.svg",
        ":/icons/sidebar/layers.svg",
        ":/icons/sidebar/ruler.svg",
        ":/icons/sidebar/search.svg",
        ":/icons/sidebar/settings-2.svg",
        ":/icons/sidebar/log.svg"
    };
    static const char *labels[TabCount] = {
        "TRIGGER", "DECODE", "MEASURE", "SEARCH", "OPTIONS", "LOG"
    };

    for (int i = 0; i < TabCount; i++) {
        _btns[i] = new QToolButton(this);
        _btns[i]->setObjectName("sidebar_tab_btn");
        _btns[i]->setIcon(QIcon(iconPaths[i]));
        _btns[i]->setText(labels[i]);
        _btns[i]->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        _btns[i]->setIconSize(QSize(18, 18));
        _btns[i]->setFixedSize(52, 54);
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

    ADD_UI(this);
}

SideBar::~SideBar()
{
    REMOVE_UI(this);
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
    } else {
        // Switch to (or re-open) this tab
        bool prevWasSearch = (_active_tab == TabSearch && _panel_visible);
        if (_active_tab >= 0 && _active_tab < TabCount)
            _btns[_active_tab]->setChecked(false);

        _active_tab    = tab;
        _panel_visible = true;
        _stack->setCurrentIndex(tab);
        _stack->setVisible(true);
        _btns[tab]->setChecked(true);

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
}

void SideBar::refresh_device_options()
{
    _device_options_widget->rebuild();
}

void SideBar::UpdateLanguage() {}
void SideBar::UpdateTheme()    {}
void SideBar::UpdateFont()     {}

} // namespace dock
} // namespace pv
