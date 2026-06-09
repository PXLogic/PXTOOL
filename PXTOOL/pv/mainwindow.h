/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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


#ifndef DSVIEW_PV_MAINWINDOW_H
#define DSVIEW_PV_MAINWINDOW_H

#include <list>
#include <QMainWindow>
#include <QTranslator>
#include "dialogs/dsmessagebox.h"
#include "interface/icallbacks.h"
#include "eventobject.h"
#include <QJsonDocument>
#include <chrono>
#include <QTimer>
#include "dstimer.h"
#include <QHBoxLayout>
#include <QHash>
#include <QList>
#include <QStackedWidget>
#include <QShortcut>
#include <libsigrok.h>
#include "sessioncallback.h"

class QAction;
class QMenuBar;
class QMenu;
class QVBoxLayout;
class QStatusBar;
class QToolBar;
class QWidget;
class QLabel;
class QDockWidget;
class AppControl;
class DeviceAgent;

using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

namespace pv {
 
class SigSession;

namespace toolbars {
class SamplingBar;
class TrigBar;
class FileBar;
class LogoBar;
class TitleBar;
}

namespace dock{
class ProtocolDock;
class TriggerDock;
class DsoTriggerDock;
class MeasureDock;
class SearchDock;
class SideBar;
}

namespace view {
class View;
}
 
//The mainwindow,referenced by MainFrame
//TODO: create graph view,toolbar,and show device list
class MainWindow : 
    public QMainWindow,
    public ISessionCallback,
    public IMainForm,
    public ISessionDataGetter,
    public IMessageListener
{
	Q_OBJECT

public:
    static const int Min_Width  = 350;
    static const int Min_Height = 300;
    static const int Base_Height = 150;
    static const int Per_Chan_Height = 35;
     
public:
    explicit MainWindow(toolbars::TitleBar *title_bar, QWidget *parent = 0);
    ~MainWindow();

    void openDoc();

public slots: 
    void switchTheme(QString style);
    void restore_dock();

private slots:
    void on_session_tab_add();
    void on_session_tab_switch(int index);
    void on_session_tab_close_by_uid(int uid);
	void on_load_file(QString file_name);
    void on_open_doc();
    void on_protocol(bool visible);
    void on_trigger(bool visible);
    void on_measure(bool visible);
    void on_search(bool visible);
    void on_display_options();
    void on_screenShot();
    void on_save();
    void on_export();
    void on_disk_cache_settings();

    // Shortcut slots
    void sc_start_collecting();
    void sc_stop_collecting();
    void sc_vernier_prev();
    void sc_vernier_next();
    void sc_measure();
    void sc_vernier_create();
    void sc_page_up();
    void sc_page_down();
    void sc_jump_zero();
    void sc_zoom_in();
    void sc_zoom_out();
    void sc_zoom_full();
    void sc_save();
    void sc_save_as();
    void sc_export();
    void sc_device_config();
    void sc_protocol_decode();
    void sc_label_measurement();
    void sc_data_search();
    void sc_close_session();
    void sc_delete_cursor();
    void on_shortcut_settings();
    bool on_load_session(QString name);  
    bool on_store_session(QString name); 
    void on_data_updated();
 
    void on_session_error();
    void on_signals_changed();
    void on_receive_trigger(quint64 trigger_pos);
    void on_frame_ended();
    void on_frame_began();
    void on_decode_done();
    void on_receive_data_len(quint64 len);
    void on_cur_snap_samplerate_changed();
    void on_trigger_message(int msg);
    void on_delay_prop_msg();
    void on_load_device_first();
  
signals:
    void prgRate(int progress);

public:
    //IMainForm
    void switchLanguage(int language) override;
    bool able_to_close();    
    QWidget* GetBodyView();
    
private:
    void setup_shortcuts();
    void rebuild_shortcuts();

private:
	void setup_ui();
    void retranslateUi();
    bool eventFilter(QObject *object, QEvent *event);
    void check_usb_device_speed();
    void reset_all_view();
    bool confirm_to_store_data();
    void update_toolbar_view_status();
    void calc_min_height();
    void update_title_bar_text();
    void setup_disk_cache_footer();
    void update_disk_cache_footer();
    QString format_disk_cache_bytes(uint64_t bytes) const;
    QString format_disk_cache_speed(uint64_t bytes_per_sec) const;

    //json operation
private:
    QString gen_config_file_path(bool isNewFormat);
    bool load_config_from_file(QString file);
    bool load_config_from_json(QJsonDocument &doc, bool &haveDecoder);
    void load_device_config();
    bool gen_config_json(QJsonObject &sessionVar);
    void save_config();
    bool save_config_to_file(QString file);
    void load_channel_view_indexs(QJsonDocument &doc); 
    QJsonDocument get_config_json_from_data_file(QString file, bool &bSucesss);
    QJsonArray get_decoder_json_from_data_file(QString file, bool &bSucesss);
    void check_config_file_version(); 
    void load_demo_decoder_config(QString optname);

  
private:
    //ISessionCallback 
    void session_error() override;
    void session_save() override;
    void data_updated() override;
    void update_capture() override;
    void cur_snap_samplerate_changed() override;      
    void signals_changed() override;
    void receive_trigger(quint64 trigger_pos) override;
    void frame_ended() override;
    void frame_began() override;
    void show_region(uint64_t start, uint64_t end, bool keep) override;
    void show_wait_trigger() override;
    void repeat_hold(int percent) override;
    void decode_done() override;
    void receive_data_len(quint64 len) override;
    void receive_header() override;    
    void trigger_message(int msg) override;   
    void delay_prop_msg(QString strMsg) override; 

    //ISessionDataGetter
    bool genSessionData(std::string &str) override;

    //IMessageListener
    void OnMessage(int msg) override;

private:
    // One entry per session tab
    struct SessionItem {
        int                uid = -1;                   // stable id, survives index shuffles
        SigSession        *session;
        pv::view::View    *view;
        QString            name;
        SessionCallback   *cb = nullptr;               // per-session ISessionCallback proxy
    };

    // Per-device group: holds an ordered list of session UIDs that share
    // the same physical/logical device handle.
    struct DeviceGroup {
        ds_device_handle  handle = NULL_HANDLE;
        QString           display_name;
        int               dev_type = 0;             // sr_device_type
        QList<int>        session_uids;             // ordered (tab render order)
        int               active_session_uid = -1;  // last-activated session in this group
        bool              offline = false;
        qint64            offline_since_ms = 0;
    };

private: 
	pv::view::View          *_view;
    dialogs::DSMessageBox   *_msg;

	QMenuBar                *_menu_bar;
	QMenu                   *_menu_file;
	QAction                 *_action_open;
	QAction                 *_action_connect;
	QAction                 *_action_quit;

	QMenu                   *_menu_view;
	QAction                 *_action_view_zoom_in;
	QAction                 *_action_view_zoom_out;
	QAction                 *_action_view_show_cursors;

	QMenu                   *_menu_help;
	QAction                 *_action_about;

    // File menu actions
    QMenu   *_menu_session    = nullptr;
    QAction *_action_load     = nullptr;
    QAction *_action_store    = nullptr;
    QAction *_action_default  = nullptr;
    QAction *_action_save     = nullptr;
    QAction *_action_export   = nullptr;
    QAction *_action_capture  = nullptr;
    QAction *_action_disk_cache = nullptr;

    // Window menu
    QMenu   *_menu_themes        = nullptr;
    QAction *_action_dark        = nullptr;
    QAction *_action_light       = nullptr;
    QAction *_action_display_opts = nullptr;
    QAction *_action_shortcuts   = nullptr;

    // Help menu actions
    QAction *_action_doc     = nullptr;
    QAction *_action_issue   = nullptr;
    QAction *_action_update  = nullptr;
    QAction *_action_log_item = nullptr;
    QMenu   *_menu_language  = nullptr;
    QAction *_action_lang_en = nullptr;
    QAction *_action_lang_cn = nullptr;
    QAction *_action_lang_tw = nullptr;

	QWidget                 *_central_widget;
	QVBoxLayout             *_vertical_layout;

	toolbars::SamplingBar   *_sampling_bar;
    QToolBar                *_device_bar;
    toolbars::TrigBar       *_trig_bar;
    toolbars::FileBar       *_file_bar;
    toolbars::LogoBar       *_logo_bar; //help button, on top right
    toolbars::TitleBar      *_title_bar;


    QDockWidget             *_sidebar_dock;
    dock::SideBar           *_sidebar_widget;

    QTranslator     _qtTrans;
    QTranslator     _appTrans;
    EventObject     _event; 
    SigSession      *_session;
    DeviceAgent     *_device_agent;
    bool            _is_switching_session;
    high_resolution_clock::time_point _last_key_press_time;
    bool            _is_save_confirm_msg;
    QString         _pattern_mode;
    QWidget         *_frame;
    DsTimer         _delay_prop_msg_timer;
    QString         _strMsg;
    QString         _lst_title_string;
    QString         _title_ext_string;

    int         _key_value;
    bool        _key_vaild;

    // Multi-session tabs
    QList<SessionItem>  _session_items;
    QHash<int, int>     _uid_to_index;          // uid -> global index into _session_items
    int                 _next_session_uid = 0;
    int                 _bootstrap_uid = -1;
    int                 _bootstrap_uid_protected = -1;  // never cleared; the AppControl-owned SigSession
    QStackedWidget     *_session_stack;
    QWidget            *_session_tab_bar;
    QHBoxLayout        *_tab_bar_layout;
    QWidget            *_disk_cache_footer_line = nullptr;
    QWidget            *_disk_cache_footer = nullptr;
    QLabel             *_disk_cache_footer_label = nullptr;
    QTimer              _disk_cache_footer_timer;

    // Device-grouped multi-session bookkeeping (Task 2: scaffolding only,
    // no callers yet).
    QList<DeviceGroup>  _device_groups;
    int                 _active_group_index = -1;
    QTimer              _offline_gc_timer;
    int                 _pending_close_uid = -1;

    void rebuild_tab_buttons();
    void rebuild_uid_index();
    void update_tab_bar_style();
    void switch_to_session(int index);
    void switch_to_device(ds_device_handle handle);
    void switch_to_session_for_handle(int global_index, ds_device_handle handle);

    // DeviceGroup helpers (Task 2)
    DeviceGroup       *current_group();
    DeviceGroup       *find_group_by_handle(ds_device_handle handle);
    DeviceGroup       *group_owning_session(int uid);
    int                index_of_group(DeviceGroup *g) const;
    int                active_session_global_index_of_group(DeviceGroup *g);
    DeviceGroup       *create_group(ds_device_handle handle);
    int                create_session_in_group(DeviceGroup *grp);
    void               register_groups_from_device_list();
    void               mark_offline_for_missing_handles();
    ds_device_handle   find_latest_device_handle();
    ds_device_handle   find_latest_hardware_device_handle();
    ds_device_handle   find_demo_device_handle();
    ds_device_handle   pick_default_device_handle();
    void               gc_offline_groups();

    QList<QShortcut*> _shortcuts;
};

} // namespace pv

#endif // DSVIEW_PV_MAINWINDOW_H
