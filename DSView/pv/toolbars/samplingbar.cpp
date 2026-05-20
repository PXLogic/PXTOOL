/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
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

#include "samplingbar.h"
#include <assert.h>
#include <QAction>
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QStandardItemModel>
#include <QStyleFactory>
#include <math.h>
#include <libusb-1.0/libusb.h>
#include "../dialogs/deviceoptions.h"
#include "../dialogs/waitingdialog.h"
#include "../dialogs/dsmessagebox.h"
#include "../view/dsosignal.h"
#include "../dialogs/interval.h"
#include "../config/appconfig.h"
#include "../dsvdef.h"
#include "../log.h"
#include "../deviceagent.h"
#include "../ui/msgbox.h"
#include "../view/view.h"
#include "../ui/fn.h"

#define SINGLE_ACTION_ICON  "/once.svg"
#define REPEAT_ACTION_ICON  "/repeat.svg"
#define LOOP_ACTION_ICON  "/loop.svg"

using std::map;
using std::max;
using std::min;
using std::string;

namespace pv
{
    namespace toolbars
    {

        const QString SamplingBar::RLEString = "(RLE)";
        const QString SamplingBar::DIVString = " / div";

        SamplingBar::SamplingBar(SigSession *session, QWidget *parent) : QToolBar("Sampling Bar", parent),
                                                                         _device_type(this),
                                                                         _device_selector(this),
                                                                         _configure_button(this),
                                                                         _sample_count(this),
                                                                         _sample_rate(this),
                                                                         _run_stop_button(this),
                                                                         _instant_button(this),
                                                                         _mode_button(this),
                                                                         _capture_strip(nullptr),
                                                                         _capture_container(nullptr),
                                                                         _capture_action(nullptr)
        {
            _updating_device_list = false;
            _updating_sample_rate = false;
            _updating_sample_count = false;
            _is_run_as_instant = false;

            _last_device_handle = NULL_HANDLE;
            _last_device_index = -1;
            _next_switch_device = NULL_HANDLE;
            _view = NULL;

            _session = session;
            _device_agent = _session->get_device();

            setMovable(false);
            setContentsMargins(0, 0, 0, 0);
            if (layout())
                layout()->setSpacing(0);

            _device_selector.setSizeAdjustPolicy(DsComboBox::AdjustToContents);
            _sample_rate.setSizeAdjustPolicy(DsComboBox::AdjustToContents);
            _sample_count.setSizeAdjustPolicy(DsComboBox::AdjustToContents);
            _device_selector.setMaximumWidth(ComboBoxMaxWidth);

            // Combo boxes use explicit per-widget stylesheets (applied in reStyle)
            // rather than setStyle(Fusion), which bypasses QStyleSheetStyle and
            // prevents qApp QSS rules from applying on macOS.

            //tr
            _run_stop_button.setObjectName("run_stop_button");
            _instant_button.setObjectName("instant_capture_button");

            // Mode combo: index 0=Single, 1=Repetitive, 2=Loop
            _mode_button.addItem("Single");
            _mode_button.addItem("Repetitive");
            _mode_button.addItem("Loop");

            connect(&_device_selector, SIGNAL(currentIndexChanged(int)), this, SLOT(on_device_selected()));
            connect(&_configure_button, SIGNAL(clicked()), this, SLOT(on_configure()));
            connect(&_run_stop_button, SIGNAL(clicked()), this, SLOT(on_run_stop()));
            connect(&_instant_button, SIGNAL(clicked()), this, SLOT(on_instant_stop()));
            connect(&_sample_count, SIGNAL(currentIndexChanged(int)), this, SLOT(on_samplecount_sel(int)));
            connect(&_mode_button, SIGNAL(currentIndexChanged(int)), this, SLOT(on_mode_changed(int)));
            connect(&_sample_rate, SIGNAL(currentIndexChanged(int)), this, SLOT(on_samplerate_sel(int)));

            ADD_UI(this);
        }

        void SamplingBar::attachCaptureToolBar(QToolBar *strip)
        {
            assert(strip != nullptr);
            if (_capture_strip != nullptr) return;
            _capture_strip = strip;

            strip->setMovable(false);
            strip->setContentsMargins(0, 0, 0, 0);
            if (strip->layout())
                strip->layout()->setSpacing(0);

            // Single container fills the strip so we control layout entirely
            _capture_container = new QWidget(strip);
            _capture_container->setObjectName("device_bar_inner");
            _capture_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

            QHBoxLayout *lay = new QHBoxLayout(_capture_container);
            lay->setContentsMargins(10, 1, 10, 1);
            lay->setSpacing(0);

            // Helper: label + control in a tight group; optionally stores the label pointer
            auto makeGroup = [&](const QString &labelText, QWidget *ctrl, QLabel **lblOut = nullptr) -> QWidget* {
                QWidget *g = new QWidget(_capture_container);
                QHBoxLayout *gl = new QHBoxLayout(g);
                gl->setContentsMargins(0, 0, 0, 0);
                gl->setSpacing(5);
                QLabel *lbl = new QLabel(labelText, g);
                lbl->setObjectName("device_bar_label");
                if (lblOut) *lblOut = lbl;
                gl->addWidget(lbl);
                gl->addWidget(ctrl);
                return g;
            };

            // Vertical separator between groups
            auto makeSep = [&]() -> QWidget* {
                QFrame *f = new QFrame(_capture_container);
                f->setFrameShape(QFrame::VLine);
                f->setObjectName("device_bar_sep");
                return f;
            };

            // Trigger sidebar toggle button (leftmost)
            _triggers_toggle_btn.setObjectName("TriggerToggleButton");
            _triggers_toggle_btn.setToolTip(tr("Show/Hide trigger settings"));
            _triggers_toggle_btn.setFixedWidth(30);
            _triggers_toggle_btn.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            _triggers_toggle_btn.setIconSize(QSize(18, 18));
            connect(&_triggers_toggle_btn, SIGNAL(clicked()), this, SLOT(on_triggers_toggle()));
            lay->addWidget(&_triggers_toggle_btn);
            lay->addSpacing(8);

            // DEVICE: selector only
            {
                QWidget *g = new QWidget(_capture_container);
                QHBoxLayout *gl = new QHBoxLayout(g);
                gl->setContentsMargins(0, 0, 0, 0);
                gl->setSpacing(4);
                _lbl_device = new QLabel(tr("Device"), g);
                _lbl_device->setObjectName("device_bar_label");
                gl->addWidget(_lbl_device);
                gl->addWidget(&_device_selector);
                lay->addWidget(g);
            }

            lay->addSpacing(16);

            // SAMPLE RATE
            lay->addWidget(makeGroup(tr("Sample Rate"), &_sample_rate, &_lbl_smplrate));

            lay->addSpacing(16);

            // BUFFER (sample count / duration)
            lay->addWidget(makeGroup(tr("Buffer"), &_sample_count, &_lbl_buffer));

            lay->addSpacing(16);

            // MODE
            _mode_button.setObjectName("mode_button");
            lay->addWidget(makeGroup(tr("Mode"), &_mode_button, &_lbl_mode));

            lay->addStretch(1);

            // Start / Stop capture
            _run_stop_button.setObjectName("run_stop_button");
            _run_stop_button.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            _run_stop_button.setStyle(QStyleFactory::create("Fusion"));
            _run_stop_button.setStyleSheet(
                "QToolButton { background-color: rgba(147,51,234,0.2);"
                "  border: 1px solid #9333ea; border-radius: 4px;"
                "  color: #c084fc; font-size: 12px; font-weight: 500;"
                "  padding: 0px 16px; }"
                "QToolButton:hover { background-color: rgba(147,51,234,0.3);"
                "  border-color: #a855f7; color: #e9d5ff; }");
            lay->addWidget(&_run_stop_button);

            lay->addSpacing(8);

            // Instant
            _instant_button.setObjectName("instant_capture_button");
            _instant_button.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            _instant_button.setStyle(QStyleFactory::create("Fusion"));
            _instant_button.setStyleSheet(
                "QToolButton { background-color: rgba(22,163,74,0.1);"
                "  border: 1px solid rgba(22,163,74,0.5); border-radius: 4px;"
                "  color: #4ade80; font-size: 12px; font-weight: 500;"
                "  padding: 0px 16px; }"
                "QToolButton:hover { background-color: rgba(22,163,74,0.2);"
                "  border-color: #16a34a; color: #86efac; }"
                "QToolButton:disabled { background-color: rgba(80,80,80,0.1);"
                "  border-color: rgba(100,100,100,0.4); color: #555555; }");
            lay->addWidget(&_instant_button);

            // Visibility now controlled on the widget directly, not via QAction
            _mode_action     = nullptr;
            _run_stop_action = nullptr;
            _instant_action  = nullptr;

            _capture_action = strip->addWidget(_capture_container);

            update_view_status();
        }

        SamplingBar::~SamplingBar()
        {
            REMOVE_UI(this);
        }

        void SamplingBar::setSession(SigSession *session)
        {
            _session      = session;
            _device_agent = session->get_device();
            update_device_list();
        }

        void SamplingBar::detachFromDeviceBar()
        {
            if (_capture_strip == nullptr)
                return;

            // Reparent member-backed widgets off the container before it is destroyed
            auto lift = [this](QWidget *w) {
                if (w != nullptr)
                    w->setParent(this);
            };

            lift(&_device_type);
            lift(&_device_selector);
            lift(&_configure_button);
            lift(&_sample_rate);
            lift(&_sample_count);
            lift(&_mode_button);
            lift(&_run_stop_button);
            lift(&_instant_button);

            if (_capture_action != nullptr) {
                _capture_strip->removeAction(_capture_action);
                _capture_action = nullptr;
            }
            // _capture_container is now empty and owned by the strip; it will be deleted with it
            _capture_container = nullptr;
            _capture_strip = nullptr;
        }

        void SamplingBar::retranslateUi()
        {
            if (_lbl_device)   _lbl_device->setText(tr("Device"));
            if (_lbl_smplrate) _lbl_smplrate->setText(tr("Sample Rate"));
            if (_lbl_buffer)   _lbl_buffer->setText(tr("Buffer"));
            if (_lbl_mode)     _lbl_mode->setText(tr("Mode"));

            bool bDev = _device_agent->have_instance();

            if (bDev)
            {
                if (_device_agent->is_demo())
                {
                    _device_type.setText(tr("Demo"));
                }
                else if (_device_agent->is_file())
                {
                    _device_type.setText(tr("File"));
                }
                else
                {
                    int usb_speed = LIBUSB_SPEED_HIGH;
                    _device_agent->get_config_int32(SR_CONF_USB_SPEED, usb_speed);

                    if (usb_speed == LIBUSB_SPEED_HIGH)
                        _device_type.setText("USB 2.0");
                    else if (usb_speed == LIBUSB_SPEED_SUPER)
                        _device_type.setText("USB 3.0");
                    else
                        _device_type.setText("USB UNKNOWN");
                }
            }
            _configure_button.setText(tr("Options"));
           _mode_button.setItemText(0, QString(tr("Single")).remove('&'));
           _mode_button.setItemText(1, QString(tr("Repetitive")).remove('&'));
           _mode_button.setItemText(2, QString(tr("Loop")).remove('&'));

            int mode = _device_agent->get_work_mode();
            bool is_working = _session->is_working();

            auto str_start = tr("Start");
            auto str_stop  = tr("Stop");
            auto str_single  = tr("Single");
            auto str_instant  = tr("Instant");
            auto str_one_stop  = tr("Stop");

            if (_is_run_as_instant)
            {
                if (bDev && mode == DSO)
                    _instant_button.setText(is_working ? str_one_stop : str_single);
                else
                    _instant_button.setText(is_working ? str_one_stop : str_instant);

                _run_stop_button.setText(str_start);
            }
            else
            {
                _run_stop_button.setText(is_working ? str_stop: str_start);

                if (bDev && mode == DSO)
                    _instant_button.setText(str_single);
                else
                    _instant_button.setText(str_instant);
            }

        }

        void SamplingBar::reStyle()
        {
            // Apply combo box styles directly so they remain correct regardless of
            // whether measureSize() has overwritten the widget stylesheet.
            bool isDark = (AppConfig::Instance().frameOptions.style != "light");
            QString comboBg     = isDark ? "#1a1a1a" : "#ffffff";
            QString comboBorder = isDark ? "#444444" : "#6b7280";
            QString comboFg     = isDark ? "#d1d5db" : "#374151";
            QString comboHoverBorder = isDark ? "#666666" : "#374151";
            QString comboDisabledBg = isDark ? "#111111" : "#f3f4f6";
            QString comboDisabledFg = isDark ? "#555555" : "#9ca3af";
            QString comboDisabledBorder = isDark ? "#2a2a2a" : "#d1d5db";
            QString comboStyle =
                QString("QComboBox { background-color: %1; border: 1px solid %2;"
                        " border-radius: 4px; padding: 3px 8px; min-height: 22px;"
                        " color: %3; font-size: 12px; }"
                        "QComboBox:hover { border-color: %4; }"
                        "QComboBox:disabled { background-color: %5; border-color: %6;"
                        " color: %7; }"
                        "QComboBox::drop-down { border: none; width: 16px; }")
                .arg(comboBg, comboBorder, comboFg, comboHoverBorder,
                     comboDisabledBg, comboDisabledBorder, comboDisabledFg);
            _device_selector.setStyleSheet(comboStyle);
            _sample_rate.setStyleSheet(comboStyle);
            _sample_count.setStyleSheet(comboStyle);
            _mode_button.setStyleSheet(comboStyle);

            bool bDev = _device_agent->have_instance();

            if (bDev)
            {
                if (_device_agent->is_demo())
                    _device_type.setIcon(QIcon(":/icons/demo.svg"));
                else if (_device_agent->is_file())
                    _device_type.setIcon(QIcon(":/icons/data.svg"));
                else
                {
                    int usb_speed = LIBUSB_SPEED_HIGH;
                    _device_agent->get_config_int32(SR_CONF_USB_SPEED, usb_speed);

                    if (usb_speed == LIBUSB_SPEED_SUPER)
                        _device_type.setIcon(QIcon(":/icons/usb3.svg"));
                    else
                        _device_type.setIcon(QIcon(":/icons/usb2.svg"));
                }
            }

            if (true)
            {
                QString iconPath = GetIconPath();
                update_triggers_toggle_btn();
                _configure_button.setIcon(QIcon(iconPath + "/params.svg"));
            
                bool is_working = _session->is_working();
                QString icon2 = is_working ? "stop.svg" : "start.svg";
                _run_stop_button.setIcon(QIcon(iconPath + "/" + icon2));
                _instant_button.setIcon(QIcon(iconPath + "/single.svg"));

                // Update run/stop button style directly — property selectors are
                // unreliable with per-widget Fusion style on macOS.
                if (is_working) {
                    _run_stop_button.setStyleSheet(
                        "QToolButton { background-color: rgba(220,38,38,0.2);"
                        "  border: 1px solid #dc2626; border-radius: 4px;"
                        "  color: #f87171; font-size: 12px; font-weight: 500;"
                        "  padding: 0px 16px; }"
                        "QToolButton:hover { background-color: rgba(220,38,38,0.3);"
                        "  border-color: #ef4444; color: #fca5a5; }");
                } else {
                    _run_stop_button.setStyleSheet(
                        "QToolButton { background-color: rgba(147,51,234,0.2);"
                        "  border: 1px solid #9333ea; border-radius: 4px;"
                        "  color: #c084fc; font-size: 12px; font-weight: 500;"
                        "  padding: 0px 16px; }"
                        "QToolButton:hover { background-color: rgba(147,51,234,0.3);"
                        "  border-color: #a855f7; color: #e9d5ff; }");
                }


                update_mode_icon();
            }
        }

        void SamplingBar::on_configure()
        {
            int ret;

            if (_device_agent->have_instance() == false)
            {
                dsv_info("Have no device, can't to set device config.");
                return;
            }

            _session->broadcast_msg(DSV_MSG_BEGIN_DEVICE_OPTIONS);

            pv::dialogs::DeviceOptions dlg(this);
            connect(_session->device_event_object(), SIGNAL(device_updated()), &dlg, SLOT(reject()));

            ret = dlg.exec();

            if (ret == QDialog::Accepted)
            {
                if (_session->have_view_data() == false)
                    this->commit_settings();

                _session->broadcast_msg(DSV_MSG_DEVICE_OPTIONS_UPDATED);

                update_sample_rate_list();

                int mode = _device_agent->get_work_mode();
                bool zero = false;
                bool test;
                bool ret;

                if (mode == DSO){
                    _device_agent->get_config_bool(SR_CONF_ZERO, zero);
                   
                    if (zero){
                        zero_adj();
                        return;
                    }
                }

                ret = _device_agent->get_config_bool(SR_CONF_TEST, test);
                if (ret)
                {
                    if (test)
                    {
                        update_sample_rate_selector_value();
                        _sample_count.setDisabled(true);
                        _sample_rate.setDisabled(true);
                    }
                    else
                    {
                        _sample_count.setDisabled(false);
                        if (mode != DSO)
                            _sample_rate.setDisabled(false);
                    }
                }

                this->reload();
            }

            _session->broadcast_msg(DSV_MSG_END_DEVICE_OPTIONS);
        }

        void SamplingBar::zero_adj()
        { 
            for (auto s : _session->get_signals())
            {
                if (s->signal_type() == SR_CHANNEL_DSO){
                    view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                    dsoSig->set_enable(true);
                }
            }

            const int index_back = _sample_count.currentIndex();
            int i = 0;

            for (i = 0; i < _sample_count.count(); i++){
                if (_sample_count.itemData(i).value<uint64_t>() == ZeroTimeBase)
                    break;
            }

            set_sample_count_index(i);
            commit_hori_res();

            if (_session->is_working() == false)
                _session->start_capture(false);

            pv::dialogs::WaitingDialog wait(this, _session, SR_CONF_ZERO);
            if (wait.start() == QDialog::Rejected)
            {
                for (auto s : _session->get_signals())
                {
                    if (s->signal_type() == SR_CHANNEL_DSO){
                        view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                        dsoSig->commit_settings();
                    }
                }
            }

            if (_session->is_working())
                _session->stop_capture();

            set_sample_count_index(index_back);
            commit_hori_res();
        }

        void SamplingBar::set_sample_rate(uint64_t sample_rate)
        {
            for (int i = _sample_rate.count() - 1; i >= 0; i--)
            {
                uint64_t cur_index_sample_rate = _sample_rate.itemData(
                                                                 i)
                                                     .value<uint64_t>();
                if (sample_rate >= cur_index_sample_rate)
                {
                    _sample_rate.setCurrentIndex(i);
                    break;
                }
            }
            commit_settings();
        }

        void SamplingBar::update_sample_rate_selector()
        { 
            GVariant *gvar_dict, *gvar_list;
            const uint64_t *elements = NULL;
            gsize num_elements;

            dsv_info("Update rate list.");

            if (_updating_sample_rate)
            {
                dsv_err("Error! The rate list is updating.");
                return;
            }

            disconnect(&_sample_rate, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(on_samplerate_sel(int)));

            if (_device_agent->have_instance() == false)
            {
                dsv_info("DBG update_sample_rate_selector: have_instance=false, returning early");
                connect(&_sample_rate, SIGNAL(currentIndexChanged(int)),
                        this, SLOT(on_samplerate_sel(int)));
                return;
            }

            _updating_sample_rate = true;

            gvar_dict = _device_agent->get_config_list(NULL, SR_CONF_SAMPLERATE);

            if (gvar_dict == NULL)
            {
                _sample_rate.clear();
                _sample_rate.show();
                _updating_sample_rate = false;
                connect(&_sample_rate, SIGNAL(currentIndexChanged(int)),
                        this, SLOT(on_samplerate_sel(int)));
                update_sample_count_selector();
                return;
            }

            if ((gvar_list = g_variant_lookup_value(gvar_dict,
                                                    "samplerates", G_VARIANT_TYPE("at"))))
            {
                elements = (const uint64_t *)g_variant_get_fixed_array(
                    gvar_list, &num_elements, sizeof(uint64_t));
                _sample_rate.clear();

                for (unsigned int i = 0; i < num_elements; i++)
                {
                    char *const s = sr_samplerate_string(elements[i]);
                    _sample_rate.addItem(QString(s),
                                         QVariant::fromValue(elements[i]));
                    g_free(s);
                }


                _sample_rate.show();
                g_variant_unref(gvar_list);
            }


            _sample_rate.setMinimumWidth(_sample_rate.sizeHint().width() + 15);
            _sample_rate.view()->setMinimumWidth(_sample_rate.sizeHint().width() + 30);

            _updating_sample_rate = false;
            g_variant_unref(gvar_dict);

            update_sample_rate_selector_value();

            connect(&_sample_rate, SIGNAL(currentIndexChanged(int)),
                    this, SLOT(on_samplerate_sel(int)));

            update_sample_count_selector();
        }

        void SamplingBar::update_sample_rate_selector_value()
        {
            if (_updating_sample_rate)
                return;
            _updating_sample_rate = true;

            const uint64_t samplerate = _device_agent->get_sample_rate();
            uint64_t cur_value = _sample_rate.itemData(_sample_rate.currentIndex()).value<uint64_t>();

            if (samplerate != cur_value)
            {
                for (int i = _sample_rate.count() - 1; i >= 0; i--)
                {
                    if (samplerate >= _sample_rate.itemData(i).value<uint64_t>())
                    {
                        _sample_rate.setCurrentIndex(i);
                        break;
                    }
                }
            }

            _updating_sample_rate = false;
        }

        void SamplingBar::on_samplerate_sel(int index)
        {
            (void)index;
            if (_device_agent->get_work_mode() != DSO)
                update_sample_count_selector();
        }

        void SamplingBar::update_sample_count_selector()
        {
            bool stream_mode = false;
            uint64_t hw_depth = 0;
            uint64_t sw_depth;
            uint64_t rle_depth = 0;
            uint64_t max_timebase = 0;
            uint64_t min_timebase = SR_NS(10);
            double pre_duration = SR_SEC(1);
            double duration;
            bool rle_support = false;

            dsv_info("Update sample count list.");

            if (_updating_sample_count)
            {
                dsv_err("Error! The sample count is updating.");
                return;
            }

            disconnect(&_sample_count, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(on_samplecount_sel(int)));

            assert(!_updating_sample_count);
            _updating_sample_count = true;

            _device_agent->get_config_bool(SR_CONF_STREAM, stream_mode);
            _device_agent->get_config_uint64(SR_CONF_HW_DEPTH, hw_depth);
            int mode = _device_agent->get_work_mode();

            if (mode == LOGIC)
            {
#if defined(__x86_64__) || defined(_M_X64)
                sw_depth = LogicMaxSWDepth64;
#elif defined(__i386) || defined(_M_IX86)
                int ch_num = _session->get_ch_num(SR_CHANNEL_LOGIC);
                if (ch_num <= 0)
                    sw_depth = LogicMaxSWDepth32;
                else
                    sw_depth = LogicMaxSWDepth32 / ch_num;
#endif
            }
            else
            {
                sw_depth = AnalogMaxSWDepth;
            }

            if (mode == LOGIC)
            {
                _device_agent->get_config_bool(SR_CONF_RLE_SUPPORT, rle_support);
                if (rle_support)
                    rle_depth = min(hw_depth * SR_KB(1), sw_depth);
            }
            else if (mode == DSO)
            {
                _device_agent->get_config_uint64(SR_CONF_MAX_TIMEBASE, max_timebase);
                _device_agent->get_config_uint64(SR_CONF_MIN_TIMEBASE, min_timebase);
            }

            if (0 != _sample_count.count())
                pre_duration = _sample_count.itemData(
                                                _sample_count.currentIndex())
                                   .value<double>();
            _sample_count.clear();
            const uint64_t samplerate = _sample_rate.itemData(
                                                        _sample_rate.currentIndex())
                                            .value<uint64_t>();
            const double hw_duration = hw_depth / (samplerate * (1.0 / SR_SEC(1)));

            if (mode == DSO)
                duration = max_timebase;
            else if (stream_mode)
                duration = sw_depth / (samplerate * (1.0 / SR_SEC(1)));
            else if (rle_support)
                duration = rle_depth / (samplerate * (1.0 / SR_SEC(1)));
            else
                duration = hw_duration;

            assert(duration > 0);
            bool not_last = true;

            do
            {
                QString suffix = (mode == DSO) ? DIVString : (!stream_mode && duration > hw_duration) ? RLEString
                                                                                                      : "";
                char *const s = sr_time_string(duration);
                _sample_count.addItem(QString(s) + suffix, QVariant::fromValue(duration));
                g_free(s);

                double unit;
                if (duration >= SR_DAY(1))
                    unit = SR_DAY(1);
                else if (duration >= SR_HOUR(1))
                    unit = SR_HOUR(1);
                else if (duration >= SR_MIN(1))
                    unit = SR_MIN(1);
                else
                    unit = 1;

                const double log10_duration = pow(10, floor(log10(duration / unit)));

                if (duration > 5 * log10_duration * unit)
                    duration = 5 * log10_duration * unit;
                else if (duration > 2 * log10_duration * unit)
                    duration = 2 * log10_duration * unit;
                else if (duration > log10_duration * unit)
                    duration = log10_duration * unit;
                else
                    duration = log10_duration > 1 ? duration * 0.5 : (unit == SR_DAY(1) ? SR_HOUR(20) : unit == SR_HOUR(1) ? SR_MIN(50)
                                                                                                    : unit == SR_MIN(1)    ? SR_SEC(50)
                                                                                                                           : duration * 0.5);

                if (mode == DSO)
                    not_last = duration >= min_timebase;
                else if (mode == ANALOG)
                    not_last = (duration >= SR_MS(200)) &&
                               (duration / SR_SEC(1) * samplerate >= SR_KB(1));
                else
                    not_last = (duration / SR_SEC(1) * samplerate >= SR_KB(1));

            } while (not_last);

            _updating_sample_count = true;

            if (pre_duration > _sample_count.itemData(0).value<double>())
            {
                set_sample_count_index(0);
            }
            else if (pre_duration < _sample_count.itemData(_sample_count.count() - 1).value<double>())
            {
                set_sample_count_index(_sample_count.count() - 1);
            }
            else
            {
                for (int i = 0; i < _sample_count.count(); i++)
                {
                    double sel_val = _sample_count.itemData(i).value<double>();
                    if (pre_duration >= sel_val)
                    {
                        set_sample_count_index(i);
                        break;
                    }
                }
            }
            _updating_sample_count = false;

            update_sample_count_selector_value();
            on_samplecount_sel(_sample_count.currentIndex());

            connect(&_sample_count, SIGNAL(currentIndexChanged(int)), this, SLOT(on_samplecount_sel(int)));
        }

        void SamplingBar::update_sample_count_selector_value()
        {
            if (_updating_sample_count)
                return;

            double duration;
            uint64_t v;

            if (_device_agent->get_work_mode() == DSO)
            { 
                if (_device_agent->get_config_uint64(SR_CONF_TIMEBASE, v))
                {
                    duration = (double)v; 
                }
                else
                {
                    dsv_err("ERROR: config_get SR_CONF_TIMEBASE failed.");
                    return;
                }
            }
            else
            { 
                if (_device_agent->get_config_uint64(SR_CONF_LIMIT_SAMPLES, v))
                {
                    duration = (double)v; 
                }
                else
                {
                    dsv_err("ERROR: config_get SR_CONF_TIMEBASE failed.");
                    return;
                }
                const uint64_t samplerate = _device_agent->get_sample_rate();
                duration = duration / samplerate * SR_SEC(1);
            }
            assert(!_updating_sample_count);
            _updating_sample_count = true;

            double cur_duration = _sample_count.itemData(_sample_count.currentIndex()).value<double>();
            if (duration != cur_duration)
            {
                for (int i = 0; i < _sample_count.count(); i++)
                {
                    double sel_val = _sample_count.itemData(i).value<double>();
                    if (duration >= sel_val)
                    {
                        set_sample_count_index(i);
                        break;
                    }
                }
            }

            _updating_sample_count = false;
        }

        void SamplingBar::apply_sample_count(double &hori_res)
        {   
            hori_res = -1;

            if (_device_agent->get_work_mode() == DSO){
                hori_res = commit_hori_res();

                if (_session->have_view_data() == false){
                    _session->apply_samplerate();
                }
            }

            _session->broadcast_msg(DSV_MSG_DEVICE_DURATION_UPDATED);
        }

        void SamplingBar::on_samplecount_sel(int index)
        {
            (void)index;

            double hori_res = -1;
            apply_sample_count(hori_res);
        }

        double SamplingBar::get_hori_res()
        {
            return _sample_count.itemData(_sample_count.currentIndex()).value<double>();
        }

        double SamplingBar::hori_knob(int dir)
        {
            double hori_res = -1;

            if (_session->get_device()->get_work_mode() != DSO){
                assert(false);
            }

            disconnect(&_sample_count, SIGNAL(currentIndexChanged(int)), this, SLOT(on_samplecount_sel(int)));

            if (0 == dir)
            {
                hori_res = commit_hori_res();
            }
            else if ((dir > 0) && (_sample_count.currentIndex() > 0))
            {
                set_sample_count_index(_sample_count.currentIndex() - 1);
                hori_res = commit_hori_res();

                if (_session->have_view_data() == false){                   
                    _session->apply_samplerate(); 
                    _session->broadcast_msg(DSV_MSG_DEVICE_DURATION_UPDATED);                 
                }
            }
            else if ((dir < 0) && (_sample_count.currentIndex() < _sample_count.count() - 1))
            {
                set_sample_count_index(_sample_count.currentIndex() + 1); 
                hori_res = commit_hori_res();             

                if (_session->have_view_data() == false){
                    _session->apply_samplerate();
                    _session->broadcast_msg(DSV_MSG_DEVICE_DURATION_UPDATED);
                }
            }

            connect(&_sample_count, SIGNAL(currentIndexChanged(int)),
                    this, SLOT(on_samplecount_sel(int)));

            return hori_res;
        }

        double SamplingBar::commit_hori_res()
        {
            const double hori_res = _sample_count.itemData(
                                                     _sample_count.currentIndex())
                                        .value<double>();

            const uint64_t sample_limit = _device_agent->get_sample_limit();
            uint64_t max_sample_rate;

            if (_device_agent->get_config_uint64(SR_CONF_MAX_DSO_SAMPLERATE, max_sample_rate) == false)
            {
                dsv_err("ERROR: config_get SR_CONF_MAX_DSO_SAMPLERATE failed.");
                return -1;
            }

            const uint64_t sample_rate = min((uint64_t)(sample_limit * SR_SEC(1) /
                                                        (hori_res * DS_CONF_DSO_HDIVS)),
                                             (uint64_t)(max_sample_rate /
                                                        (_session->get_ch_num(SR_CHANNEL_DSO) ? _session->get_ch_num(SR_CHANNEL_DSO) : 1)));
            set_sample_rate(sample_rate);

            _device_agent->set_config_uint64( SR_CONF_TIMEBASE, hori_res);

            return hori_res;
        }

        void SamplingBar::commit_settings()
        {
            bool test = false;
            if (_device_agent->have_instance())
            {
                _device_agent->get_config_bool(SR_CONF_TEST, test);
            }

            if (test)
            {
                update_sample_rate_selector_value();
                update_sample_count_selector_value();
            }
            else
            {
                const double sample_duration = _sample_count.itemData(
                                                                _sample_count.currentIndex())
                                                   .value<double>();
                const uint64_t sample_rate = _sample_rate.itemData(
                                                             _sample_rate.currentIndex())
                                                 .value<uint64_t>();

                if (_device_agent->have_instance())
                {
                    if (sample_rate != _device_agent->get_sample_rate())
                        _device_agent->set_config_uint64(
                                                  SR_CONF_SAMPLERATE,
                                                  sample_rate);

                    if (_device_agent->get_work_mode() != DSO)
                    {
                        const uint64_t sample_count = ((uint64_t)ceil(sample_duration / SR_SEC(1) *
                                                                      sample_rate) +
                                                       SAMPLES_ALIGN) &
                                                      ~SAMPLES_ALIGN;
                        if (sample_count != _device_agent->get_sample_limit())
                            _device_agent->set_config_uint64(
                                                      SR_CONF_LIMIT_SAMPLES,
                                                      sample_count);

                        bool rle_mode = _sample_count.currentText().contains(RLEString);
                        _device_agent->set_config_bool(
                                                  SR_CONF_RLE,
                                                  rle_mode);
                    }
                }
            }
        }

        void SamplingBar::on_run_stop()
        {
            _run_stop_button.setEnabled(false);
            QTimer::singleShot(10, this, &SamplingBar::on_run_stop_action);
        }

        void SamplingBar::on_run_stop_action()
        {
            action_run_stop();
            _run_stop_button.setEnabled(true);
        }
      
        // start or stop capture
        bool SamplingBar::action_run_stop()
        {    
            if (_session->is_doing_action()){
                dsv_info("Task is busy.");              
                return false;
            }

            if (_session->is_working()){
                return _session->stop_capture();
            }

            if (_device_agent->have_instance() == false)
            {
                dsv_info("Have no device, can't to collect data.");
                return false;
            }

            commit_settings();

            if (_device_agent->get_work_mode() == DSO)
            {
                bool zero;

                bool ret = _device_agent->get_config_bool(SR_CONF_ZERO, zero);
                if (ret && zero)
                {   
                    QString str1(tr("Auto Calibration"));
                    QString str2(tr("Please adjust zero skew and save the result"));
                    bool bRet = MsgBox::Confirm(str1, str2);

                    if (bRet)
                    {
                        zero_adj();
                    }
                    else
                    {
                        _device_agent->set_config_bool(SR_CONF_ZERO, false);
                        update_view_status();
                    }
                    return false;                    
                }
            }

            if (_device_agent->get_work_mode() == LOGIC && _view != NULL){
                if (_session->is_realtime_refresh())
                    _view->auto_set_max_scale();
            }

            _is_run_as_instant = false;
            bool ret = _session->start_capture(false);

            return ret;
        }

        void SamplingBar::on_instant_stop()
        {
            if (_instant_button.isVisible() == false){
                return;
            }
            _instant_button.setEnabled(false);
            QTimer::singleShot(10, this, &SamplingBar::on_instant_stop_action);
        }

        void SamplingBar::on_instant_stop_action()
        {
            action_instant_stop();
            _instant_button.setEnabled(true);
        }

        bool SamplingBar::action_instant_stop()
        { 
            if (_session->is_doing_action()){
                dsv_info("Task is busy.");
                return false;
            }

            if (_session->is_working()){ 
                return _session->stop_capture();
            }            
            
            if (_device_agent->have_instance() == false)
            {
                dsv_info("Error! Have no device, can't to collect data.");
                return false;
            }

            commit_settings();

            if (_device_agent->get_work_mode() == DSO)
            {
                bool zero;

                bool ret = _device_agent->get_config_bool(SR_CONF_ZERO, zero);
                if (ret && zero)
                {  
                    QString strMsg(tr("Auto Calibration program will be started. Don't connect any probes. \nIt can take a while!"));

                    if (MsgBox::Confirm(strMsg))
                    {
                        zero_adj();
                    }
                    else
                    {
                        _device_agent->set_config_bool(SR_CONF_ZERO, false);
                        update_view_status();
                    }
                    return false;            
                }
            }

            if (_device_agent->get_work_mode() == LOGIC && _session->is_realtime_refresh()){
                if (_view != NULL)
                    _view->auto_set_max_scale();
            }
            
            _is_run_as_instant = true;
            bool ret = _session->start_capture(true);

            return ret;  
        }

        void SamplingBar::on_device_selected()
        {
            if (_updating_device_list)
            {
                return;
            }
            if (_device_selector.currentIndex() == -1)
            {
                dsv_err("Have no selected device.");
                return;
            }
            _session->stop_capture();
            _session->session_save();

            ds_device_handle devHandle = (ds_device_handle)_device_selector.currentData().toULongLong();
            if (_session->have_hardware_data() && _session->is_first_store_confirm()){
                if (MsgBox::Confirm(tr("Save captured data?")))
                {
                    _updating_device_list = true;
                    _device_selector.setCurrentIndex(_last_device_index);
                    _updating_device_list = false;
                    _next_switch_device = devHandle; // Save end, auto switch to this device.
                    sig_store_session_data();
                    return;
                }
            }

            if (_session->set_device(devHandle)){
                _last_device_index = _device_selector.currentIndex();   
            }
            else{
                update_device_list(); // Reload the list.
            }
        }

        void SamplingBar::enable_toggle(bool enable)
        {
            bool test = false;

            if (_device_agent->have_instance())
            {
                _device_agent->get_config_bool(SR_CONF_TEST, test);
            }
            if (!test)
            {
                _sample_count.setDisabled(!enable);

                if (_device_agent->get_work_mode() == DSO)
                    _sample_rate.setDisabled(true);
                else
                    _sample_rate.setDisabled(!enable);
            }
            else
            {
                _sample_count.setDisabled(true);
                _sample_rate.setDisabled(true);
            }
        }

        void SamplingBar::reload()
        {
            QString iconPath = GetIconPath();
    int op_mode = -1;
    bool stream_cfg = false;
    const bool has_op_mode = _device_agent->get_config_int16(SR_CONF_OPERATION_MODE, op_mode);
    const bool has_stream_cfg = _device_agent->get_config_bool(SR_CONF_STREAM, stream_cfg);
    const bool stream_by_helper = _device_agent->is_stream_mode();
    dsv_info("SamplingBar::reload mode=%d dev=%s op_mode(valid=%d,val=%d) stream_cfg(valid=%d,val=%d) helper_stream=%d demo=%d",
             _device_agent->get_work_mode(),
             _device_agent->name().toUtf8().data(),
             (int)has_op_mode, op_mode,
             (int)has_stream_cfg, (int)stream_cfg,
             (int)stream_by_helper, (int)_device_agent->is_demo());

            // Disable Loop item by default; re-enable for LOGIC non-file devices.
            if (auto *m = qobject_cast<QStandardItemModel*>(_mode_button.model()))
                m->item(2)->setEnabled(false);

            int mode = _device_agent->get_work_mode();
            if (mode == LOGIC)
            {
                if (_device_agent->is_file()){
                    _mode_button.setVisible(false);
                }
                else
                {
                    update_mode_icon();
                    _mode_button.setVisible(true);

                    if (auto *m = qobject_cast<QStandardItemModel*>(_mode_button.model()))
                        m->item(2)->setEnabled(true);
                }
                _run_stop_button.setVisible(true);
                _instant_button.setVisible(true);      
            }
            else if (mode == ANALOG)
            {
                _mode_button.setVisible(false);
                _run_stop_button.setVisible(true);
                _instant_button.setVisible(false);
            }
            else if (mode == DSO)
            {
                _mode_button.setVisible(false);
                _run_stop_button.setVisible(true);
                _instant_button.setVisible(true);
            }
        
            retranslateUi();
            reStyle();
            update();
        if (auto *m = qobject_cast<QStandardItemModel*>(_mode_button.model()))
            dsv_info("SamplingBar::reload LoopItemEnabled=%d", (int)m->item(2)->isEnabled());
        }

        void SamplingBar::on_mode_changed(int index)
        {
            if (index == 0) {
                _session->set_collect_mode(COLLECT_SINGLE);
                if (_device_agent->is_demo()) {
                    _device_agent->set_config_string(SR_CONF_PATTERN_MODE, "protocol");
                    _session->broadcast_msg(DSV_MSG_DEMO_OPERATION_MODE_CHNAGED);
                }
            } else if (index == 1) {
                if (_device_agent->is_stream_mode() || _device_agent->is_demo()) {
                    _session->set_repeat_intvl(0.1);
                    _session->set_collect_mode(COLLECT_REPEAT);
                } else {
                    pv::dialogs::Interval interval_dlg(this);
                    interval_dlg.set_interval(_session->get_repeat_intvl());
                    interval_dlg.exec();
                    if (interval_dlg.is_done()) {
                        _session->set_repeat_intvl(interval_dlg.get_interval());
                        _session->set_collect_mode(COLLECT_REPEAT);
                    } else {
                        // Dialog cancelled — revert selection without triggering this slot again
                        update_mode_icon();
                        return;
                    }
                }
                if (_device_agent->is_demo()) {
                    _device_agent->set_config_string(SR_CONF_PATTERN_MODE, "random");
                    _session->broadcast_msg(DSV_MSG_DEMO_OPERATION_MODE_CHNAGED);
                }
            } else if (index == 2) {
                _session->set_collect_mode(COLLECT_LOOP);
                if (_device_agent->is_demo()) {
                    _device_agent->set_config_string(SR_CONF_PATTERN_MODE, "random");
                    _session->broadcast_msg(DSV_MSG_DEMO_OPERATION_MODE_CHNAGED);
                }
            }
            update_mode_icon();
        }

        void SamplingBar::update_device_list()
        {
            struct ds_device_base_info *array = NULL;
            int dev_count = 0;
            int select_index = 0;

            dsv_info("Update device list.");

            array = _session->get_device_list(dev_count, select_index);

            if (array == NULL)
            {
                dsv_err("Get deivce list error!");
                return;
            }

            _updating_device_list = true;
            struct ds_device_base_info *p = NULL;
            ds_device_handle    cur_dev_handle = NULL_HANDLE;

            _device_selector.clear();

            for (int i = 0; i < dev_count; i++)
            {
                p = (array + i);
                _device_selector.addItem(QString(p->name), QVariant::fromValue((unsigned long long)p->handle));
                
                if (i == select_index)
                    cur_dev_handle = p->handle;
            }
            free(array);

            _device_selector.setCurrentIndex(select_index);

            if (cur_dev_handle != _last_device_handle){                
                update_sample_rate_list();
                _last_device_handle = cur_dev_handle;                
            }

            _last_device_index = select_index;
            int width = _device_selector.sizeHint().width();
            _device_selector.setFixedWidth(min(width + 15, _device_selector.maximumWidth()));
            _device_selector.view()->setMinimumWidth(width + 30);

            _updating_device_list = false;
        }

        void SamplingBar::config_device()
        {
            on_configure();
        }

        void SamplingBar::update_view_status()
        {
            // Not safe to query hardware state before a device is connected
            if (!_device_agent->have_instance())
                return;

            int bEnable = _session->is_working() == false;
            int mode = _session->get_device()->get_work_mode();
            int op_mode = -1;
            bool stream_cfg = false;
            const bool has_op_mode = _session->get_device()->get_config_int16(SR_CONF_OPERATION_MODE, op_mode);
            const bool has_stream_cfg = _session->get_device()->get_config_bool(SR_CONF_STREAM, stream_cfg);

            _device_type.setEnabled(bEnable);
            _mode_button.setEnabled(bEnable);
            _configure_button.setEnabled(bEnable);
            _device_selector.setEnabled(bEnable);
            // Disable Loop item by default; re-enable for LOGIC non-file devices.
            if (auto *m = qobject_cast<QStandardItemModel*>(_mode_button.model()))
                m->item(2)->setEnabled(false);

            if (_session->get_device()->is_file()){
                _sample_rate.setEnabled(false);
                _sample_count.setEnabled(false);
            }
            else if (mode == DSO){
                _sample_rate.setEnabled(false);
                _sample_count.setEnabled(bEnable);

                if (_session->is_working() && _session->is_instant() == false)
                {
                    _sample_count.setEnabled(true);
                }
            }
            else{
                _sample_rate.setEnabled(bEnable);
                _sample_count.setEnabled(bEnable);

                if (mode == LOGIC && _session->get_device()->is_hardware())
                {
                    int mode_val = 0;
                    if (_session->get_device()->get_config_int16(SR_CONF_OPERATION_MODE, mode_val)){
                        if (mode_val == LO_OP_INTEST){
                            _sample_rate.setEnabled(false);
                            _sample_count.setEnabled(false);
                        }
                    }
                }

                if (mode == LOGIC && _device_agent->is_file() == false){
                    if (auto *m = qobject_cast<QStandardItemModel*>(_mode_button.model()))
                        m->item(2)->setEnabled(true);
                }
            }

            if (auto *m = qobject_cast<QStandardItemModel*>(_mode_button.model()))
                dsv_info("SamplingBar::update_view_status mode=%d op_mode(valid=%d,val=%d) stream_cfg(valid=%d,val=%d) helper_stream=%d demo=%d LoopItemEnabled=%d",
                         mode,
                         (int)has_op_mode, op_mode,
                         (int)has_stream_cfg, (int)stream_cfg,
                         (int)_device_agent->is_stream_mode(),
                         (int)_device_agent->is_demo(),
                         (int)m->item(2)->isEnabled());

            if (_session->is_working()){
                if (_is_run_as_instant)
                    _run_stop_button.setEnabled(false);
                else
                    _instant_button.setEnabled(false);
            } else {
                _run_stop_button.setEnabled(true);
                _instant_button.setEnabled(true);                
            }
 
            QString iconPath = GetIconPath();

            if (_is_run_as_instant)
                _instant_button.setIcon(!bEnable ? QIcon(iconPath + "/stop.svg") : QIcon(iconPath + "/single.svg"));
            else
                _run_stop_button.setIcon(!bEnable ? QIcon(iconPath + "/stop.svg") : QIcon(iconPath + "/start.svg"));

            if (!bEnable && !_is_run_as_instant) {
                _run_stop_button.setStyleSheet(
                    "QToolButton { background-color: rgba(220,38,38,0.2);"
                    "  border: 1px solid #dc2626; border-radius: 4px;"
                    "  color: #f87171; font-size: 12px; font-weight: 500;"
                    "  padding: 0px 16px; }"
                    "QToolButton:hover { background-color: rgba(220,38,38,0.3);"
                    "  border-color: #ef4444; color: #fca5a5; }");
            } else {
                _run_stop_button.setStyleSheet(
                    "QToolButton { background-color: rgba(147,51,234,0.2);"
                    "  border: 1px solid #9333ea; border-radius: 4px;"
                    "  color: #c084fc; font-size: 12px; font-weight: 500;"
                    "  padding: 0px 16px; }"
                    "QToolButton:hover { background-color: rgba(147,51,234,0.3);"
                    "  border-color: #a855f7; color: #e9d5ff; }");
            }

            if (_is_run_as_instant && !bEnable) {
                _instant_button.setStyleSheet(
                    "QToolButton { background-color: rgba(220,38,38,0.2);"
                    "  border: 1px solid #dc2626; border-radius: 4px;"
                    "  color: #f87171; font-size: 12px; font-weight: 500;"
                    "  padding: 0px 16px; }"
                    "QToolButton:hover { background-color: rgba(220,38,38,0.3);"
                    "  border-color: #ef4444; color: #fca5a5; }");
            } else {
                _instant_button.setStyleSheet(
                    "QToolButton { background-color: rgba(22,163,74,0.1);"
                    "  border: 1px solid rgba(22,163,74,0.5); border-radius: 4px;"
                    "  color: #4ade80; font-size: 12px; font-weight: 500;"
                    "  padding: 0px 16px; }"
                    "QToolButton:hover { background-color: rgba(22,163,74,0.2);"
                    "  border-color: #16a34a; color: #86efac; }"
                    "QToolButton:disabled { background-color: rgba(80,80,80,0.1);"
                    "  border-color: rgba(100,100,100,0.4); color: #555555; }");
            }

            retranslateUi();

            if (bEnable){
                _is_run_as_instant = false;
            }

            update_mode_icon();
            update_triggers_toggle_btn();

            if (_session->get_device()->is_demo() && bEnable)
            {
                QString opt_mode = _device_agent->get_demo_operation_mode();

                if (opt_mode != "random" && mode == LOGIC){
                    _sample_rate.setEnabled(false);
                    _sample_count.setEnabled(false);
                }
            }
        }

        ds_device_handle SamplingBar::get_next_device_handle()
        {
            ds_device_handle h = _next_switch_device;
            _next_switch_device = NULL_HANDLE;
            return h;
        }

        void SamplingBar::update_mode_icon()
        {
            QSignalBlocker blocker(_mode_button);
            if (_session->is_repeat_mode())
                _mode_button.setCurrentIndex(1);
            else if (_session->is_loop_mode())
                _mode_button.setCurrentIndex(2);
            else
                _mode_button.setCurrentIndex(0);
        }

        void SamplingBar::run_or_stop()
        {
            on_run_stop();
        }

        void SamplingBar::run_or_stop_instant()
        {
            on_instant_stop();
        }

        void SamplingBar::UpdateLanguage()
        {
            retranslateUi();
        }

        void SamplingBar::UpdateTheme()
        {
            reStyle();
        }

        void SamplingBar::UpdateFont()
        {  
            QFont font = this->font();
            font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
            if (_capture_strip != nullptr)
                ui::set_toolbar_font(_capture_strip, font);
            else
                ui::set_toolbar_font(this, font);

            update_view_status();
        }

        void SamplingBar::set_sample_count_index(int index)
        {
            _sample_count.setCurrentIndex(index);
        }

        void SamplingBar::on_triggers_toggle()
        {
            if (_view) {
                _view->set_triggers_visible(!_view->is_triggers_visible());
                update_triggers_toggle_btn();
                _view->update();
            }
        }

        void SamplingBar::update_triggers_toggle_btn()
        {
            QString iconPath = GetIconPath();
            bool visible = _view && _view->is_triggers_visible();
            QString iconFile = visible ? "/trigger-eye.svg" : "/trigger-eye-off.svg";
            _triggers_toggle_btn.setIcon(QIcon(iconPath + iconFile));
        }

    } // namespace toolbars
} // namespace pv
