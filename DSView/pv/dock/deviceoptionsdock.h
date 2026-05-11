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

#ifndef DSVIEW_PV_DOCK_DEVICEOPTIONSDOCK_H
#define DSVIEW_PV_DOCK_DEVICEOPTIONSDOCK_H

#include <QScrollArea>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QTimer>
#include <vector>

#include "../dialogs/deviceoptions.h"   // reuse IChannelCheck, ChannelLabel, ChannelModePair
#include "../prop/binding/deviceoptions.h"
#include "../prop/binding/probeoptions.h"
#include "../ui/uimanager.h"

class DeviceAgent;

namespace pv {

class SigSession;

namespace dock {

class DeviceOptionsDock : public QScrollArea, public IChannelCheck, public IUiWindow
{
    Q_OBJECT

public:
    DeviceOptionsDock(QWidget *parent, SigSession *session);
    ~DeviceOptionsDock();

    /** Call when the panel tab is made visible so it can refresh. */
    void panel_shown();

private:
    void build_content();
    void build_dynamic_panel();
    void logic_probes(QVBoxLayout &layout);
    void analog_probes(QGridLayout &layout);
    QString dynamic_widget(QLayout *lay);
    QLayout *get_property_form(QWidget *parent);

    void set_all_probes(bool set);
    void enable_max_probes();
    void channel_checkbox_clicked(QCheckBox *sc);

    // IChannelCheck
    void ChannelChecked(int index, QObject *object) override;

    // IUiWindow
    void UpdateLanguage() override {}
    void UpdateTheme()    override {}
    void UpdateFont()     override;

private slots:
    void on_apply();
    void mode_check_timeout();
    void channel_check();
    void analog_channel_check();
    void on_anlog_tab_changed(int index);
    void enable_all_probes();
    void disable_all_probes();
    void on_analog_channel_enable();
    void zero_adj();
    void on_calibration();

private:
    SigSession    *_session;
    DeviceAgent   *_device_agent;

    QWidget       *_inner;          // inner scrollable widget
    QVBoxLayout   *_inner_lay;
    QVBoxLayout   *_container_lay;
    QWidget       *_container_panel;
    QWidget       *_dynamic_panel;

    QTimer         _mode_check_timer;
    int            _opt_mode;
    int            _groupHeight1;
    int            _groupHeight2;
    volatile bool  _isBuilding;
    int            _cur_analog_tag_index;
    QString        _demo_operation_mode;
    bool           _content_built;

    std::vector<QCheckBox *>                        _probes_checkBox_list;
    pv::prop::binding::DeviceOptions               *_device_options_binding;
    std::vector<pv::prop::binding::ProbeOptions *>  _probe_options_binding_list;
    std::vector<ChannelModePair>                    _channel_mode_indexs;
    std::vector<struct sr_channel *>                _dso_channel_list;
    std::vector<bool>                               _lst_probe_enabled_status;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_DOCK_DEVICEOPTIONSDOCK_H
