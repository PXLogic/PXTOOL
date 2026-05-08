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
#include "../view/view.h"
#include <QLabel>
#include <QIcon>
#include <QSize>
#include <QSizePolicy>

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

    _protocol_widget = new ProtocolDock(this, view, session);
    _measure_widget  = new MeasureDock(this, view, session);
    _search_widget   = new SearchDock(this, view, session);

    _options_widget = new QWidget(this);
    _options_widget->setObjectName("options_panel");
    auto *optLayout = new QVBoxLayout(_options_widget);
    optLayout->setContentsMargins(16, 16, 16, 16);
    optLayout->setSpacing(8);
    auto *optTitle = new QLabel(tr("OPTIONS"), _options_widget);
    optTitle->setObjectName("options_title_label");
    optLayout->addWidget(optTitle);
    auto *optHint = new QLabel(tr("Configure sampling and device\nsettings in the toolbar above."), _options_widget);
    optHint->setObjectName("options_hint_label");
    optHint->setWordWrap(true);
    optLayout->addWidget(optHint);
    optLayout->addStretch();

    // Outer content stack: one page per tab
    _stack = new QStackedWidget(this);
    _stack->addWidget(_trigger_stack);   // page 0: trigger
    _stack->addWidget(_protocol_widget); // page 1: decodes
    _stack->addWidget(_measure_widget);  // page 2: measures
    _stack->addWidget(_search_widget);   // page 3: search
    _stack->addWidget(_options_widget);  // page 4: options
    _stack->setVisible(false);
    _stack->setMinimumWidth(280);

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
        ":/icons/sidebar/settings-2.svg"
    };
    static const char *labels[TabCount] = {
        "TRIGGER", "DECODE", "MEASURE", "SEARCH", "OPTIONS"
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

    // Connect buttons using index capture
    for (int i = 0; i < TabCount; i++) {
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

void SideBar::UpdateLanguage() {}
void SideBar::UpdateTheme()    {}
void SideBar::UpdateFont()     {}

} // namespace dock
} // namespace pv
