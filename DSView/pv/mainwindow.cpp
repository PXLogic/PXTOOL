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

#include <QAction>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QToolBar>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QEvent>
#include <QtGlobal>
#include <QApplication>
#include <QStandardPaths>
#include <QScreen>
#include <QTimer>
#include <QFileInfo>
#include <libusb-1.0/libusb.h>
#include <QGuiApplication>
#include <QTextStream>
#include <QJsonValue>
#include <QJsonArray>
#include <functional>

//include with qt5
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QDesktopWidget>
#endif

#include "mainwindow.h"

#include "data/logicsnapshot.h"
#include "data/dsosnapshot.h"
#include "data/analogsnapshot.h"

#include "dialogs/about.h"
#include "dialogs/deviceoptions.h"
#include "dialogs/storeprogress.h"
#include "dialogs/waitingdialog.h"
#include "dialogs/regionoptions.h"
#include "dialogs/applicationpardlg.h"
#include "dialogs/shortcutdlg.h"

#include "toolbars/samplingbar.h"
#include "toolbars/trigbar.h"
#include "toolbars/filebar.h"
#include "toolbars/logobar.h"
#include "toolbars/titlebar.h"

#include "dock/triggerdock.h"
#include "dock/dsotriggerdock.h"
#include "dock/measuredock.h"
#include "dock/searchdock.h"
#include "dock/protocoldock.h"
#include "dock/sidebar.h"

#include "view/view.h"
#include "view/trace.h"
#include "view/signal.h"
#include "view/dsosignal.h"
#include "view/logicsignal.h"
#include "view/analogsignal.h"

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <list>
#include "ui/msgbox.h"
#include "config/appconfig.h"
#include "appcontrol.h"
#include "dsvdef.h"
#include "appcontrol.h"
#include "utility/encoding.h"
#include "utility/path.h"
#include "log.h"
#include "sigsession.h"
#include "deviceagent.h"
#include <stdlib.h>
#include "ZipMaker.h"
#include "mainframe.h"
#include "dsvdef.h"
#include <thread>
#include "ui/uimanager.h"

namespace pv
{

    namespace{
        QString tmp_file;
    }

    MainWindow::MainWindow(toolbars::TitleBar *title_bar, QWidget *parent)
        : QMainWindow(parent)
    {
        _msg = NULL;
        _frame = parent; 

        assert(title_bar);
        assert(_frame);

        _title_bar = title_bar;

        _session = AppControl::Instance()->GetSession();
        _session->set_callback(this);
        _device_agent = _session->get_device();
        _session->add_msg_listener(this);

        _is_auto_switch_device = false;
        _is_switching_session = false;
        _is_save_confirm_msg = false;

        _pattern_mode = "random";

        _active_tab_index = 0;
        _session_stack = nullptr;
        _session_tab_bar = nullptr;
        _tab_bar_layout = nullptr;

        setup_ui();

        setContextMenuPolicy(Qt::NoContextMenu);

        _key_vaild = false;
        _last_key_press_time = high_resolution_clock::now();

        update_title_bar_text();
    }

    MainWindow::~MainWindow()
    {
        if (_sampling_bar != nullptr)
            _sampling_bar->detachFromDeviceBar();

        // Free per-session callback proxies owned by session items.
        // The SigSession objects themselves are managed by AppControl (session 0)
        // or deleted explicitly for extra sessions.
        for (SessionItem &item : _session_items) {
            delete item.cb;
            item.cb = nullptr;
        }
    }

    void MainWindow::setup_ui()
    {
        setObjectName(QString::fromUtf8("MainWindow"));
        setContentsMargins(0, 0, 0, 0);
        layout()->setSpacing(0);

        // Setup the central widget
        _central_widget = new QWidget(this);
        _vertical_layout = new QVBoxLayout(_central_widget);
        _vertical_layout->setSpacing(0);
        _vertical_layout->setContentsMargins(0, 0, 0, 0);
        setCentralWidget(_central_widget);

        // Setup capture controls: top device strip + logic owner (hidden QToolBar)
        _sampling_bar = new toolbars::SamplingBar(_session, this);
        _sampling_bar->setObjectName("sampling_bar");
        _device_bar = new QToolBar(this);
        _device_bar->setObjectName("device_bar");
        _device_bar->setIconSize(QSize(14, 14));
        _sampling_bar->attachCaptureToolBar(_device_bar);
        _sampling_bar->hide();

        _trig_bar = new toolbars::TrigBar(_session, this);
        _trig_bar->setObjectName("trig_bar");
        _trig_bar->hide();
        _file_bar = new toolbars::FileBar(_session, this);
        _file_bar->setObjectName("file_bar");
        _file_bar->hide();
        _logo_bar = new toolbars::LogoBar(_session, this);
        _logo_bar->setObjectName("logo_bar");
        _logo_bar->hide();

        // QStackedWidget holds one View per session tab
        _session_stack = new QStackedWidget(_central_widget);
        _vertical_layout->addWidget(_session_stack);

        // Setup first (device) session view
        _view = new pv::view::View(_session, _sampling_bar, this);
        _session_stack->addWidget(_view);

        // Register first session item with its own SessionCallback proxy
        SessionCallback *firstCb = new SessionCallback(this);
        firstCb->setActive(true); // session 0 is active at startup
        _session->set_callback(firstCb);

        SessionItem firstItem;
        firstItem.session = _session;
        firstItem.view    = _view;
        firstItem.name    = tr("Device");
        firstItem.cb      = firstCb;
        _session_items.append(firstItem);

        TabEntry deviceTab;
        deviceTab.name = tr("Device");
        _tabs.append(deviceTab);

        // Session tab bar (bottom strip, like atk-logic stateBar)
        _session_tab_bar = new QWidget(_central_widget);
        _session_tab_bar->setFixedHeight(30);
        _session_tab_bar->setObjectName("session_tab_bar");
        _tab_bar_layout = new QHBoxLayout(_session_tab_bar);
        _tab_bar_layout->setContentsMargins(6, 0, 6, 0);
        _tab_bar_layout->setSpacing(2);
        _vertical_layout->addWidget(_session_tab_bar);

        rebuild_tab_buttons();

        // Unified sidebar (replaces individual trigger/protocol/measure/search docks)
        _sidebar_dock = new QDockWidget(this);
        _sidebar_dock->setObjectName("sidebar_dock");
        _sidebar_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        _sidebar_dock->setTitleBarWidget(new QWidget(_sidebar_dock));
        _sidebar_dock->setAllowedAreas(Qt::RightDockWidgetArea);

        _sidebar_widget = new dock::SideBar(_sidebar_dock, *_view, _session);
        _sidebar_dock->setWidget(_sidebar_widget);

        _session->set_decoder_pannel(_sidebar_widget->protocol_widget());

        addDockWidget(Qt::RightDockWidgetArea, _sidebar_dock);

        // Standalone menu bar — NOT QMainWindow::menuBar() to avoid QMainWindow
        // placing it in its internal menu slot (which renders a collapsed button
        // even when hidden).
        _menu_bar = new QMenuBar(this);
        _menu_bar->setObjectName("main_menu_bar");
        _menu_bar->setNativeMenuBar(false);
        _menu_bar->hide();

        // File menu
        _menu_file = _menu_bar->addMenu(tr("File"));
        _menu_session = _menu_file->addMenu(tr("Config..."));
        _action_load    = _menu_session->addAction(tr("Load..."));
        _action_store   = _menu_session->addAction(tr("Store..."));
        _action_default = _menu_session->addAction(tr("Default..."));
        _action_open = _menu_file->addAction(tr("Open..."));
        _action_save    = _menu_file->addAction(tr("Save..."));
        _action_export  = _menu_file->addAction(tr("Export..."));
        _action_capture = _menu_file->addAction(tr("Capture..."));
        _menu_file->addSeparator();
        _action_quit = _menu_file->addAction(tr("Exit"));
        connect(_action_load,    SIGNAL(triggered()), _file_bar, SLOT(on_actionLoad_triggered()));
        connect(_action_store,   SIGNAL(triggered()), _file_bar, SLOT(on_actionStore_triggered()));
        connect(_action_default, SIGNAL(triggered()), _file_bar, SLOT(on_actionDefault_triggered()));
        connect(_action_open,    SIGNAL(triggered()), _file_bar, SLOT(on_actionOpen_triggered()));
        connect(_action_save,    &QAction::triggered, this, &MainWindow::on_save);
        connect(_action_export,  &QAction::triggered, this, &MainWindow::on_export);
        connect(_action_capture, SIGNAL(triggered()), _file_bar, SLOT(on_actionCapture_triggered()));
        connect(_action_quit,    &QAction::triggered, this, &MainWindow::close);

        // Window menu
        _menu_view = _menu_bar->addMenu(tr("Window"));
        _menu_themes = _menu_view->addMenu(tr("Themes"));
        _action_dark  = _menu_themes->addAction(tr("Dark"));
        _action_light = _menu_themes->addAction(tr("Light"));
        connect(_action_dark,  SIGNAL(triggered()), _trig_bar, SLOT(on_actionDark_triggered()));
        connect(_action_light, SIGNAL(triggered()), _trig_bar, SLOT(on_actionLight_triggered()));
        _action_display_opts  = _menu_view->addAction(tr("Display Options..."));
        connect(_action_display_opts, SIGNAL(triggered()), _trig_bar, SLOT(on_display_setting()));
        _action_shortcuts = _menu_view->addAction(tr("Keyboard Shortcuts..."));
        connect(_action_shortcuts, &QAction::triggered, this, &MainWindow::on_shortcut_settings);

        // Help menu
        _menu_help = _menu_bar->addMenu(tr("Help"));
        _action_about = _menu_help->addAction(tr("About"));
        _action_doc    = _menu_help->addAction(tr("Manual"));
        _action_issue  = _menu_help->addAction(tr("Bug Report"));
        _action_update = _menu_help->addAction(tr("Check for Updates"));
        _action_log_item = _menu_help->addAction(tr("Log Options"));
        _menu_help->addSeparator();
        _menu_language = _menu_help->addMenu(tr("Language"));
        _action_lang_en = _menu_language->addAction(tr("English"));
        _action_lang_cn = _menu_language->addAction(tr("中文"));
        connect(_action_about,    SIGNAL(triggered()), _logo_bar, SLOT(on_actionAbout_triggered()));
        connect(_action_doc,      &QAction::triggered, this, &MainWindow::on_open_doc);
        connect(_action_issue,    SIGNAL(triggered()), _logo_bar, SLOT(on_actionIssue_triggered()));
        connect(_action_update,   SIGNAL(triggered()), _logo_bar, SLOT(on_action_update()));
        connect(_action_log_item, SIGNAL(triggered()), _logo_bar, SLOT(on_action_setting_log()));
        connect(_action_lang_en,  SIGNAL(triggered()), _logo_bar, SLOT(on_actionEn_triggered()));
        connect(_action_lang_cn,  SIGNAL(triggered()), _logo_bar, SLOT(on_actionCn_triggered()));

        // Move File/Window/Help into the title bar and hide the in-window menu bar
        _title_bar->addMenusToTitleBar(_menu_file, _menu_view, _menu_help);
        _menu_bar->hide();

        setIconSize(QSize(40, 40));
        addToolBar(_device_bar);

        // event filter
        _view->installEventFilter(this);
        _device_bar->installEventFilter(this);
        _trig_bar->installEventFilter(this);
        _file_bar->installEventFilter(this);
        _logo_bar->installEventFilter(this);
        _sidebar_dock->installEventFilter(this);

        // defaut language
        AppConfig &app = AppConfig::Instance();
        switchLanguage(app.frameOptions.language);
        switchTheme(app.frameOptions.style);

        _sampling_bar->set_view(_view);

        // event
        connect(&_event, SIGNAL(session_error()), this, SLOT(on_session_error()));
        connect(&_event, SIGNAL(signals_changed()), this, SLOT(on_signals_changed()));
        connect(&_event, SIGNAL(receive_trigger(quint64)), this, SLOT(on_receive_trigger(quint64)));
        connect(&_event, SIGNAL(frame_ended()), this, SLOT(on_frame_ended()), Qt::DirectConnection);
        connect(&_event, SIGNAL(frame_began()), this, SLOT(on_frame_began()), Qt::DirectConnection);
        connect(&_event, SIGNAL(decode_done()), this, SLOT(on_decode_done()));
        connect(&_event, SIGNAL(data_updated()), this, SLOT(on_data_updated()));
        connect(&_event, SIGNAL(cur_snap_samplerate_changed()), this, SLOT(on_cur_snap_samplerate_changed()));
        connect(&_event, SIGNAL(receive_data_len(quint64)), this, SLOT(on_receive_data_len(quint64)));
        connect(&_event, SIGNAL(trigger_message(int)), this, SLOT(on_trigger_message(int)));

        // view
        connect(_view, SIGNAL(cursor_update()), _sidebar_widget->measure_widget(), SLOT(cursor_update()));
        connect(_view, SIGNAL(cursor_moving()), _sidebar_widget->measure_widget(), SLOT(cursor_moving()));
        connect(_view, SIGNAL(cursor_moved()), _sidebar_widget->measure_widget(), SLOT(reCalc()));
        connect(_view, SIGNAL(prgRate(int)), this, SIGNAL(prgRate(int)));
        connect(_view, SIGNAL(auto_trig(int)), _sidebar_widget->dso_trigger_widget(), SLOT(auto_trig(int)));
        connect(_sidebar_widget, SIGNAL(sig_search_visible(bool)), _view, SLOT(show_search_cursor(bool)));

        // trig_bar
        connect(_trig_bar, SIGNAL(sig_protocol(bool)), this, SLOT(on_protocol(bool)));
        connect(_trig_bar, SIGNAL(sig_trigger(bool)), this, SLOT(on_trigger(bool)));
        connect(_trig_bar, SIGNAL(sig_measure(bool)), this, SLOT(on_measure(bool)));
        connect(_trig_bar, SIGNAL(sig_search(bool)), this, SLOT(on_search(bool)));
        connect(_trig_bar, SIGNAL(sig_setTheme(QString)), this, SLOT(switchTheme(QString)));
        connect(_trig_bar, SIGNAL(sig_show_lissajous(bool)), _view, SLOT(show_lissajous(bool)));

        // file toolbar
        connect(_file_bar, SIGNAL(sig_load_file(QString)), this, SLOT(on_load_file(QString)));
        connect(_file_bar, SIGNAL(sig_save()), this, SLOT(on_save()));
        connect(_file_bar, SIGNAL(sig_export()), this, SLOT(on_export()));
        connect(_file_bar, SIGNAL(sig_screenShot()), this, SLOT(on_screenShot()), Qt::QueuedConnection);
        connect(_file_bar, SIGNAL(sig_load_session(QString)), this, SLOT(on_load_session(QString)));
        connect(_file_bar, SIGNAL(sig_store_session(QString)), this, SLOT(on_store_session(QString)));

        // logobar
        connect(_logo_bar, SIGNAL(sig_open_doc()), this, SLOT(on_open_doc()));

        connect(_sidebar_widget->protocol_widget(), SIGNAL(protocol_updated()), this, SLOT(on_signals_changed()));

        // SamplingBar
        connect(_sampling_bar, SIGNAL(sig_store_session_data()), this, SLOT(on_save()));

        //
        connect(_sidebar_widget->dso_trigger_widget(), SIGNAL(set_trig_pos(int)), _view, SLOT(set_trig_pos(int)));

        _delay_prop_msg_timer.SetCallback(std::bind(&MainWindow::on_delay_prop_msg, this));
 
        _logo_bar->set_mainform_callback(this);

        // Try load from file.
        QString ldFileName(AppControl::Instance()->_open_file_name.c_str());
        if (ldFileName != "")
        {   
            std::string file_name = pv::path::ToUnicodePath(ldFileName);

            if (QFile::exists(ldFileName))
            {              
                dsv_info("Auto load file:%s", file_name.c_str());
                tmp_file = ldFileName;
            }
            else
            {
                dsv_err("file is not exists:%s", file_name.c_str());
                MsgBox::Show(tr("Open file error!"), ldFileName, NULL);
            }
        }

        on_load_device_first();
        setup_shortcuts();
    }

    void MainWindow::on_load_device_first()
    {
        if (tmp_file != ""){
            on_load_file(tmp_file);
            tmp_file = "";
        }
        else{
            _session->set_default_device();
        }
    }

    void MainWindow::retranslateUi()
    {
        // Menu titles
        if (_menu_file)  _menu_file->setTitle(tr("File"));
        if (_menu_view)  _menu_view->setTitle(tr("Window"));
        if (_menu_help)  _menu_help->setTitle(tr("Help"));

        // File menu items
        if (_menu_session)   _menu_session->setTitle(tr("Config..."));
        if (_action_load)    _action_load->setText(tr("Load..."));
        if (_action_store)   _action_store->setText(tr("Store..."));
        if (_action_default) _action_default->setText(tr("Default..."));
        if (_action_open)    _action_open->setText(tr("Open..."));
        if (_action_save)    _action_save->setText(tr("Save..."));
        if (_action_export)  _action_export->setText(tr("Export..."));
        if (_action_capture) _action_capture->setText(tr("Capture..."));
        if (_action_quit)    _action_quit->setText(tr("Exit"));

        // Window menu items
        if (_menu_themes)         _menu_themes->setTitle(tr("Themes"));
        if (_action_dark)         _action_dark->setText(tr("Dark"));
        if (_action_light)        _action_light->setText(tr("Light"));
        if (_action_display_opts) _action_display_opts->setText(tr("Display Options..."));
        if (_action_shortcuts)    _action_shortcuts->setText(tr("Keyboard Shortcuts..."));

        // Help menu items
        if (_action_about)    _action_about->setText(tr("About"));
        if (_action_doc)      _action_doc->setText(tr("Manual"));
        if (_action_issue)    _action_issue->setText(tr("Bug Report"));
        if (_action_update)   _action_update->setText(tr("Check for Updates"));
        if (_action_log_item) _action_log_item->setText(tr("Log Options"));
        if (_menu_language)   _menu_language->setTitle(tr("Language"));
        if (_action_lang_en)  _action_lang_en->setText(tr("English"));

        // Child widget retranslation
        if (_title_bar)      _title_bar->retranslateUi();
        if (_sampling_bar)   _sampling_bar->retranslateUi();
        if (_sidebar_widget) _sidebar_widget->retranslateUi();
    }

    // -----------------------------------------------------------------------
    // Session tab bar helpers
    // -----------------------------------------------------------------------

    void MainWindow::rebuild_tab_buttons()
    {
        // Remove all existing widgets from the layout
        while (_tab_bar_layout->count() > 0) {
            QLayoutItem *item = _tab_bar_layout->takeAt(0);
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }

        bool isDark = AppConfig::Instance().IsDarkStyle();
        QString bgColor   = isDark ? "#1E1E1E" : "#E8E8E8";
        QString selBg     = isDark ? "#2D2D2D" : "#FFFFFF";
        QString textColor = isDark ? "#CCCCCC" : "#333333";
        QString selText   = isDark ? "#FFFFFF" : "#000000";

        QString closeBtnStyle = isDark
            ? "QPushButton{background:transparent;color:#888;border:none;"
              "font-size:11px;padding:0px;max-width:14px;min-width:14px;}"
              "QPushButton:hover{color:#FF6060;}"
            : "QPushButton{background:transparent;color:#999;border:none;"
              "font-size:11px;padding:0px;max-width:14px;min-width:14px;}"
              "QPushButton:hover{color:#FF0000;}";

        for (int i = 0; i < _tabs.size(); ++i) {
            const TabEntry &tab = _tabs.at(i);
            bool isActive = (i == _active_tab_index);
            bool isDevice = (i == 0);

            QWidget *tabWidget = new QWidget(_session_tab_bar);
            tabWidget->setFixedHeight(24);
            QHBoxLayout *tl = new QHBoxLayout(tabWidget);
            tl->setContentsMargins(8, 0, isDevice ? 8 : 4, 0);
            tl->setSpacing(5);

            // Dot indicator: green for device, blue for additional sessions
            QWidget *dot = new QWidget(tabWidget);
            dot->setFixedSize(6, 6);
            dot->setStyleSheet(QString("background:%1;border-radius:3px;")
                .arg(isDevice
                    ? (isDark ? "#4CAF50" : "#1E7B34")   // green = device
                    : (isDark ? "#5B9BD5" : "#2563EB"))); // blue = extra session
            tl->addWidget(dot);

            // Tab name button
            QPushButton *nameBtn = new QPushButton(tab.name, tabWidget);
            nameBtn->setFlat(true);
            nameBtn->setCursor(Qt::PointingHandCursor);
            nameBtn->setStyleSheet(QString(
                "QPushButton{background:transparent;color:%1;border:none;"
                "font-size:12px;padding:0px;text-align:left;}"
                "QPushButton:hover{color:%2;}")
                .arg(isActive ? selText : textColor)
                .arg(selText));
            int capturedIndex = i;
            connect(nameBtn, &QPushButton::clicked, [this, capturedIndex](){
                on_session_tab_switch(capturedIndex);
            });
            tl->addWidget(nameBtn);

            // Close button for non-device tabs
            if (!isDevice) {
                QPushButton *closeBtn = new QPushButton("×", tabWidget);
                closeBtn->setFlat(true);
                closeBtn->setCursor(Qt::PointingHandCursor);
                closeBtn->setStyleSheet(closeBtnStyle);
                connect(closeBtn, &QPushButton::clicked, [this, capturedIndex](){
                    on_session_tab_close(capturedIndex);
                });
                tl->addWidget(closeBtn);
            }

            tabWidget->setStyleSheet(QString(
                "QWidget{background:%1;border-radius:4px;}")
                .arg(isActive ? selBg : bgColor));

            _tab_bar_layout->addWidget(tabWidget);
        }

        // Spacer then "+" button
        _tab_bar_layout->addStretch(1);

        QPushButton *addBtn = new QPushButton("+", _session_tab_bar);
        addBtn->setFixedSize(24, 24);
        addBtn->setCursor(Qt::PointingHandCursor);
        addBtn->setToolTip(tr("Open file in new comparison tab"));
        addBtn->setStyleSheet(QString(
            "QPushButton{background:%1;color:%2;border-radius:4px;"
            "font-size:16px;font-weight:bold;border:none;}"
            "QPushButton:hover{background:%3;}")
            .arg(isDark ? "#2A2A2A" : "#E0E0E0")
            .arg(isDark ? "#AAAAAA" : "#444444")
            .arg(isDark ? "#3A3A3A" : "#D0D0D0"));
        connect(addBtn, &QPushButton::clicked, this, &MainWindow::on_session_tab_add);
        _tab_bar_layout->addWidget(addBtn);

        // Background style for the whole bar
        _session_tab_bar->setStyleSheet(QString(
            "QWidget#session_tab_bar{background:%1;border-top:1px solid %2;}")
            .arg(isDark ? "#1A1A1A" : "#D8D8D8")
            .arg(isDark ? "#333333" : "#BBBBBB"));
    }

    void MainWindow::update_tab_bar_style()
    {
        rebuild_tab_buttons();
    }

    void MainWindow::on_session_tab_add()
    {
        // Create a new independent session tab in the same window.
        // Each session has its own SigSession, View, and SamplingBar,
        // allowing independent device connection and waveform capture.

        // Build a new session name
        QString newName = tr("Session %1").arg(_session_items.size() + 1);

        // Create new session + per-session callback proxy (inactive until switched to)
        SigSession *newSession = new SigSession();
        SessionCallback *newCb = new SessionCallback(this);
        newSession->set_callback(newCb);

        pv::view::View *newView = new pv::view::View(newSession, _sampling_bar, this);
        _session_stack->addWidget(newView);

        SessionItem item;
        item.session = newSession;
        item.view    = newView;
        item.name    = newName;
        item.cb      = newCb;
        _session_items.append(item);

        TabEntry entry;
        entry.name = newName;
        _tabs.append(entry);

        // Switch to the new tab — this also attaches the new sampling bar
        switch_to_session(_session_items.size() - 1);
    }

    void MainWindow::switch_to_session(int index)
    {
        if (index < 0 || index >= _session_items.size())
            return;
        if (index == _active_tab_index)
            return;

        dsv_info("switch_to_session: %d -> %d | from-session@%p has_data=%d | to-session@%p has_data=%d",
            _active_tab_index, index,
            (void*)_session_items[_active_tab_index].session,
            (int)_session_items[_active_tab_index].session->have_view_data(),
            (void*)_session_items[index].session,
            (int)_session_items[index].session->have_view_data());

        _is_switching_session = true;

        // Save the outgoing session's channel enabled states while the device is
        // still active for it.  rebind_device() will restore these when we return.
        _session_items[_active_tab_index].session->save_channel_enabled_states();

        // Deactivate outgoing session:
        // - disable its callback proxy so its async events don't reach MainWindow
        // - stop capture if running
        // - remove IMessageListener so broadcast_msg() is no longer received
        SessionItem &oldItem = _session_items[_active_tab_index];
        if (oldItem.cb)
            oldItem.cb->setActive(false);
        if (oldItem.session->is_working())
            oldItem.session->stop_capture();
        oldItem.session->remove_msg_listener(this);

        // Activate incoming session
        _active_tab_index = index;
        SessionItem &newItem = _session_items[index];

        _session      = newItem.session;
        _view         = newItem.view;
        _device_agent = _session->get_device();

        // Route libsigrok C-library callbacks to the new session
        newItem.session->set_as_current();
        newItem.session->add_msg_listener(this);

        // Activate the new session's callback proxy so events reach MainWindow
        if (newItem.cb)
            newItem.cb->setActive(true);

        _sampling_bar->setSession(_session);
        _sampling_bar->set_view(_view);
        _session_stack->setCurrentWidget(_view);
        _sidebar_widget->setSession(_session);

        _is_switching_session = false;

        rebuild_tab_buttons();

        if (newItem.saved_handle != NULL_HANDLE) {
            // Always rebind: ds_active_device() may have freed and recreated
            // sr_channel structs since this session was last active (e.g. when
            // another session opened the same virtual-demo device).
            // rebind_device() calls refresh_signal_probes() to patch dangling
            // _probe pointers before any rendering attempt.
            _session->rebind_device(newItem.saved_handle);

            if (_session->have_view_data()) {
                _view->set_all_update(true);
                _view->signals_changed(NULL);
                _view->data_updated();
                _view->on_state_changed(true);  // mirrors DSV_MSG_COLLECT_END

                // Run the same refresh once again in the next event loop tick.
                // On tab switch, the incoming view geometry can still be settling;
                // delaying one tick avoids stale width/offset/cache state.
                SigSession *sessToRefresh = _session;
                QTimer::singleShot(0, this, [this, sessToRefresh]() {
                    if (_session == sessToRefresh && _session->have_view_data()) {
                        _view->set_all_update(true);
                        _view->signals_changed(NULL);
                        _view->data_updated();
                        _view->on_state_changed(true);
                        _view->update_view_port();
                        dsv_info("switch_to_session delayed refresh: session@%p width=%d has_data=%d",
                                 (void*)_session, _view->get_view_width(),
                                 (int)_session->have_view_data());
                    }
                });
            }
        } else if (index != 0) {
            // First visit to a new extra session: run full device init.
            // Capture sessToInit so that if the user switches away before the
            // timer fires, set_default_device() is still called on the CORRECT
            // session rather than whatever _session points to at fire time.
            SigSession *sessToInit = newItem.session;
            QTimer::singleShot(0, this, [this, sessToInit]() {
                if (_session == sessToInit) {
                    // Still on this session — init normally.
                    _session->set_default_device();
                }
                // If user switched away the session still has saved_handle ==
                // NULL_HANDLE, so the next switch_to_session() call will
                // re-enter this branch and retry initialisation.
            });
        }

        update_toolbar_view_status();
        dsv_info("switch_to_session done: active=%d session@%p has_data=%d device_status=%d",
            _active_tab_index, (void*)_session,
            (int)_session->have_view_data(), (int)_session->get_device_status());
    }

    void MainWindow::on_session_tab_switch(int index)
    {
        switch_to_session(index);
    }

    void MainWindow::on_session_tab_close(int index)
    {
        if (index <= 0 || index >= _session_items.size())
            return; // cannot close the device tab (index 0)

        // Stop and clean up the closing session
        SessionItem &item = _session_items[index];

        // Deactivate callback proxy first so no more events reach MainWindow
        if (item.cb)
            item.cb->setActive(false);
        if (item.session->is_working())
            item.session->stop_capture();
        item.session->remove_msg_listener(this);

        // If closing the active tab, fall back to the device tab
        if (_active_tab_index == index) {
            _active_tab_index = 0;
            SessionItem &devItem = _session_items[0];
            _session      = devItem.session;
            _view         = devItem.view;
            _device_agent = _session->get_device();
            devItem.session->set_as_current();
            devItem.session->add_msg_listener(this);
            if (devItem.cb)
                devItem.cb->setActive(true);
            _sampling_bar->setSession(_session);
            _sampling_bar->set_view(_view);
            _session_stack->setCurrentWidget(_view);
            _sidebar_widget->setSession(_session);
            update_toolbar_view_status();
        } else if (_active_tab_index > index) {
            _active_tab_index--;
        }

        // Remove view from stack, then free session resources.
        // SessionCallback is owned by SessionItem; delete it with the session.
        _session_stack->removeWidget(item.view);
        delete item.view;
        delete item.cb;
        delete item.session;

        _session_items.removeAt(index);
        _tabs.removeAt(index);

        rebuild_tab_buttons();
    }

    // -----------------------------------------------------------------------

    void MainWindow::on_load_file(QString file_name)
    {
        try
        {
            if (_device_agent->is_hardware()){
                save_config();
            }

            _session->set_file(file_name);
        }
        catch (QString e)
        {   
            QString strMsg(tr("Failed to load "));
            strMsg += file_name;
            MsgBox::Show(strMsg);
            _session->set_default_device();
        }
    }

    void MainWindow::session_error()
    {
        _event.session_error();
    }

    void MainWindow::session_save()
    {
        save_config();
    }

    void MainWindow::on_session_error()
    {
        QString title;
        QString details;
        QString ch_status = "";

        switch (_session->get_error())
        {
        case SigSession::Hw_err:
            dsv_info("MainWindow::on_session_error(),Hw_err, stop capture");
            _session->stop_capture();
            title = tr("Hardware Operation Failed");
            details = tr("Please replug device to refresh hardware configuration!");
            break;
        case SigSession::Malloc_err:
            dsv_info("MainWindow::on_session_error(),Malloc_err, stop capture");
            _session->stop_capture();
            title = tr("Malloc Error");
            details = tr("Memory is not enough for this sample!\nPlease reduce the sample depth!");
            break;
        case SigSession::Pkt_data_err:
            title = tr("Packet Error");
            details = tr("the content of received packet are not expected!");
            _session->refresh(0);
            break;
        case SigSession::Data_overflow:
            dsv_info("MainWindow::on_session_error(),Data_overflow, stop capture");
            _session->stop_capture();
            title = tr("Data Overflow");
            details = tr("USB bandwidth can not support current sample rate! \nPlease reduce the sample rate!");
            break;
        default:
            title = tr("Undefined Error");
            details = tr("Not expected error!");
            break;
        }

        pv::dialogs::DSMessageBox msg(this, title);
        msg.mBox()->setText(details);
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        connect(_session->device_event_object(), SIGNAL(device_updated()), &msg, SLOT(accept()));
        _msg = &msg;
        msg.exec();
        _msg = NULL;

        _session->clear_error();
    }

    void MainWindow::save_config()
    { 
        if (_device_agent->have_instance() == false)
        {
            dsv_info("There is no need to save the configuration");
            return;
        }

        AppConfig &app = AppConfig::Instance();        

        if (_device_agent->is_hardware()){
            QString sessionFile = gen_config_file_path(true);
            save_config_to_file(sessionFile);
        }

        app.frameOptions.windowState = saveState();
        app.SaveFrame();
    }

    QString MainWindow::gen_config_file_path(bool isNewFormat)
    { 
        AppConfig &app = AppConfig::Instance();

        QString file = GetProfileDir();
        QDir dir(file);
        if (dir.exists() == false){
            dir.mkpath(file);
        } 

        QString driver_name = _device_agent->driver_name();
        QString mode_name = QString::number(_device_agent->get_work_mode()); 
        QString lang_name;
        QString base_path = dir.absolutePath() + "/" + driver_name + mode_name;

        if (!isNewFormat){
            lang_name = QString::number(app.frameOptions.language);           
        }

        return base_path + ".ses" + lang_name + ".dsc";
    }

    bool MainWindow::able_to_close()
    {
        if (_device_agent->is_hardware() && _session->have_hardware_data() == false){
            _sampling_bar->commit_settings();
        }
        // not used, refer to closeEvent of mainFrame
        save_config();
        
        if (confirm_to_store_data()){
            on_save();
            return false; 
        } 
        return true;
    }

    void MainWindow::on_protocol(bool visible)
    {
        _sidebar_widget->showTab(dock::SideBar::TabDecodes, visible);

        if (!visible)
            _view->setFocus();
    }

    void MainWindow::on_trigger(bool visible)
    {
        bool isDso = (_device_agent->get_work_mode() == DSO);
        _sidebar_widget->setDsoMode(isDso);
        _sidebar_widget->showTab(dock::SideBar::TabTrigger, visible);

        if (!visible)
            _view->setFocus();
    }

    void MainWindow::on_measure(bool visible)
    {
        _sidebar_widget->showTab(dock::SideBar::TabMeasures, visible);

        if (!visible)
            _view->setFocus();
    }

    void MainWindow::on_search(bool visible)
    {
        _sidebar_widget->showTab(dock::SideBar::TabSearch, visible);

        if (!visible)
            _view->setFocus();
    }

    void MainWindow::on_display_options()
    {
        pv::dialogs::ApplicationParamDlg dlg;
        dlg.ShowDlg(this);
    }

    void MainWindow::on_screenShot()
    {
        AppConfig &app = AppConfig::Instance();
        QString default_name = app.userHistory.screenShotPath + "/" + APP_NAME + QDateTime::currentDateTime().toString("-yyMMdd-hhmmss");

        int x = parentWidget()->pos().x();
        int y = parentWidget()->pos().y();
        int w = parentWidget()->frameGeometry().width();
        int h = parentWidget()->frameGeometry().height();

        (void)h;
        (void)w;
        (void)x;
        (void)y;

#ifdef _WIN32 
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QPixmap pixmap = QGuiApplication::primaryScreen()->grabWindow(QApplication::desktop->winId(), x, y, w, h);
    #else
        QPixmap pixmap = QPixmap::grabWidget(parentWidget());
    #endif
#elif __APPLE__ 
        x += MainFrame::Margin;
        y += MainFrame::Margin;
        w -= MainFrame::Margin * 2;
        h -= MainFrame::Margin * 2;

    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QPixmap pixmap = QGuiApplication::primaryScreen()->grabWindow(winId(), x, y, w, h);
    #else
        QDesktopWidget *desktop = QApplication::desktop();
        int curMonitor = desktop->screenNumber(this);
        QPixmap pixmap = QGuiApplication::screens().at(curMonitor)->grabWindow(winId(), x, y, w, h);
    #endif       
#else       
        QPixmap pixmap = QGuiApplication::primaryScreen()->grabWindow(winId());
#endif

        QString format = "png";
        QString fileName = QFileDialog::getSaveFileName(
            this,
            tr("Save As"),
            default_name,
            "png file(*.png);;jpeg file(*.jpeg)",
            &format);

        if (!fileName.isEmpty())
        {
            QStringList list = format.split('.').last().split(')');
            QString suffix = list.first();

            QFileInfo f(fileName);
            if (f.suffix().compare(suffix))
            {
                //tr
                fileName += "." + suffix;
            }

            pixmap.save(fileName, suffix.toLatin1());

            fileName = path::GetDirectoryName(fileName);

            if (app.userHistory.screenShotPath != fileName)
            {
                app.userHistory.screenShotPath = fileName;
                app.SaveHistory();
            }
        }
    }

    // save file
    void MainWindow::on_save()
    {
        using pv::dialogs::StoreProgress;

        if (_device_agent->have_instance() == false)
        {
            dsv_info("Have no device, can't to save data.");
            return;
        }

        if (_session->is_working()){
            dsv_info("Save data: stop the current device."); 
            _session->stop_capture();
        }

        _session->set_saving(true);

        StoreProgress *dlg = new StoreProgress(_session, this);
        dlg->SetView(_view);
        dlg->save_run(this);
    }

    void MainWindow::on_export()
    {
        using pv::dialogs::StoreProgress;

        if (_session->is_working()){
            dsv_info("Export data: stop the current device."); 
            _session->stop_capture();
        }

        StoreProgress *dlg = new StoreProgress(_session, this);
        dlg->SetView(_view);
        dlg->export_run();
    }

    bool MainWindow::on_load_session(QString name)
    {
        return load_config_from_file(name);
    }

    bool MainWindow::load_config_from_file(QString file)
    {
        if (file == ""){
            dsv_err("File name is empty.");
            assert(false);
        }

        _sidebar_widget->protocol_widget()->del_all_protocol();

        std::string file_name = pv::path::ToUnicodePath(file);
        dsv_info("Load device profile: \"%s\"", file_name.c_str());
        
        QFile sf(file);

        if (!sf.exists()){ 
            dsv_warn("Warning: device profile is not exists: \"%s\"", file_name.c_str());
            return false;
        }

        if (!sf.open(QIODevice::ReadOnly))
        {
            dsv_warn("Warning: Couldn't open device profile to load!");
            return false;
        }

        QString data = QString::fromUtf8(sf.readAll());
        QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
        sf.close();

        bool bDecoder = false;
        int ret = load_config_from_json(doc, bDecoder);

        if (ret && _device_agent->get_work_mode() == DSO)
        {
            _sidebar_widget->dso_trigger_widget()->update_view();
        }

        if (_device_agent->is_hardware()){
            _title_ext_string = file;
            update_title_bar_text();
        }

        return ret;
    }

    bool MainWindow::gen_config_json(QJsonObject &sessionVar)
    {
        AppConfig &app = AppConfig::Instance();

        GVariant *gvar_opts;
        GVariant *gvar;
        gsize num_opts;

        QString title = QApplication::applicationName() + " v" + QApplication::applicationVersion();

        QJsonArray channelVar;
        sessionVar["Version"] = QJsonValue::fromVariant(SESSION_FORMAT_VERSION);
        sessionVar["Device"] = QJsonValue::fromVariant(_device_agent->driver_name());
        sessionVar["DeviceMode"] = QJsonValue::fromVariant(_device_agent->get_work_mode());
        sessionVar["Language"] = QJsonValue::fromVariant(app.frameOptions.language);
        sessionVar["Title"] = QJsonValue::fromVariant(title);

        if (_device_agent->is_hardware() && _device_agent->get_work_mode() == LOGIC)
        {
            sessionVar["CollectMode"] = _session->get_collect_mode();
        }

        gvar_opts = _device_agent->get_config_list(NULL, SR_CONF_DEVICE_SESSIONS);
        if (gvar_opts == NULL)
        {
            dsv_warn("Device config list is empty. id:SR_CONF_DEVICE_SESSIONS");
            /* Driver supports no device instance sessions. */
            return false;
        }

        const int *const options = (const int32_t *)g_variant_get_fixed_array(
                                        gvar_opts, &num_opts, sizeof(int32_t));

        for (unsigned int i = 0; i < num_opts; i++)
        {
            const struct sr_config_info *const info = _device_agent->get_config_info(options[i]);
            gvar = _device_agent->get_config(info->key);
            if (gvar != NULL)
            {
                if (info->datatype == SR_T_BOOL)
                    sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_boolean(gvar));
                else if (info->datatype == SR_T_UINT64)
                    sessionVar[info->name] = QJsonValue::fromVariant(QString::number(g_variant_get_uint64(gvar)));
                else if (info->datatype == SR_T_UINT8)
                    sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_byte(gvar));
                 else if (info->datatype == SR_T_INT16)
                    sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_int16(gvar));
                else if (info->datatype == SR_T_FLOAT) //save as string format
                    sessionVar[info->name] = QJsonValue::fromVariant(QString::number(g_variant_get_double(gvar)));
                else if (info->datatype == SR_T_CHAR)
                    sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_string(gvar, NULL));
                else if (info->datatype == SR_T_LIST)
                    sessionVar[info->name] =  QJsonValue::fromVariant(g_variant_get_int16(gvar));
                else{
                    dsv_err("Unkown config info type:%d", info->datatype);
                    assert(false);
                }
                g_variant_unref(gvar);                
            }
        }

        for (auto s : _session->get_signals())
        {
            QJsonObject s_obj;
            s_obj["index"] = s->get_index();
            s_obj["view_index"] = s->get_view_index();
            s_obj["type"] = s->get_type();
            s_obj["enabled"] = s->enabled();
            s_obj["name"] = s->get_name();

            if (s->get_colour().isValid())
                s_obj["colour"] = QJsonValue::fromVariant(s->get_colour());
            else
                s_obj["colour"] = QJsonValue::fromVariant("default");

            view::LogicSignal *logicSig = NULL;
            if ((logicSig = dynamic_cast<view::LogicSignal *>(s)))
            {
                s_obj["strigger"] = logicSig->get_trig();
            }
            
            if (s->signal_type() == SR_CHANNEL_DSO)
            {
                view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                s_obj["vdiv"] = QJsonValue::fromVariant(static_cast<qulonglong>(dsoSig->get_vDialValue()));
                s_obj["vfactor"] = QJsonValue::fromVariant(static_cast<qulonglong>(dsoSig->get_factor()));
                s_obj["coupling"] = dsoSig->get_acCoupling();
                s_obj["trigValue"] = dsoSig->get_trig_vrate();
                s_obj["zeroPos"] = dsoSig->get_zero_ratio();
            }
 
            if (s->signal_type() == SR_CHANNEL_ANALOG)
            {
                view::AnalogSignal *analogSig = (view::AnalogSignal*)s;
                s_obj["vdiv"] = QJsonValue::fromVariant(static_cast<qulonglong>(analogSig->get_vdiv()));
                s_obj["vfactor"] = QJsonValue::fromVariant(static_cast<qulonglong>(analogSig->get_factor()));
                s_obj["coupling"] = analogSig->get_acCoupling();
                s_obj["zeroPos"] = analogSig->get_zero_ratio();
                s_obj["mapUnit"] = analogSig->get_mapUnit();
                s_obj["mapMin"] = analogSig->get_mapMin();
                s_obj["mapMax"] = analogSig->get_mapMax();
                s_obj["mapDefault"] = analogSig->get_mapDefault();
            }

            if (logicSig) {
                int rh = s->get_height_override();
                if (rh > 0)
                    s_obj["row_height"] = rh;
            }
            channelVar.append(s_obj);
        }
        sessionVar["channel"] = channelVar;

        if (_device_agent->get_work_mode() == LOGIC)
        {
            sessionVar["trigger"] = _sidebar_widget->trigger_widget()->get_session();
        }

        StoreSession ss(_session);
        QJsonArray decodeJson;
        ss.gen_decoders_json(decodeJson);
        sessionVar["decoder"] = decodeJson;

        if (_device_agent->get_work_mode() == DSO)
        {
            sessionVar["measure"] = _view->get_viewstatus()->get_session();
        }

        if (gvar_opts != NULL)
            g_variant_unref(gvar_opts);

        return true;
    }

    bool MainWindow::load_config_from_json(QJsonDocument &doc, bool &haveDecoder)
    {
        haveDecoder = false;

        QJsonObject sessionObj = doc.object();

        int mode = _device_agent->get_work_mode();

        // check config file version
        if (!sessionObj.contains("Version"))
        {
            dsv_dbg("Profile version is not exists!");
            return false;
        }

        int format_ver = sessionObj["Version"].toInt();

        if (format_ver < 2)
        {
            dsv_err("Profile version is error!");
            return false;
        }

        if (sessionObj.contains("CollectMode") && _device_agent->is_hardware()){
            int collect_mode = sessionObj["CollectMode"].toInt();
            _session->set_collect_mode((DEVICE_COLLECT_MODE)collect_mode);
        }

        int conf_dev_mode = sessionObj["DeviceMode"].toInt();

        if (_device_agent->is_hardware())
        {
            QString driverName = _device_agent->driver_name();
            QString sessionDevice = sessionObj["Device"].toString();            
            // check device and mode
            if (driverName != sessionDevice || mode != conf_dev_mode)
            {
                MsgBox::Show(NULL, tr("Profile is not compatible with current device or mode!"), this);
                return false;
            }
        }

        // load device settings
        GVariant *gvar_opts = _device_agent->get_config_list(NULL, SR_CONF_DEVICE_SESSIONS);
        gsize num_opts;

        if (gvar_opts != NULL)
        {
            const int *const options = (const int32_t *)g_variant_get_fixed_array(
                gvar_opts, &num_opts, sizeof(int32_t));

            for (unsigned int i = 0; i < num_opts; i++)
            {
                const struct sr_config_info *info = _device_agent->get_config_info(options[i]);

                if (!sessionObj.contains(info->name))
                    continue;

                GVariant *gvar = NULL;
                int id = 0;

                if (info->datatype == SR_T_BOOL){
                    gvar = g_variant_new_boolean(sessionObj[info->name].toInt());
                }
                else if (info->datatype == SR_T_UINT64){
                    //from string text.
                    gvar = g_variant_new_uint64(sessionObj[info->name].toString().toULongLong());         
                }
                else if (info->datatype == SR_T_UINT8){
                    if (sessionObj[info->name].toString() != "")
                        gvar = g_variant_new_byte(sessionObj[info->name].toString().toUInt());
                    else
                        gvar = g_variant_new_byte(sessionObj[info->name].toInt());                       
                }
                else if (info->datatype == SR_T_INT16){
                    gvar = g_variant_new_int16(sessionObj[info->name].toInt());
                }
                else if (info->datatype == SR_T_FLOAT){
                    if (sessionObj[info->name].toString() != "")
                        gvar = g_variant_new_double(sessionObj[info->name].toString().toDouble());
                    else
                        gvar = g_variant_new_double(sessionObj[info->name].toDouble()); 
                }
                else if (info->datatype == SR_T_CHAR){
                    gvar = g_variant_new_string(sessionObj[info->name].toString().toLocal8Bit().data());
                }
                else if (info->datatype == SR_T_LIST)
                { 
                    id = 0;

                    if (format_ver > 2){
                        // Is new version format.
                        id = sessionObj[info->name].toInt();
                    }
                    else{
                        const char *fd_key = sessionObj[info->name].toString().toLocal8Bit().data();
                        id = ds_dsl_option_value_to_code(conf_dev_mode, info->key, fd_key);
                        if (id == -1){
                            dsv_err("Convert failed, key:\"%s\", value:\"%s\""
                                ,info->name, fd_key);
                            id = 0; //set default value.
                        }
                        else{
                            dsv_info("Convert success, key:\"%s\", value:\"%s\", get code:%d"
                                ,info->name, fd_key, id);
                        }
                    }            
                    gvar = g_variant_new_int16(id);
                }

                if (gvar == NULL)
                {
                    dsv_warn("Warning: Profile failed to parse key:'%s'", info->name);
                    continue;
                }

                bool bFlag = _device_agent->set_config(info->key, gvar);
                if (!bFlag){
                    dsv_err("Set device config option failed, id:%d, code:%d", info->key, id);
                }   
            }
        }

        // load channel settings
        if (mode == DSO)
        {
            for (const GSList *l = _device_agent->get_channels(); l; l = l->next)
            {
                sr_channel *const probe = (sr_channel *)l->data;
                assert(probe);

                for (const QJsonValue &value : sessionObj["channel"].toArray())
                {
                    QJsonObject obj = value.toObject();
                    if (QString(probe->name) == obj["name"].toString() &&
                        probe->type == obj["type"].toDouble())
                    {
                        probe->vdiv = obj["vdiv"].toDouble();
                        probe->coupling = obj["coupling"].toDouble();
                        probe->vfactor = obj["vfactor"].toDouble();
                        probe->trig_value = obj["trigValue"].toDouble();
                        probe->map_unit = g_strdup(obj["mapUnit"].toString().toStdString().c_str());
                        probe->map_min = obj["mapMin"].toDouble();
                        probe->map_max = obj["mapMax"].toDouble();
                        probe->enabled = obj["enabled"].toBool();
                        break;
                    }
                }
            }
        }
        else
        {
            for (const GSList *l = _device_agent->get_channels(); l; l = l->next)
            {
                sr_channel *const probe = (sr_channel *)l->data;
                assert(probe);
                bool isEnabled = false;

                for (const QJsonValue &value : sessionObj["channel"].toArray())
                {
                    QJsonObject obj = value.toObject();

                    if ((probe->index == obj["index"].toInt()) &&
                        (probe->type == obj["type"].toInt()))
                    {
                        isEnabled = true;
                        QString chan_name = obj["name"].toString().trimmed();
                        if (chan_name == ""){
                            chan_name = QString::number(probe->index);
                        }
                        
                        probe->enabled = obj["enabled"].toBool();
                        probe->name = g_strdup(chan_name.toStdString().c_str());
                        probe->vdiv = obj["vdiv"].toDouble();
                        probe->coupling = obj["coupling"].toDouble();
                        probe->vfactor = obj["vfactor"].toDouble();
                        probe->trig_value = obj["trigValue"].toDouble();
                        probe->map_unit = g_strdup(obj["mapUnit"].toString().toStdString().c_str());
                        probe->map_min = obj["mapMin"].toDouble();
                        probe->map_max = obj["mapMax"].toDouble();

                        if (obj.contains("mapDefault"))
                        {
                            probe->map_default = obj["mapDefault"].toBool();
                        }

                        break;
                    }
                }
                if (!isEnabled)
                    probe->enabled = false;
            }
        }

        _session->reload();

        // load signal setting
        if (mode == DSO)
        {
            for (auto s : _session->get_signals())
            {
                for (const QJsonValue &value : sessionObj["channel"].toArray())
                {
                    QJsonObject obj = value.toObject();

                    if (s->get_name() ==  obj["name"].toString() &&
                        s->get_type() ==  obj["type"].toDouble())
                    {
                        QColor loadedColour(obj["colour"].toString());
                        if (loadedColour.isValid())
                            s->set_colour(loadedColour);
                       
                        if (s->signal_type() == SR_CHANNEL_DSO)
                        {   
                            view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                            dsoSig->load_settings();
                            dsoSig->set_zero_ratio(obj["zeroPos"].toDouble());
                            dsoSig->set_trig_ratio(obj["trigValue"].toDouble());
                            dsoSig->commit_settings();
                        }
                        break;
                    }
                }
            }
        }
        else
        {
            for (auto s : _session->get_signals())
            {
                for (const QJsonValue &value : sessionObj["channel"].toArray())
                {
                    QJsonObject obj = value.toObject();
                    if ((s->get_index() == obj["index"].toInt()) &&
                        (s->get_type() == obj["type"].toInt()))
                    {
                        QString chan_name = obj["name"].toString().trimmed();
                        if (chan_name == ""){
                            chan_name = QString::number(s->get_index());
                        }

                        QColor loadedColour(obj["colour"].toString());
                        if (loadedColour.isValid())
                            s->set_colour(loadedColour);
                        s->set_name(chan_name);

                        view::LogicSignal *logicSig = NULL;
                        if ((logicSig = dynamic_cast<view::LogicSignal *>(s)))
                        {
                            logicSig->set_trig(obj["strigger"].toDouble());
                        }
 
                        if (s->signal_type() == SR_CHANNEL_DSO)
                        {
                            view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                            dsoSig->load_settings();
                            dsoSig->set_zero_ratio(obj["zeroPos"].toDouble());
                            dsoSig->set_trig_ratio(obj["trigValue"].toDouble());
                            dsoSig->commit_settings();
                        }
 
                        if (s->signal_type() == SR_CHANNEL_ANALOG)
                        {   
                            view::AnalogSignal *analogSig = (view::AnalogSignal*)s;
                            analogSig->set_zero_ratio(obj["zeroPos"].toDouble());
                            analogSig->commit_settings();
                        }

                        if (logicSig && obj.contains("row_height")) {
                            int rh = obj["row_height"].toInt();
                            if (rh > 0)
                                s->set_height_override(rh);
                        }
                        
                        break;
                    }
                }
            }
        }

        // update UI settings
        _sampling_bar->update_sample_rate_list();
        _sidebar_widget->trigger_widget()->device_updated();
        _view->header_updated();

        // load trigger settings
        if (sessionObj.contains("trigger"))
        {
            _sidebar_widget->trigger_widget()->set_session(sessionObj["trigger"].toObject());
        }

        // load decoders
        if (sessionObj.contains("decoder"))
        {
            QJsonArray deArray = sessionObj["decoder"].toArray();
            if (deArray.empty() == false)
            {
                haveDecoder = true;
                StoreSession ss(_session);
                ss.load_decoders(_sidebar_widget->protocol_widget(), deArray);
                _view->update_all_trace_postion();
            }
        }

        // load measure
        if (sessionObj.contains("measure"))
        {
            auto *bottom_bar = _view->get_viewstatus();
            bottom_bar->load_session(sessionObj["measure"].toArray(), format_ver);
        }

        if (gvar_opts != NULL)
            g_variant_unref(gvar_opts);

        load_channel_view_indexs(doc);

        return true;
    }

    void MainWindow::load_channel_view_indexs(QJsonDocument &doc)
    {
        QJsonObject sessionObj = doc.object();

        int mode = _device_agent->get_work_mode();
        if (mode != LOGIC)
            return;

        std::vector<int> view_indexs;

        for (const QJsonValue &value : sessionObj["channel"].toArray()){
            QJsonObject obj = value.toObject();

            if (obj.contains("view_index")){  
                view_indexs.push_back(obj["view_index"].toInt());
            }
        }

         if (view_indexs.size()){
            int i = 0;

            for (auto s : _session->get_signals()){
                s->set_view_index(view_indexs[i]);
                i++;
            }

            _view->update_all_trace_postion();
        }
    }
    
    bool MainWindow::on_store_session(QString name)
    {
        return save_config_to_file(name);
    }

    bool MainWindow::save_config_to_file(QString name)
    {
        if (name == ""){
            dsv_err("Session file name is empty.");
            assert(false);
        }

        std::string file_name = pv::path::ToUnicodePath(name);
        dsv_info("Store session to file: \"%s\"", file_name.c_str());

        QFile sf(name);
        if (!sf.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            dsv_warn("Warning: Couldn't open profile to write!");
            return false;
        }

        QTextStream outStream(&sf);
        encoding::set_utf8(outStream);

        QJsonObject sessionVar;
        if (!gen_config_json(sessionVar)){
            return false;
        }

        QJsonDocument sessionDoc(sessionVar);
        outStream << QString::fromUtf8(sessionDoc.toJson());
        sf.close();
        return true;
    }

    bool MainWindow::genSessionData(std::string &str)
    {
        QJsonObject sessionVar;
        if (!gen_config_json(sessionVar))
        {
            return false;
        }

        QJsonDocument sessionDoc(sessionVar);
        QString data = QString::fromUtf8(sessionDoc.toJson());
        str.append(data.toLocal8Bit().data());
        return true;
    }

    void MainWindow::restore_dock()
    {
        // default dockwidget size
        AppConfig &app = AppConfig::Instance();
        QByteArray st = app.frameOptions.windowState;
        if (!st.isEmpty())
        {
            // Skip restoring state from before the unified sidebar was introduced —
            // old state mentions the removed individual docks and would hide sidebar_dock.
            if (st.contains("sidebar_dock"))
            {
                try
                {
                    restoreState(st);
                }
                catch (...)
                {
                    MsgBox::Show(NULL, tr("restore window status error!"));
                }
            }
            else
            {
                // Stale state from old layout — discard it
                app.frameOptions.windowState.clear();
            }
        }

        // Ensure the sidebar dock is always visible regardless of saved state
        _sidebar_dock->setVisible(true);

        // The panel starts closed; collapse the dock to icon-strip width so
        // the icon strip is flush with the right window edge even if the
        // saved window state recorded a wider dock (panel was open when saved).
        resizeDocks({_sidebar_dock}, {dock::SideBar::kIconStripWidth}, Qt::Horizontal);

        // Resotre the dock pannel.
        if (_device_agent->have_instance())
            _trig_bar->reload();
    }

    // -----------------------------------------------------------------------
    // Keyboard shortcut setup
    // -----------------------------------------------------------------------

    void MainWindow::setup_shortcuts()
    {
        rebuild_shortcuts();
    }

    void MainWindow::rebuild_shortcuts()
    {
        for (auto *sc : _shortcuts)
            delete sc;
        _shortcuts.clear();

        auto &opts = AppConfig::Instance().shortcutOptions;

        auto add = [&](const QString &seq, auto slot) {
            if (seq.isEmpty()) return;
            auto *sc = new QShortcut(QKeySequence(seq), this);
            sc->setContext(Qt::ApplicationShortcut);
            connect(sc, &QShortcut::activated, this, slot);
            _shortcuts.append(sc);
        };

        add(opts.startCollecting,   &MainWindow::sc_start_collecting);
        add(opts.stopCollecting,    &MainWindow::sc_stop_collecting);
        add(opts.switchVernierUp,   &MainWindow::sc_vernier_prev);
        add(opts.switchVernierDown, &MainWindow::sc_vernier_next);
        add(opts.parameterMeasure,  &MainWindow::sc_measure);
        add(opts.vernierCreate,     &MainWindow::sc_vernier_create);
        add(opts.switchPageUp,      &MainWindow::sc_page_up);
        add(opts.switchPageDown,    &MainWindow::sc_page_down);
        add(opts.jumpZero,          &MainWindow::sc_jump_zero);
        add(opts.zoomIn,            &MainWindow::sc_zoom_in);
        add(opts.zoomOut,           &MainWindow::sc_zoom_out);
        add(opts.zoomFull,          &MainWindow::sc_zoom_full);
        add(opts.saveFile,          &MainWindow::sc_save);
        add(opts.saveAs,            &MainWindow::sc_save_as);
        add(opts.exportFile,        &MainWindow::sc_export);
        add(opts.deviceConfig,      &MainWindow::sc_device_config);
        add(opts.protocolDecode,    &MainWindow::sc_protocol_decode);
        add(opts.labelMeasurement,  &MainWindow::sc_label_measurement);
        add(opts.dataSearch,        &MainWindow::sc_data_search);
        add(opts.closeSession,      &MainWindow::sc_close_session);

        // Delete key always removes the active cursor
        auto *delSc = new QShortcut(QKeySequence(Qt::Key_Delete), this);
        delSc->setContext(Qt::ApplicationShortcut);
        connect(delSc, &QShortcut::activated, this, &MainWindow::sc_delete_cursor);
        _shortcuts.append(delSc);
    }

    // -----------------------------------------------------------------------
    // Shortcut slot implementations
    // -----------------------------------------------------------------------

    void MainWindow::sc_start_collecting()
    {
        if (!_session->is_working())
            _sampling_bar->run_or_stop();
    }

    void MainWindow::sc_stop_collecting()
    {
        if (_session->is_working())
            _sampling_bar->run_or_stop();
    }

    void MainWindow::sc_vernier_prev()
    {
        _view->nav_cursor(-1);
    }

    void MainWindow::sc_vernier_next()
    {
        _view->nav_cursor(1);
    }

    void MainWindow::sc_measure()
    {
        _trig_bar->measure_clicked();
    }

    void MainWindow::sc_vernier_create()
    {
        int64_t center = _view->offset() + _view->get_view_width() / 2;
        uint64_t sampleIndex = (uint64_t)(center * _view->scale() * _session->cur_snap_samplerate());
        _view->add_cursor(sampleIndex);
        _view->show_cursors(true);
    }

    void MainWindow::sc_page_up()
    {
        if (_active_tab_index > 0)
            switch_to_session(_active_tab_index - 1);
    }

    void MainWindow::sc_page_down()
    {
        if (_active_tab_index < _session_items.size() - 1)
            switch_to_session(_active_tab_index + 1);
    }

    void MainWindow::sc_jump_zero()
    {
        _view->set_scale_offset(_view->scale(), 0);
    }

    void MainWindow::sc_zoom_in()
    {
        _view->zoom(1);
    }

    void MainWindow::sc_zoom_out()
    {
        _view->zoom(-1);
    }

    void MainWindow::sc_zoom_full()
    {
        _view->zoom_fit();
    }

    void MainWindow::sc_save()
    {
        on_save();
    }

    void MainWindow::sc_save_as()
    {
        on_save();
    }

    void MainWindow::sc_export()
    {
        on_export();
    }

    void MainWindow::sc_device_config()
    {
        _sampling_bar->config_device();
    }

    void MainWindow::sc_protocol_decode()
    {
        _trig_bar->protocol_clicked();
    }

    void MainWindow::sc_label_measurement()
    {
        _trig_bar->measure_clicked();
    }

    void MainWindow::sc_data_search()
    {
        _trig_bar->search_clicked();
    }

    void MainWindow::sc_close_session()
    {
        on_session_tab_close(_active_tab_index);
    }

    void MainWindow::sc_delete_cursor()
    {
        _view->delete_active_cursor();
    }

    void MainWindow::on_shortcut_settings()
    {
        pv::dialogs::ShortcutDlg dlg(this);
        dlg.exec();
        if (dlg.IsClickYes())
            rebuild_shortcuts();
    }

    bool MainWindow::eventFilter(QObject *object, QEvent *event)
    {
        (void)object;
    
        if (event->type() == QEvent::KeyPress)
        {
            const auto &sigs = _session->get_signals();
            QKeyEvent *ke = (QKeyEvent *)event;
            
            int modifier = ke->modifiers();

            // Pass through modifier combos — QShortcut handles them.
            // Only handle bare keys (no modifiers) in this eventFilter.
            if (modifier & Qt::ControlModifier ||
                modifier & Qt::ShiftModifier  ||
                modifier & Qt::AltModifier)
            {
                return false;
            }

            high_resolution_clock::time_point key_press_time = high_resolution_clock::now();
            milliseconds timeInterval = std::chrono::duration_cast<milliseconds>(key_press_time - _last_key_press_time);
            int64_t time_keep =  timeInterval.count();
            if (time_keep < 200){
                return true;
            }
            _last_key_press_time = key_press_time;           
            
            switch (ke->key())
            {
            case Qt::Key_S:
                _sampling_bar->run_or_stop();
                break;

            case Qt::Key_I:
                _sampling_bar->run_or_stop_instant();
                break;

            case Qt::Key_T:
                _trig_bar->trigger_clicked();
                break;

            case Qt::Key_D:
                _trig_bar->protocol_clicked();
                break;

            case Qt::Key_M:
                _trig_bar->measure_clicked();
                break;

            case Qt::Key_R:
                _trig_bar->search_clicked();
                break;

            case Qt::Key_O:
                _sampling_bar->config_device();
                break;

            case Qt::Key_PageUp:
                _view->set_scale_offset(_view->scale(),
                                        _view->offset() - _view->get_view_width());
                break;
            case Qt::Key_PageDown:
                _view->set_scale_offset(_view->scale(),
                                        _view->offset() + _view->get_view_width());

                break;

            case Qt::Key_Left:
                _view->zoom(1);
                break;

            case Qt::Key_Right:
                _view->zoom(-1);
                break;

            case Qt::Key_0:
                for (auto s : sigs)
                {
                    if (s->signal_type() == SR_CHANNEL_DSO)
                    {
                        view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                        if (dsoSig->get_index() == 0)
                            dsoSig->set_vDialActive(!dsoSig->get_vDialActive());
                        else
                            dsoSig->set_vDialActive(false);
                    }
                }
                _view->setFocus();
                update();
                break;

            case Qt::Key_1:
                for (auto s : sigs)
                {
                    if (s->signal_type() == SR_CHANNEL_DSO)
                    {
                        view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                        if (dsoSig->get_index() == 1)
                            dsoSig->set_vDialActive(!dsoSig->get_vDialActive());
                        else
                            dsoSig->set_vDialActive(false);
                    }
                }
                _view->setFocus();
                update();
                break;

            case Qt::Key_Up: 
                for (auto s : sigs)
                {
                    if (s->signal_type() == SR_CHANNEL_DSO){
                        view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                        if (dsoSig->get_vDialActive())
                        {
                            dsoSig->go_vDialNext(true);
                            update();
                            break;
                        }
                    }
                }
                break;

            case Qt::Key_Down:
                for (auto s : sigs)
                {
                    if (s->signal_type() == SR_CHANNEL_DSO){
                        view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                        if (dsoSig->get_vDialActive())
                        {
                            dsoSig->go_vDialPre(true);
                            update();
                            break;
                        }
                    }
                }
                break;
           
            default:
                QWidget::keyPressEvent((QKeyEvent *)event);
            }
            return true;
        }
        return false;
    }

    void MainWindow::switchLanguage(int language)
    {
        if (language == 0)
            return;

        AppConfig &app = AppConfig::Instance();
        if (app.frameOptions.language != language && language > 0) {
            app.frameOptions.language = language;
            app.SaveFrame();
        }

        qApp->removeTranslator(&_appTrans);
        qApp->removeTranslator(&_qtTrans);

        if (language == LAN_CN) {
            if (_appTrans.load(":/zh_CN"))
                qApp->installTranslator(&_appTrans);
            if (_qtTrans.load(":/qt_" + QString::number(language)))
                qApp->installTranslator(&_qtTrans);
        }

        retranslateUi();
        UiManager::Instance()->Update(UI_UPDATE_ACTION_LANG);
        _session->update_lang_text();
    }

    void MainWindow::switchTheme(QString style)
    {
        AppConfig &app = AppConfig::Instance();

        if (app.frameOptions.style != style)
        {
            app.frameOptions.style = style;
            app.SaveFrame();
        }

        QString qssRes = ":/" + style + ".qss";
        QFile qss(qssRes);
        qss.open(QFile::ReadOnly | QFile::Text);
        qApp->setStyleSheet(qss.readAll());
        qss.close();

        UiManager::Instance()->Update(UI_UPDATE_ACTION_THEME);
        UiManager::Instance()->Update(UI_UPDATE_ACTION_FONT);

        // Refresh tab bar colors when theme changes
        if (_session_tab_bar)
            rebuild_tab_buttons();

        data_updated();
    }

    void MainWindow::data_updated()
    {
        _event.data_updated(); // safe call
    }

    void MainWindow::on_data_updated()
    {
        _sidebar_widget->measure_widget()->reCalc();
        _view->data_updated();
    }

    void MainWindow::on_open_doc()
    {
        openDoc();
    }

    void MainWindow::openDoc()
    {
        QDir dir(GetAppDataDir());
        AppConfig &app = AppConfig::Instance();
        int lan = app.frameOptions.language;
        QDesktopServices::openUrl(
            QUrl("file:///" + dir.absolutePath() + "/ug" + QString::number(lan) + ".pdf"));
    }

    void MainWindow::update_capture()
    {
        _view->update_hori_res();
    }

    void MainWindow::cur_snap_samplerate_changed()
    {
        _event.cur_snap_samplerate_changed(); // safe call
    }

    void MainWindow::on_cur_snap_samplerate_changed()
    {
        _sidebar_widget->measure_widget()->reCalc();
    }

    /*------------------on event end-------*/

    void MainWindow::signals_changed()
    {
        _event.signals_changed(); // safe call
    }

    void MainWindow::on_signals_changed()
    {
        _view->signals_changed(NULL);
    }

    void MainWindow::receive_trigger(quint64 trigger_pos)
    {
        _event.receive_trigger(trigger_pos); // save call
    }

    void MainWindow::on_receive_trigger(quint64 trigger_pos)
    {
        _view->receive_trigger(trigger_pos);
    }

    void MainWindow::frame_ended()
    {
        _event.frame_ended(); // save call
    }

    void MainWindow::on_frame_ended()
    {
        _view->receive_end();
    }

    void MainWindow::frame_began()
    {
        _event.frame_began(); // save call
    }

    void MainWindow::on_frame_began()
    {
        _view->frame_began();
    }

    void MainWindow::show_region(uint64_t start, uint64_t end, bool keep)
    {
        _view->show_region(start, end, keep);
    }

    void MainWindow::show_wait_trigger()
    {
        _view->show_wait_trigger();
    }

    void MainWindow::repeat_hold(int percent)
    {
        (void)percent;
        _view->repeat_show();
    }

    void MainWindow::decode_done()
    {
        _event.decode_done(); // safe call
    }

    void MainWindow::on_decode_done()
    {
        _sidebar_widget->protocol_widget()->update_model();
    }

    void MainWindow::receive_data_len(quint64 len)
    {
        _event.receive_data_len(len); // safe call
    }

    void MainWindow::on_receive_data_len(quint64 len)
    {
        _view->set_receive_len(len);
    }

    void MainWindow::receive_header()
    {
    }

    void MainWindow::check_usb_device_speed()
    {
        // USB device speed check
        if (_device_agent->is_hardware())
        {
            int usb_speed = LIBUSB_SPEED_HIGH;
            _device_agent->get_config_int32(SR_CONF_USB_SPEED, usb_speed);

            bool usb30_support = false;

            if (_device_agent->get_config_bool(SR_CONF_USB30_SUPPORT, usb30_support))
            {
                dsv_info("The device's USB module version: %d.0", usb30_support ? 3 : 2);

                int cable_ver = 1;
                if (usb_speed == LIBUSB_SPEED_HIGH)
                    cable_ver = 2;
                else if (usb_speed == LIBUSB_SPEED_SUPER)
                    cable_ver = 3;

                dsv_info("The cable's USB port version: %d.0", cable_ver);

                if (usb30_support && usb_speed == LIBUSB_SPEED_HIGH){
                    QString str_err(tr("Plug the device into a USB 2.0 port will seriously affect its performance.\nPlease replug it into a USB 3.0 port."));
                    delay_prop_msg(str_err);
                }
            }
        }
    }

    void MainWindow::trigger_message(int msg)
    {
        _event.trigger_message(msg);
    }

    void MainWindow::on_trigger_message(int msg)
    {
        _session->broadcast_msg(msg);
    }

    void MainWindow::reset_all_view()
    {
        _sampling_bar->reload();
        _view->status_clear();
        _view->reload();
        _view->set_device();
        _sidebar_widget->trigger_widget()->update_view();
        _sidebar_widget->trigger_widget()->device_updated();
        _trig_bar->reload();
        _sidebar_widget->dso_trigger_widget()->update_view();
        _sidebar_widget->measure_widget()->reload();

        if (_device_agent->get_work_mode() == ANALOG)
            _view->get_viewstatus()->setVisible(false);
        else
            _view->get_viewstatus()->setVisible(true);
    }

    bool MainWindow::confirm_to_store_data()
    {   
        bool ret = false;
        _is_save_confirm_msg = true;       

        if (_session->have_hardware_data() && _session->is_first_store_confirm())
        {   
            // Only popup one time.
            ret =  MsgBox::Confirm(tr("Save captured data?"));

            if (!ret && _is_auto_switch_device)
            {
                dsv_info("The data save confirm end, auto switch to the new device.");
                _is_auto_switch_device = false;

                if (_session->is_working())
                    _session->stop_capture();

                _session->set_default_device();
            }
        }

        _is_save_confirm_msg = false;
        return ret;
    }

    void MainWindow::check_config_file_version()
    {
        auto device_agent = _session->get_device();
        if (device_agent->is_file() && device_agent->is_new_device())
        {
            if (device_agent->get_work_mode() == LOGIC)
            {   
                int version = -1; 
                if (device_agent->get_config_int16(SR_CONF_FILE_VERSION, version))
                {
                    if (version == 1)
                    {
                        QString strMsg(tr("Current loading file has an old format. \nThis will lead to a slow loading speed. \nPlease resave it after loaded."));
                        MsgBox::Show(strMsg);
                    }
                }
            }
        }
    }

    void MainWindow::load_device_config()
    {   
        _title_ext_string = "";        
        int mode = _device_agent->get_work_mode();
        QString file;

        if (_device_agent->is_hardware())
        { 
            QString ses_name = gen_config_file_path(true);

            bool bExist = false;

            QFile sf(ses_name);
            if (!sf.exists()){
                dsv_info("Try to load the low version profile.");
                ses_name =  gen_config_file_path(false);
            }
            else{
                bExist = true;
            }

            if (!bExist)
            {
                QFile sf2(ses_name);
                if (!sf2.exists()){
                    dsv_info("Try to load the default profile.");
                    ses_name = _file_bar->genDefaultSessionFile();
                }
            } 

            file =  ses_name;
        }
        else if (_device_agent->is_demo())
        {
            QDir dir(GetFirmwareDir());
            if (dir.exists())
            {
                QString ses_name = dir.absolutePath() + "/" 
                            + _device_agent->driver_name() + QString::number(mode) + ".dsc";

                QFile sf(ses_name);
                if (sf.exists()){
                    file = ses_name;
                }
            }
        }

        if (file != ""){
            bool ret = load_config_from_file(file);
            if (ret && _device_agent->is_hardware()){
                _title_ext_string = file;
            }
        }
    }

    QJsonDocument MainWindow::get_config_json_from_data_file(QString file, bool &bSucesss)
    {
        QJsonDocument sessionDoc;
        QJsonParseError error;
        bSucesss = false;

        if (file == ""){
            dsv_err("File name is empty.");
            assert(false);
        }

        auto f_name = pv::path::ConvertPath(file);
        ZipReader rd(f_name.c_str());
        auto *data = rd.GetInnterFileData("session");

        if (data != NULL)
        {
            QByteArray raw_bytes = QByteArray::fromRawData(data->data(), data->size());
            QString jsonStr(raw_bytes.data());
            QByteArray qbs = jsonStr.toUtf8();
            sessionDoc = QJsonDocument::fromJson(qbs, &error);

            if (error.error != QJsonParseError::NoError)
            {
                QString estr = error.errorString();
                dsv_err("File::get_session(), parse json error:\"%s\"!", estr.toUtf8().data());
            }
            else{
                bSucesss = true;
            }

            rd.ReleaseInnerFileData(data);
        }

        return sessionDoc;
    }

    QJsonArray MainWindow::get_decoder_json_from_data_file(QString file, bool &bSucesss)
    {
        QJsonArray dec_array;
        QJsonParseError error;

        bSucesss = false;

        if (file == ""){
            dsv_err("File name is empty.");
            assert(false);
        }

        /* read "decoders" */
        auto f_name = path::ConvertPath(file);
        ZipReader rd(f_name.c_str());
        auto *data = rd.GetInnterFileData("decoders");

        if (data != NULL)
        {
            QByteArray raw_bytes = QByteArray::fromRawData(data->data(), data->size());
            QString jsonStr(raw_bytes.data());
            QByteArray qbs = jsonStr.toUtf8();
            QJsonDocument sessionDoc = QJsonDocument::fromJson(qbs, &error);

            if (error.error != QJsonParseError::NoError)
            {
                QString estr = error.errorString();
                dsv_err("MainWindow::get_decoder_json_from_file(), parse json error:\"%s\"!", estr.toUtf8().data());
            }
            else{
                bSucesss = true;
            }

            dec_array = sessionDoc.array();
            rd.ReleaseInnerFileData(data);
        }

        return dec_array;
    }

    void MainWindow::update_toolbar_view_status()
    {
        _sampling_bar->update_view_status();
        _file_bar->update_view_status();
        _trig_bar->update_view_status();
    }

    void MainWindow::OnMessage(int msg)
    {
        switch (msg)
        {
            case DSV_MSG_DEVICE_LIST_UPDATED:
            {
                _sampling_bar->update_device_list();
                break;
            }
            case DSV_MSG_START_COLLECT_WORK_PREV:
            {
                if (_device_agent->get_work_mode() == LOGIC)
                    _sidebar_widget->trigger_widget()->try_commit_trigger();
                else if (_device_agent->get_work_mode() == DSO)
                    _sidebar_widget->dso_trigger_widget()->check_setting();

                _view->capture_init();
                _view->on_state_changed(false);
                break;
            }
            case DSV_MSG_START_COLLECT_WORK:
            {
                update_toolbar_view_status();
                _view->on_state_changed(false);
                _sidebar_widget->protocol_widget()->update_view_status();
                break;
            }        
            case DSV_MSG_COLLECT_END:
            {
                prgRate(0);
                _view->repeat_unshow();
                _view->on_state_changed(true);
                break;
            }
            case DSV_MSG_END_COLLECT_WORK:
            {
                update_toolbar_view_status();
                _sidebar_widget->protocol_widget()->update_view_status();   
                break;
            }
            case DSV_MSG_CURRENT_DEVICE_CHANGE_PREV:
            {
                if (_msg != NULL){
                    _msg->close();
                    _msg = NULL;
                }
                _view->hide_calibration();

                _sidebar_widget->protocol_widget()->del_all_protocol();
                _view->reload();
                break;
            }
            case DSV_MSG_CURRENT_DEVICE_CHANGED:
            {
                // Note: rebind_device() calls ds_active_device() which fires
                // NO events, so this handler is only reached from set_device()
                // (first-time device init / device selection change), never
                // from a tab switch.

                // Determine whether this message is for the primary device session
                // (tab 0) or an additional session tab.
                bool isDeviceTab = (_session_items.isEmpty() ||
                                    _session == _session_items[0].session);

                reset_all_view();
                load_device_config();
                update_title_bar_text();
                _sampling_bar->update_device_list();

                // Update device tab name only when the device session changes
                if (isDeviceTab && !_tabs.isEmpty()) {
                    QString devName;
                    if (_device_agent->is_demo())
                        devName = tr("Session 1");
                    else
                        devName = _device_agent->driver_name();
                    if (devName.isEmpty()) devName = tr("Device");
                    _tabs[0].name = devName;
                    if (_session_tab_bar)
                        rebuild_tab_buttons();
                }
                
                // Persist the device handle so we can rebind on tab switch-back
                if (_active_tab_index < _session_items.size())
                    _session_items[_active_tab_index].saved_handle =
                        _device_agent->handle();

                _logo_bar->dsl_connected(_session->get_device()->is_hardware());
                update_toolbar_view_status();
                _sidebar_widget->refresh_device_options();
                _session->device_event_object()->device_updated();

                if (_device_agent->is_hardware())
                {
                    _session->on_load_config_end();
                }                
                
                if (_device_agent->get_work_mode() == LOGIC && _device_agent->is_file() == false)
                    _view->auto_set_max_scale();

                if (_device_agent->is_file())
                {
                    check_config_file_version();

                    bool bDoneDecoder = false;
                    bool bLoadSuccess = false;
                    QJsonDocument doc = get_config_json_from_data_file(_device_agent->path(), bLoadSuccess);

                    if (bLoadSuccess){
                        load_config_from_json(doc, bDoneDecoder);
                    }

                    if (isDeviceTab && !bDoneDecoder && _device_agent->get_work_mode() == LOGIC)
                    {                    
                        QJsonArray deArray = get_decoder_json_from_data_file(_device_agent->path(), bLoadSuccess);

                        if (bLoadSuccess){
                            StoreSession ss(_session);
                            ss.load_decoders(_sidebar_widget->protocol_widget(), deArray);
                        }
                    }

                    _view->update_all_trace_postion();                
                    QTimer::singleShot(100, this, [this](){
                        _session->start_capture(true);
                    });
                }
                else if (_device_agent->is_demo() && isDeviceTab)
                {
                    // Demo decoder config is only loaded for the main device session;
                    // additional session tabs start with a clean protocol dock.
                    if(_device_agent->get_work_mode() == LOGIC)
                    {
                        _pattern_mode = _device_agent->get_demo_operation_mode();
                        _sidebar_widget->protocol_widget()->del_all_protocol();
                        _view->auto_set_max_scale();

                        if(_pattern_mode != "random"){
                        load_demo_decoder_config(_pattern_mode);
                        }
                    }
                }
        
                calc_min_height();

                if (_device_agent->is_hardware() && _device_agent->is_new_device()){
                    check_usb_device_speed();
                }

                break;
            }
            case DSV_MSG_DEVICE_OPTIONS_UPDATED:
            {
                _sidebar_widget->trigger_widget()->device_updated();
                _sidebar_widget->measure_widget()->reload();
                _view->check_calibration();                      
                break;
            }
            case DSV_MSG_DEVICE_DURATION_UPDATED:
            {
                _sidebar_widget->trigger_widget()->device_updated();
                _view->timebase_changed();
                break;
            }
            case DSV_MSG_DEVICE_MODE_CHANGED:
            {
                bool isDeviceTab = (_session_items.isEmpty() ||
                                    _session == _session_items[0].session);
                _view->mode_changed(); 
                reset_all_view();
                load_device_config();
                update_title_bar_text();
                _view->hide_calibration();

                update_toolbar_view_status();
                _sampling_bar->update_sample_rate_list();

                if (_device_agent->is_hardware())
                    _session->on_load_config_end();
                
                if (_device_agent->get_work_mode() == LOGIC)
                    _view->auto_set_max_scale();

                if(_device_agent->is_demo() && isDeviceTab)
                {
                    _pattern_mode = _device_agent->get_demo_operation_mode();
                    _sidebar_widget->protocol_widget()->del_all_protocol();

                    if(_device_agent->get_work_mode() == LOGIC)
                    {
                        if(_pattern_mode != "random"){
                            _device_agent->update();
                            load_demo_decoder_config(_pattern_mode);
                        }
                    }
                }

                calc_min_height();           
                break;
            }
            case DSV_MSG_NEW_USB_DEVICE:
            {
                if (_msg != NULL){
                    _msg->close();
                    _msg = NULL;
                }

                _sampling_bar->update_device_list();

                //If the current device is working, do not remind to switch new device.
                if (_session->get_device()->is_hardware() && _session->is_working()){
                    return;
                }

                // If a saving task is running, not need to remind to switch device, 
                // when the task end, the new device will be selected.
                if (_session->get_device()->is_demo() == false && !_is_save_confirm_msg)
                {
                    QString msgText = tr("To switch the new device?");
                    
                    if (MsgBox::Confirm(msgText, "", &_msg, NULL) == false){ 
                        _msg = NULL;
                        return;
                    }
                    _msg = NULL;
                }

                // The store confirm is not processed.
                if (_is_save_confirm_msg){
                    dsv_info("New device attached:Waitting for the confirm box be closed.");
                    _is_auto_switch_device = true; 
                    return;
                }

                if (_session->is_saving()){
                    dsv_info("New device attached:Waitting for store the data. and will switch to new device.");
                    _is_auto_switch_device = true;
                    return;
                }

                int mode = _device_agent->get_work_mode();

                if (mode != DSO && confirm_to_store_data())
                {
                    _is_auto_switch_device = true;

                    if (_session->is_working())
                        _session->stop_capture();

                    on_save();
                }
                else
                {   
                    if (_session->is_working())
                        _session->stop_capture();
                    
                    _session->set_default_device();
                }

                break;
            }
            case DSV_MSG_CURRENT_DEVICE_DETACHED:
            {
                // Suppress device-detach handling during intentional session
                // switches: remove_msg_listener / add_msg_listener during
                // switch_to_session can deliver queued messages; we must not
                // call set_default_device() in that context.
                if (_is_switching_session)
                    break;

                if (_msg != NULL){
                    _msg->close();
                    _msg = NULL;
                }

                // Save current config, and switch to the last device.
                _session->device_event_object()->device_updated();
                save_config();
                _view->hide_calibration();

                if (_session->is_saving()){
                    dsv_info("Device detached:Waitting for store the data. and will switch to new device.");
                    _is_auto_switch_device = true;
                    return;
                }

                if (confirm_to_store_data()){
                    _is_auto_switch_device = true;
                    on_save();
                }
                else{
                    _session->set_default_device();
                }
                break;
            }
            case DSV_MSG_SAVE_COMPLETE:
            {
                _session->clear_store_confirm_flag();

                if (_is_auto_switch_device)
                {
                    _is_auto_switch_device = false;
                    _session->set_default_device();
                }
                else
                {
                    ds_device_handle devh = _sampling_bar->get_next_device_handle();
                    if (devh != NULL_HANDLE)
                    {
                        dsv_info("Auto switch to the selected device.");
                        _session->set_device(devh);
                    }
                }
                break;
            }
            case DSV_MSG_CLEAR_DECODE_DATA:
            {
                if (_device_agent->get_work_mode() == LOGIC)
                    _sidebar_widget->protocol_widget()->reset_view();
                break;
            }            
            case DSV_MSG_STORE_CONF_PREV:
            {
                if (_device_agent->is_hardware() && _session->have_hardware_data() == false){
                    _sampling_bar->commit_settings();
                }
                break;
            }
            case DSV_MSG_BEGIN_DEVICE_OPTIONS:
            case DSV_MSG_COLLECT_MODE_CHANGED:
            {
                if(_device_agent->is_demo()){
                    _pattern_mode = _device_agent->get_demo_operation_mode();
                }
                if (msg == DSV_MSG_COLLECT_MODE_CHANGED){
                    _sidebar_widget->trigger_widget()->device_updated();
                    _view->update();
                }           
                break;
            }
            case DSV_MSG_END_DEVICE_OPTIONS:
            case DSV_MSG_DEMO_OPERATION_MODE_CHNAGED:
            {
                bool isDeviceTab = (_session_items.isEmpty() ||
                                    _session == _session_items[0].session);
                if(isDeviceTab && _device_agent->is_demo() &&_device_agent->get_work_mode() == LOGIC){                
                    QString pattern_mode = _device_agent->get_demo_operation_mode();       
                    
                    if(pattern_mode != _pattern_mode)
                    {
                        _pattern_mode = pattern_mode; 

                        _device_agent->update();
                        _session->clear_view_data();
                        _session->init_signals();
                        update_toolbar_view_status();
                        _sampling_bar->update_sample_rate_list();
                        _sidebar_widget->protocol_widget()->del_all_protocol();
                            
                        if(_pattern_mode != "random"){
                            _session->set_collect_mode(COLLECT_SINGLE);
                            load_demo_decoder_config(_pattern_mode);

                            if (msg == DSV_MSG_END_DEVICE_OPTIONS)
                                _session->start_capture(false); // Auto load data.
                        }
                    }                
                }
                calc_min_height();            
                break;
            }
            case DSV_MSG_APP_OPTIONS_CHANGED:
            {
                update_title_bar_text();
                break;
            }
            case DSV_MSG_FONT_OPTIONS_CHANGED:
            {
                UiManager::Instance()->Update(UI_UPDATE_ACTION_FONT);          
                break;
            }
            case DSV_MSG_DATA_POOL_CHANGED:
            {
                _view->check_measure();
                break;
            }            
        }
    }

    void MainWindow::calc_min_height()
    {
        if (_frame != NULL)
        {
            if (_device_agent->get_work_mode() == LOGIC)
            {
                int ch_num = _session->get_ch_num(-1);
                int win_height = Base_Height + Per_Chan_Height * ch_num;

                if (win_height < Min_Height)
                    _frame->setMinimumHeight(win_height);
                else
                    _frame->setMinimumHeight(Min_Height);
            }
            else{
                _frame->setMinimumHeight(Min_Height);
            }
        }  
    }

    void MainWindow::delay_prop_msg(QString strMsg)
    {
        _strMsg = strMsg;
        if (_strMsg != ""){
            _delay_prop_msg_timer.Start(500);
        }
    }

    void MainWindow::on_delay_prop_msg()
    {
        _delay_prop_msg_timer.Stop();

        if (_strMsg != ""){
            MsgBox::Show("", _strMsg, this, &_msg);
            _msg = NULL;
        }            
    }

    void MainWindow::update_title_bar_text()
    {
          // Set the title
        QString title = QApplication::applicationName() + " v" + QApplication::applicationVersion();
        AppConfig &app = AppConfig::Instance();

        if (_title_ext_string != "" && app.appOptions.displayProfileInBar){
            title += " [" + _title_ext_string + "]";
        }

        if (_lst_title_string != title){
            _lst_title_string = title;

            setWindowTitle(QApplication::translate("MainWindow", title.toLocal8Bit().data(), 0));
            _title_bar->setTitle(this->windowTitle());
        }        
    }

    void MainWindow::load_demo_decoder_config(QString optname)
    { 
        QString file = GetAppDataDir() + "/demo/logic/" + optname + ".demo";
        bool bLoadSurccess = false;

        QJsonArray deArray = get_decoder_json_from_data_file(file, bLoadSurccess);

        if (bLoadSurccess){
            StoreSession ss(_session);
            ss.load_decoders(_sidebar_widget->protocol_widget(), deArray);
        }

        QJsonDocument doc = get_config_json_from_data_file(file, bLoadSurccess);
        if (bLoadSurccess){
            load_channel_view_indexs(doc);
        }
        
        _view->update_all_trace_postion();
    }

    QWidget* MainWindow::GetBodyView()
    {
        return _view;
    }   
  
} // namespace pv
