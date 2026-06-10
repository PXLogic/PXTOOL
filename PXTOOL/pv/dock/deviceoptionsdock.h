/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
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
#include <QLineEdit>
#include <QSpinBox>
#include <QRadioButton>
#include <QSet>
#include <QTimer>
#include <QPointer>
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

    /** Update the session and device-agent pointers when the active session changes. */
    void setSession(SigSession *session);

    /** Rebuild panel if the device is now ready but content hasn't been built yet. */
    void rebuild();

    /** Defer refresh of SamplingBar buffer list (e.g. after STREAM_BUFF set_config). */
    static void schedule_sample_count_refresh();

    /** Call when the active device changes within the current session. */
    void on_device_changed();

    /** Refresh inline disk cache controls after settings changed elsewhere. */
    void refresh_disk_cache_settings();

signals:
    /** Emitted after Apply commits channel changes. The set of channel
     * indices that went from enabled -> disabled is passed so listeners
     * (e.g. ProtocolDock) can selectively remove decoders that depend on a
     * now-disabled probe instead of nuking every decoder unconditionally.
     * An empty set means no channels were disabled (e.g. user clicked
     * "Enable All"); listeners should not remove anything in that case. */
    void sig_channels_applied(const QSet<int> &disabled_channel_indices);
    void sig_channel_mode_changed();
    /** Stream buff / other device-option changes that alter SR_CONF_HW_DEPTH. */
    void sig_sample_count_refresh_needed();
    /** Disk cache settings changed from the inline editor or menu dialog. */
    void sig_disk_cache_settings_changed();

private:
    void build_content();
    void build_dynamic_panel();
    void logic_probes(QVBoxLayout &layout);
    void analog_probes(QGridLayout &layout);
    QString dynamic_widget(QLayout *lay);
    QLayout *get_property_form(QWidget *parent);
    QWidget *create_disk_cache_inline_editor(QWidget *parent);
    bool commit_disk_cache_inline();
    void refresh_disk_cache_inline();
    void update_stream_buffer_enabled_state();

    void set_all_probes(bool set);
    void enable_max_probes();
    void channel_checkbox_clicked(QCheckBox *sc);

    // IChannelCheck
    void ChannelChecked(int index, QObject *object) override;

    // IUiWindow
    void UpdateLanguage() override;
    void UpdateTheme()    override {}
    void UpdateFont()     override;

private slots:
    void on_apply();
    void schedule_apply();
    void mode_check_timeout();
    void channel_check();
    void analog_channel_check();
    void on_anlog_tab_changed(int index);
    void enable_all_probes();
    void disable_all_probes();
    void on_analog_channel_enable();
    void zero_adj();
    void on_calibration();
    void on_disk_cache_enabled_changed(int state);
    void on_disk_cache_text_finished();
    void on_disk_cache_browse();

private:
    SigSession    *_session;
    DeviceAgent   *_device_agent;

    QWidget       *_inner;          // inner scrollable widget
    QVBoxLayout   *_inner_lay;
    QVBoxLayout   *_container_lay;
    QWidget       *_container_panel;
    QWidget       *_dynamic_panel;

    QTimer         _mode_check_timer;
    QWidget       *_stream_buffer_widget;
    QWidget       *_disk_cache_inline;
    QCheckBox     *_disk_cache_enable;
    QSpinBox      *_disk_cache_ram;
    QSpinBox      *_disk_cache_disk;
    QLineEdit     *_disk_cache_dir;
    QPushButton   *_disk_cache_browse;
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
