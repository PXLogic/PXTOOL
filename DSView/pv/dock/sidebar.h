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

#ifndef DSVIEW_PV_DOCK_SIDEBAR_H
#define DSVIEW_PV_DOCK_SIDEBAR_H

#include <QWidget>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include "../ui/uimanager.h"

namespace pv {

class SigSession;

namespace view {
class View;
}

namespace dock {

class TriggerDock;
class DsoTriggerDock;
class ProtocolDock;
class MeasureDock;
class SearchDock;
class DeviceOptionsDock;

class SideBar : public QWidget, public IUiWindow
{
    Q_OBJECT

public:
    enum Tab {
        TabTrigger  = 0,
        TabDecodes  = 1,
        TabMeasures = 2,
        TabSearch   = 3,
        TabOptions  = 4,
        TabCount    = 5
    };

    SideBar(QWidget *parent, view::View &view, SigSession *session);
    ~SideBar();

    TriggerDock       *trigger_widget()        { return _trigger_widget; }
    DsoTriggerDock    *dso_trigger_widget()    { return _dso_trigger_widget; }
    ProtocolDock      *protocol_widget()       { return _protocol_widget; }
    MeasureDock       *measure_widget()        { return _measure_widget; }
    SearchDock        *search_widget()         { return _search_widget; }
    DeviceOptionsDock *device_options_widget() { return _device_options_widget; }

    void showTab(Tab tab, bool visible);
    void setDsoMode(bool isDso);

    // IUiWindow
    void UpdateLanguage() override;
    void UpdateTheme()    override;
    void UpdateFont()     override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void sig_search_visible(bool visible);

private slots:
    void onButtonClicked(int tab);

private:
    QStackedWidget *_stack;
    QWidget        *_icon_strip;
    QToolButton    *_btns[TabCount];
    int             _active_tab;
    bool            _panel_visible;

    QStackedWidget    *_trigger_stack;
    TriggerDock       *_trigger_widget;
    DsoTriggerDock    *_dso_trigger_widget;
    ProtocolDock      *_protocol_widget;
    MeasureDock       *_measure_widget;
    SearchDock        *_search_widget;
    DeviceOptionsDock *_device_options_widget;

    SigSession     *_session;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_DOCK_SIDEBAR_H
