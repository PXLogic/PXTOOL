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

#include "deviceoptionsdock.h"

#include <QLabel>
#include <QRadioButton>
#include <QTabWidget>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QScreen>
#include <assert.h>

// ---------------------------------------------------------------------------
// Lightweight flex-wrap container for ChannelLabel items.
// Items are placed left-to-right with fixed spacing; when a row is full they
// wrap onto the next row. The widget resizes its own height automatically.
// ---------------------------------------------------------------------------
class ChannelFlowWidget : public QWidget
{
public:
    explicit ChannelFlowWidget(int spacing, QWidget *parent = nullptr)
        : QWidget(parent), _spacing(spacing)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    void addItem(QWidget *w)
    {
        w->setParent(this);
        _items.push_back(w);
    }

    QSize sizeHint() const override { return minimumSizeHint(); }

    QSize minimumSizeHint() const override
    {
        int h = doLayout(QRect(0, 0, qMax(width(), 1), 0), false);
        return QSize(0, h);
    }

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        int h = doLayout(contentsRect(), true);
        setFixedHeight(qMax(h, 1));
    }

private:
    // Performs (or simulates) the flow layout. Returns the total height needed.
    // If apply=true, moves the child widgets to their computed positions.
    int doLayout(const QRect &r, bool apply) const
    {
        if (_items.empty()) return 0;

        int x = r.x();
        int y = r.y();
        int rowH = 0;

        for (QWidget *w : _items) {
            QSize s = w->sizeHint();
            if (s.isEmpty()) s = w->size();

            // Wrap if we exceed the right boundary (but always place at least
            // one item per row to avoid infinite loops with very narrow panels).
            if (x != r.x() && x + s.width() > r.x() + r.width()) {
                x = r.x();
                y += rowH + _spacing;
                rowH = 0;
            }

            if (apply)
                w->move(x, y);

            x    += s.width() + _spacing;
            rowH  = qMax(rowH, s.height());
        }

        return y + rowH - r.y();
    }

    int                   _spacing;
    std::vector<QWidget*> _items;
};

#include "../prop/property.h"
#include "../config/appconfig.h"
#include "../appcontrol.h"
#include "../sigsession.h"
#include "../ui/langresource.h"
#include "../log.h"
#include "../ui/msgbox.h"

using namespace std;

namespace pv {
namespace dock {

DeviceOptionsDock::DeviceOptionsDock(QWidget *parent, SigSession *session)
    : QScrollArea(parent)
    , _session(session)
    , _opt_mode(0)
    , _groupHeight1(0)
    , _groupHeight2(0)
    , _isBuilding(false)
    , _cur_analog_tag_index(0)
    , _content_built(false)
    , _dynamic_panel(nullptr)
    , _container_panel(nullptr)
    , _container_lay(nullptr)
    , _inner(nullptr)
    , _inner_lay(nullptr)
    , _device_options_binding(nullptr)
{
    _device_agent = session->get_device();

    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet("QScrollArea { border: none; }");

    _inner = new QWidget(this);
    _inner->setObjectName("deviceOptionsWidget");
    _inner_lay = new QVBoxLayout(_inner);
    _inner_lay->setContentsMargins(0, 0, 0, 0);
    _inner_lay->setSpacing(0);
    setWidget(_inner);

    ADD_UI(this);
}

DeviceOptionsDock::~DeviceOptionsDock()
{
    _mode_check_timer.stop();
    delete _device_options_binding;
    _device_options_binding = nullptr;
    REMOVE_UI(this);
}

void DeviceOptionsDock::setSession(SigSession *session)
{
    if (_session == session)
        return;
    dsv_info("DeviceOptionsDock::setSession session@%p _content_built=%d", (void*)session, (int)_content_built);
    _session = session;
    _device_agent = session->get_device();
    _mode_check_timer.stop();
    if (_content_built) {
        _content_built = false;
        // Defer rebuild to next event-loop tick so rebind_device() completes first,
        // ensuring channel states read from the driver are current.
        QTimer::singleShot(0, this, [this]() {
            dsv_info("DeviceOptionsDock::setSession singleShot: _content_built=%d have_instance=%d",
                     (int)_content_built, (int)_device_agent->have_instance());
            if (_content_built)
                return; // panel_shown() already rebuilt it
            if (_device_agent->have_instance()) {
                build_content();
                _content_built = true;
                if (!_mode_check_timer.isActive()) {
                    _mode_check_timer.setInterval(100);
                    _mode_check_timer.start();
                }
            }
            // If device not ready, panel_shown() will build when tab is opened.
        });
    } else {
        dsv_info("DeviceOptionsDock::setSession: _content_built=false, skipping singleShot");
    }
}

void DeviceOptionsDock::rebuild()
{
    dsv_info("DeviceOptionsDock::rebuild: _content_built=%d have_instance=%d",
             (int)_content_built, (int)_device_agent->have_instance());
    if (_content_built)
        return; // already built (singleShot or panel_shown() got here first)
    if (!_device_agent->have_instance())
        return; // device still not ready
    build_content();
    _content_built = true;
    if (!_mode_check_timer.isActive()) {
        _mode_check_timer.setInterval(100);
        _mode_check_timer.start();
    }
}

void DeviceOptionsDock::on_device_changed()
{
    _device_agent = _session->get_device();
    _mode_check_timer.stop();
    _content_built = false;

    // Defer to the next event-loop tick so that device_updated() and
    // on_load_config_end() in the mainwindow handler complete before we
    // query or modify channel state inside build_content().
    QTimer::singleShot(0, this, [this]() {
        if (_content_built)
            return; // panel_shown() already rebuilt it
        if (!_device_agent->have_instance())
            return;
        build_content();
        _content_built = true;
        if (!_mode_check_timer.isActive()) {
            _mode_check_timer.setInterval(100);
            _mode_check_timer.start();
        }
    });
}

void DeviceOptionsDock::panel_shown()
{
    // Refresh device agent in case it changed
    _device_agent = _session->get_device();

    if (!_content_built) {
        build_content();
        _content_built = true;
    }

    if (!_mode_check_timer.isActive()) {
        _mode_check_timer.setInterval(100);
        _mode_check_timer.start();
    }
}

void DeviceOptionsDock::build_content()
{
    // Clear previous content if any
    QLayoutItem *item;
    while ((item = _inner_lay->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    _probes_checkBox_list.clear();
    _probe_options_binding_list.clear();
    _channel_mode_indexs.clear();
    _dso_channel_list.clear();
    _dynamic_panel = nullptr;

    // (Re-)create the device options binding now that the device is ready
    delete _device_options_binding;
    _device_options_binding = new pv::prop::binding::DeviceOptions();

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    // container panel
    _container_panel = new QWidget(_inner);
    _container_lay = new QVBoxLayout(_container_panel);
    _container_lay->setDirection(QBoxLayout::TopToBottom);
    _container_lay->setAlignment(Qt::AlignTop);
    _container_lay->setContentsMargins(12, 8, 12, 8);
    _container_lay->setSpacing(5);

    // Mode group box
    QGroupBox *props_box = new QGroupBox(
        tr("Mode"), _container_panel);
    props_box->setFont(font);
    props_box->setMinimumHeight(70);
    props_box->setAlignment(Qt::AlignTop);
    QLayout *props_lay = get_property_form(props_box);
    props_lay->setContentsMargins(5, 20, 5, 5);
    props_box->setLayout(props_lay);
    _container_lay->addWidget(props_box);

    QWidget *minWid = new QWidget();
    minWid->setFixedHeight(1);
    minWid->setMinimumWidth(230);
    _container_lay->addWidget(minWid);

    // Channel / dynamic panel
    build_dynamic_panel();

    // Stretch
    _container_lay->addStretch(1);

    // Bottom divider
    auto *bot_sep = new QWidget(_inner);
    bot_sep->setObjectName("device_options_divider");
    bot_sep->setFixedHeight(1);

    // Apply button
    auto *apply_btn = new QPushButton(tr("Apply"), _inner);
    apply_btn->setObjectName("device_ok_btn");
    apply_btn->setMinimumWidth(60);
    auto *footer_lay = new QHBoxLayout();
    footer_lay->setContentsMargins(12, 10, 12, 10);
    footer_lay->addStretch();
    footer_lay->addWidget(apply_btn);

    _inner_lay->addWidget(_container_panel, 1);
    _inner_lay->addWidget(bot_sep);
    _inner_lay->addLayout(footer_lay);

    _device_agent->get_config_int16(SR_CONF_OPERATION_MODE, _opt_mode);
    if (_device_agent->is_demo())
        _demo_operation_mode = _device_agent->get_demo_operation_mode();

    connect(&_mode_check_timer, SIGNAL(timeout()), this, SLOT(mode_check_timeout()), Qt::UniqueConnection);
    connect(apply_btn, SIGNAL(clicked()), this, SLOT(on_apply()));
}

// ---------------------------------------------------------------------------
// Apply / commit
// ---------------------------------------------------------------------------

void DeviceOptionsDock::on_apply()
{
    bool hasEnabled = false;

    // Commit device properties
    const auto &dev_props = _device_options_binding->properties();
    for (auto p : dev_props)
        p->commit();

    int op_mode = -1;
    bool stream_cfg = false;
    const bool has_op_mode = _device_agent->get_config_int16(SR_CONF_OPERATION_MODE, op_mode);
    const bool has_stream_cfg = _device_agent->get_config_bool(SR_CONF_STREAM, stream_cfg);
    dsv_info("DeviceOptionsDock::on_apply after commit op_mode(valid=%d,val=%d) stream_cfg(valid=%d,val=%d) helper_stream=%d",
             (int)has_op_mode, op_mode,
             (int)has_stream_cfg, (int)stream_cfg,
             (int)_device_agent->is_stream_mode());

    // Commit probe enabled state; track which probe indices went from
    // enabled -> disabled so we can selectively notify listeners (the
    // ProtocolDock used to nuke every decoder on any Apply, which deleted
    // decoders even when the user was only ENABLING more channels).
    QSet<int> disabled_channels;
    int mode = _device_agent->get_work_mode();
    if (mode == LOGIC || mode == ANALOG) {
        int index = 0;
        for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            assert(probe);
            const bool was_enabled = probe->enabled;
            const bool will_enable = _probes_checkBox_list.at(index)->isChecked();
            probe->enabled = will_enable;
            if (was_enabled && !will_enable)
                disabled_channels.insert(probe->index);
            index++;
            if (probe->enabled)
                hasEnabled = true;
        }
    } else {
        hasEnabled = true;
    }

    if (hasEnabled) {
        auto it = _probe_options_binding_list.begin();
        while (it != _probe_options_binding_list.end()) {
            for (auto p : (*it)->properties())
                p->commit();
            ++it;
        }

        // Rebuild the session signal list from the driver's (possibly updated)
        // channel list, then notify the view to re-render. This is necessary
        // when channel mode changes (e.g. "Use 6 Channels") because the driver
        // updates sdi->channels immediately on radio-button press but the session
        // signal list is not refreshed until we do so explicitly here.
        _session->init_signals();
        // Notify the view to redraw with the new (cleared) signal list.
        // Without this the old waveform rendering persists even though the
        // internal data buffers were already cleared by init_signals().
        _session->notify_signals_changed();
        // Notify listeners (ProtocolDock) so they can drop just the decoders
        // that now reference a disabled channel. An empty set means the user
        // only enabled new channels — decoders must be preserved in that case.
        emit sig_channels_applied(disabled_channels);
    } else {
        QString strMsg(tr("All channel disabled! Please enable at least one channel."));
        MsgBox::Show(strMsg);
    }
}

// ---------------------------------------------------------------------------
// Property form (Mode group box)
// ---------------------------------------------------------------------------

QLayout *DeviceOptionsDock::get_property_form(QWidget *parent)
{
    QGridLayout *const layout = new QGridLayout(parent);
    layout->setVerticalSpacing(2);
    const auto &properties = _device_options_binding->properties();

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    int i = 0;
    for (auto p : properties) {
        const QString label = p->labeled_widget() ? QString() : p->label();
        QString label_text;
        if (!label.isEmpty()) {
            QByteArray bytes = label.toLocal8Bit();
            const char *lang_str = LangResource::Instance()->get_lang_text(
                STR_PAGE_DSL, bytes.data(), bytes.data());
            label_text = QString(lang_str);
        }

        QLabel *lb = new QLabel(label_text, parent);
        lb->setFont(font);
        layout->addWidget(lb, i, 0);

        QWidget *wid = (label == QString("Operation Mode"))
            ? p->get_widget(parent, true)
            : p->get_widget(parent);
        wid->setFont(font);
        layout->addWidget(wid, i, 1);
        layout->setRowMinimumHeight(i, 22);
        i++;
    }

    _groupHeight1 = parent->sizeHint().height() + 30;
    parent->setFixedHeight(_groupHeight1);
    return layout;
}

// ---------------------------------------------------------------------------
// Logic probes
// ---------------------------------------------------------------------------

void DeviceOptionsDock::logic_probes(QVBoxLayout &layout)
{
    layout.setSpacing(2);

    int row1 = 0;
    int row2 = 0;
    int vld_ch_num = 0;
    int cur_ch_num = 0;
    int contentHeight = 0;

    _probes_checkBox_list.clear();

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    if (_device_agent->get_work_mode() == LOGIC) {
        GVariant *gvar_opts = _device_agent->get_config_list(NULL, SR_CONF_CHANNEL_MODE);
        if (gvar_opts != NULL) {
            struct sr_list_item *plist =
                (struct sr_list_item*)g_variant_get_uint64(gvar_opts);
            g_variant_unref(gvar_opts);

            int ch_mode = 0;
            _device_agent->get_config_int16(SR_CONF_CHANNEL_MODE, ch_mode);
            _channel_mode_indexs.clear();

            while (plist != NULL && plist->id >= 0) {
                row1++;
                QString mode_bt_text = LangResource::Instance()->get_lang_text(
                    STR_PAGE_DSL, plist->name, plist->name);
                QRadioButton *mode_button = new QRadioButton(mode_bt_text);
                mode_button->setFont(font);
                ChannelModePair mode_index;
                mode_index.key   = mode_button;
                mode_index.value = plist->id;
                _channel_mode_indexs.push_back(mode_index);
                layout.addWidget(mode_button);
                contentHeight += mode_button->sizeHint().height();
                connect(mode_button, SIGNAL(pressed()), this, SLOT(channel_check()));
                if (plist->id == ch_mode)
                    mode_button->setChecked(true);
                plist++;
            }
        }
    }

    _device_agent->get_config_int16(SR_CONF_VLD_CH_NUM, vld_ch_num);

    // Use a flex-wrap flow panel so items reflow when the dock is resized.
    const int CH_SPACING = 4;
    ChannelFlowWidget *channel_pannel = new ChannelFlowWidget(CH_SPACING);

    int channel_line_height = 0;
    row2++;

    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        if (probe->enabled)
            cur_ch_num++;
        if (cur_ch_num > vld_ch_num)
            probe->enabled = false;

        ChannelLabel *ch_item = new ChannelLabel(this, NULL, probe->index);
        channel_pannel->addItem(ch_item);
        _probes_checkBox_list.push_back(ch_item->getCheckBox());
        ch_item->getCheckBox()->setCheckState(probe->enabled ? Qt::Checked : Qt::Unchecked);
        channel_line_height = ch_item->height();
    }
    layout.addWidget(channel_pannel);

    QWidget *space = new QWidget();
    space->setFixedHeight(10);
    layout.addWidget(space);
    contentHeight += 10;

    QHBoxLayout *line_lay = new QHBoxLayout();
    layout.addLayout(line_lay);
    line_lay->setSpacing(10);

    QPushButton *enable_all  = new QPushButton(
        tr("Enable All"),  this);
    QPushButton *disable_all = new QPushButton(
        tr("Disable All"), this);
    enable_all->setObjectName("device_ch_btn");
    disable_all->setObjectName("device_ch_btn");
    enable_all->setMaximumHeight(33);
    disable_all->setMaximumHeight(33);
    enable_all->setFont(font);
    disable_all->setFont(font);

    connect(enable_all,  SIGNAL(clicked()), this, SLOT(enable_all_probes()));
    connect(disable_all, SIGNAL(clicked()), this, SLOT(disable_all_probes()));

    line_lay->addWidget(enable_all);
    line_lay->addWidget(disable_all);

    const int hPad = 28;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    enable_all->setMinimumWidth(
        enable_all->fontMetrics().horizontalAdvance(enable_all->text()) + hPad);
    disable_all->setMinimumWidth(
        disable_all->fontMetrics().horizontalAdvance(disable_all->text()) + hPad);
#else
    enable_all->setMinimumWidth(
        enable_all->fontMetrics().width(enable_all->text()) + hPad);
    disable_all->setMinimumWidth(
        disable_all->fontMetrics().width(disable_all->text()) + hPad);
#endif

    contentHeight += enable_all->sizeHint().height();
    // The flow widget determines its own height dynamically; just provide a
    // reasonable minimum so the panel is never too short.
    const int estimated_rows = qMax(2, row2);
    contentHeight += (channel_line_height + CH_SPACING) * estimated_rows + 50;
    _groupHeight2  = contentHeight + (row1 + row2) * 2 + 38;

#ifdef Q_OS_DARWIN
    _groupHeight2 += 5;
#endif

    // Use setMinimumHeight so the flow widget can expand beyond the estimate
    // when items wrap onto more rows.
    _dynamic_panel->setMinimumHeight(_groupHeight2);
    _dynamic_panel->setMaximumHeight(QWIDGETSIZE_MAX);
    _dynamic_panel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

// ---------------------------------------------------------------------------
// Analog probes
// ---------------------------------------------------------------------------

void DeviceOptionsDock::analog_probes(QGridLayout &layout)
{
    _probes_checkBox_list.clear();
    _probe_options_binding_list.clear();
    _dso_channel_list.clear();

    QTabWidget *tabWidget = new QTabWidget();
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setUsesScrollButtons(false);

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    int ch_dex = 0;
    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);
        _dso_channel_list.push_back(probe);

        QWidget *probe_widget = new QWidget(tabWidget);
        QGridLayout *probe_layout = new QGridLayout(probe_widget);
        probe_widget->setLayout(probe_layout);

        bool ch_enabled = probe->enabled;
        if (ch_dex < (int)_lst_probe_enabled_status.size())
            ch_enabled = _lst_probe_enabled_status[ch_dex];
        ch_dex++;

        QCheckBox *probe_checkBox = new QCheckBox(this);
        QVariant vlayout = QVariant::fromValue((void*)probe_layout);
        probe_checkBox->setProperty("Layout", vlayout);
        probe_checkBox->setProperty("Enable", true);
        probe_checkBox->setChecked(ch_enabled);
        _probes_checkBox_list.push_back(probe_checkBox);

        QLabel *en_label = new QLabel(
            tr("Enable: "), this);
        en_label->setFont(font);
        en_label->setProperty("Enable", true);
        probe_layout->addWidget(en_label,        0, 0, 1, 1);
        probe_layout->addWidget(probe_checkBox,  0, 1, 1, 3);

        auto *probe_options_binding = new pv::prop::binding::ProbeOptions(probe);
        const auto &properties = probe_options_binding->properties();
        int i = 1;
        for (auto p : properties) {
            const QString label = p->labeled_widget() ? QString() : p->label();
            QLabel *lb = new QLabel(label, probe_widget);
            lb->setFont(font);
            probe_layout->addWidget(lb, i, 0, 1, 1);

            QWidget *pow = p->get_widget(probe_widget);
            pow->setEnabled(probe_checkBox->isChecked());
            pow->setFont(font);

            if (p->name().contains("Map Default")) {
                pow->setProperty("index", probe->index);
                connect(pow, SIGNAL(clicked()), this, SLOT(analog_channel_check()));
            } else if (probe_checkBox->isChecked() && p->name().contains("Map")) {
                bool map_default = true;
                _device_agent->get_config_bool(SR_CONF_PROBE_MAP_DEFAULT,
                                               map_default, probe, NULL);
                if (map_default)
                    pow->setEnabled(false);
                pow->setObjectName("map-row");
            }
            probe_layout->addWidget(pow, i, 1, 1, 3);
            i++;
        }
        _probe_options_binding_list.push_back(probe_options_binding);
        connect(probe_checkBox, SIGNAL(released()), this, SLOT(on_analog_channel_enable()));

        QString tabName = QString::fromUtf8(probe->name);
        tabName += " ";
        tabWidget->addTab(probe_widget, tabName);
    }

    layout.addWidget(tabWidget, 0, 0, 1, 1);
    _groupHeight2 = tabWidget->sizeHint().height() + 50;
    _dynamic_panel->setFixedHeight(_groupHeight2);
    connect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(on_anlog_tab_changed(int)));
    tabWidget->setCurrentIndex(_cur_analog_tag_index);
}

// ---------------------------------------------------------------------------
// Dynamic widget / build_dynamic_panel
// ---------------------------------------------------------------------------

QString DeviceOptionsDock::dynamic_widget(QLayout *lay)
{
    int mode = _device_agent->get_work_mode();
    if (mode == LOGIC) {
        QVBoxLayout *grid = dynamic_cast<QVBoxLayout*>(lay);
        assert(grid);
        logic_probes(*grid);
        return tr("Channel");
    } else if (mode == DSO) {
        bool have_zero;
        if (_device_agent->get_config_bool(SR_CONF_HAVE_ZERO, have_zero)) {
            QGridLayout *grid = dynamic_cast<QGridLayout*>(lay);
            assert(grid);

            QFont font = this->font();
            font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

            if (have_zero) {
                auto config_button = new QPushButton(
                    tr("Auto Calibration"), this);
                config_button->setFont(font);
                grid->addWidget(config_button, 0, 0, 1, 1);
                connect(config_button, SIGNAL(clicked()), this, SLOT(zero_adj()));

                auto cali_button = new QPushButton(
                    tr("Manual Calibration"), this);
                cali_button->setFont(font);
                grid->addWidget(cali_button, 1, 0, 1, 1);
                connect(cali_button, SIGNAL(clicked()), this, SLOT(on_calibration()));

                config_button->setFixedHeight(35);
                cali_button->setFixedHeight(35);
                _groupHeight2 = 135;
                _dynamic_panel->setFixedHeight(_groupHeight2);
                return tr("Calibration");
            }
        }
    } else if (mode == ANALOG) {
        QGridLayout *grid = dynamic_cast<QGridLayout*>(lay);
        assert(grid);
        analog_probes(*grid);
        return tr("Channel");
    }
    return QString();
}

void DeviceOptionsDock::build_dynamic_panel()
{
    _isBuilding = true;

    if (_dynamic_panel != NULL) {
        // Remove from layout first so the slot is freed synchronously,
        // then delete; deleteLater() would leave a ghost slot and cause
        // the replacement to be appended after the stretch spacer.
        _container_lay->removeWidget(_dynamic_panel);
        delete _dynamic_panel;
        _dynamic_panel = NULL;
    }

    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);

    _dynamic_panel = new QGroupBox("group", _container_panel);
    _dynamic_panel->setFont(font);

    // Insert before the trailing stretch spacer so the Channel group always
    // sits immediately below the Options group.  On the first call from
    // build_content() there is no spacer yet, so this reduces to addWidget.
    int insertIdx = _container_lay->count();
    for (int i = _container_lay->count() - 1; i >= 0; --i) {
        if (_container_lay->itemAt(i) && _container_lay->itemAt(i)->spacerItem()) {
            insertIdx = i;
            break;
        }
    }
    _container_lay->insertWidget(insertIdx, _dynamic_panel);

    if (_device_agent->get_work_mode() == LOGIC)
        _dynamic_panel->setLayout(new QVBoxLayout());
    else
        _dynamic_panel->setLayout(new QGridLayout());

    QString title = dynamic_widget(_dynamic_panel->layout());
    QGroupBox *box = dynamic_cast<QGroupBox*>(_dynamic_panel);
    box->setFont(font);
    box->setTitle(title);
    if (title.isEmpty())
        box->setVisible(false);

    _dynamic_panel->layout()->setContentsMargins(5, 20, 5, 5);
    _isBuilding = false;
}

// ---------------------------------------------------------------------------
// Probe helper slots
// ---------------------------------------------------------------------------

void DeviceOptionsDock::ChannelChecked(int index, QObject *object)
{
    (void)index;
    QCheckBox *sc = dynamic_cast<QCheckBox*>(object);
    channel_checkbox_clicked(sc);
}

void DeviceOptionsDock::set_all_probes(bool set)
{
    for (auto box : _probes_checkBox_list)
        box->setCheckState(set ? Qt::Checked : Qt::Unchecked);
}

void DeviceOptionsDock::enable_max_probes()
{
    int cur_ch_num = 0;
    for (auto box : _probes_checkBox_list)
        if (box->isChecked()) cur_ch_num++;

    int vld_ch_num;
    if (!_device_agent->get_config_int16(SR_CONF_VLD_CH_NUM, vld_ch_num))
        return;

    while (cur_ch_num < vld_ch_num && cur_ch_num < (int)_probes_checkBox_list.size()) {
        auto box = _probes_checkBox_list[cur_ch_num];
        if (!box->isChecked()) {
            box->setChecked(true);
            cur_ch_num++;
        }
    }
}

void DeviceOptionsDock::enable_all_probes()
{
    bool stream_mode;
    if (_device_agent->get_config_bool(SR_CONF_STREAM, stream_mode)) {
        if (stream_mode) {
            enable_max_probes();
            return;
        }
    }
    set_all_probes(true);
}

void DeviceOptionsDock::disable_all_probes()
{
    set_all_probes(false);
}

void DeviceOptionsDock::channel_checkbox_clicked(QCheckBox *sc)
{
    if (_device_agent->get_work_mode() == LOGIC) {
        if (sc == NULL || !sc->isChecked())
            return;
        bool stream_mode;
        if (!_device_agent->get_config_bool(SR_CONF_STREAM, stream_mode))
            return;
        if (!stream_mode)
            return;

        int cur_ch_num = 0;
        for (auto box : _probes_checkBox_list)
            if (box->isChecked()) cur_ch_num++;

        int vld_ch_num;
        if (!_device_agent->get_config_int16(SR_CONF_VLD_CH_NUM, vld_ch_num))
            return;

        if (cur_ch_num > vld_ch_num) {
            QString msg_str(tr("max count of channels!"));
            msg_str = msg_str.replace("{0}", QString::number(vld_ch_num));
            MsgBox::Show(msg_str);
            sc->setChecked(false);
        }
    } else if (_device_agent->get_work_mode() == ANALOG) {
        if (sc != NULL) {
            QGridLayout *const layout =
                (QGridLayout*)sc->property("Layout").value<void*>();
            int i = layout->count();

            int ck_index = -1, i_dex = 0;
            bool map_default = false;
            for (auto ck : _probes_checkBox_list) {
                if (ck == sc) { ck_index = i_dex; break; }
                i_dex++;
            }
            if (ck_index != -1)
                _device_agent->get_config_bool(SR_CONF_PROBE_MAP_DEFAULT,
                                               map_default,
                                               _dso_channel_list[ck_index], NULL);
            while (i--) {
                QWidget *w = layout->itemAt(i)->widget();
                if (w->property("Enable").isNull()) {
                    if (map_default && w->objectName() == "map-row")
                        w->setEnabled(false);
                    else
                        w->setEnabled(sc->isChecked());
                }
            }
        }
    }
}

void DeviceOptionsDock::channel_check()
{
    QRadioButton *bt = dynamic_cast<QRadioButton*>(sender());
    assert(bt);
    int mode_index = -1;
    for (auto p : _channel_mode_indexs) {
        if (p.key == bt) { mode_index = p.value; break; }
    }
    assert(mode_index >= 0);
    _device_agent->set_config_int16(SR_CONF_CHANNEL_MODE, mode_index);
    build_dynamic_panel();
}

void DeviceOptionsDock::analog_channel_check()
{
    QCheckBox *sc = dynamic_cast<QCheckBox*>(sender());
    if (sc != NULL) {
        for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            if (sc->property("index").toInt() == probe->index)
                _device_agent->set_config_bool(SR_CONF_PROBE_MAP_DEFAULT, sc->isChecked(), probe);
        }
    }
    _lst_probe_enabled_status.clear();
    for (auto ck : _probes_checkBox_list)
        _lst_probe_enabled_status.push_back(ck->isChecked());
    build_dynamic_panel();
}

void DeviceOptionsDock::on_analog_channel_enable()
{
    QCheckBox *sc = dynamic_cast<QCheckBox*>(sender());
    channel_checkbox_clicked(sc);
}

void DeviceOptionsDock::on_anlog_tab_changed(int index)
{
    _cur_analog_tag_index = index;
}

void DeviceOptionsDock::zero_adj()
{
    QString strMsg(tr("Auto Calibration program will be started. Don't connect any probes. \nIt can take a while!"));
    if (MsgBox::Confirm(strMsg))
        _device_agent->set_config_bool(SR_CONF_ZERO, true);
    else
        _device_agent->set_config_bool(SR_CONF_ZERO, false);
}

void DeviceOptionsDock::on_calibration()
{
    _device_agent->set_config_bool(SR_CONF_CALI, true);
}

// ---------------------------------------------------------------------------
// Timer: watch for mode changes
// ---------------------------------------------------------------------------

void DeviceOptionsDock::mode_check_timeout()
{
    if (_isBuilding || !_content_built)
        return;

    if (_device_agent->is_hardware()) {
        bool test;
        int mode;
        if (_device_agent->get_config_int16(SR_CONF_OPERATION_MODE, mode)) {
            if (mode != _opt_mode) {
                bool stream_cfg = false;
                const bool has_stream_cfg = _device_agent->get_config_bool(SR_CONF_STREAM, stream_cfg);
                dsv_info("DeviceOptionsDock::mode_check_timeout op_mode changed %d -> %d stream_cfg(valid=%d,val=%d) helper_stream=%d",
                         _opt_mode, mode,
                         (int)has_stream_cfg, (int)stream_cfg,
                         (int)_device_agent->is_stream_mode());
                _opt_mode = mode;
                build_dynamic_panel();
            }
        }
        if (_device_agent->get_config_bool(SR_CONF_TEST, test)) {
            if (test) {
                for (auto box : _probes_checkBox_list) {
                    box->setCheckState(Qt::Checked);
                    box->setDisabled(true);
                }
            }
        }
    } else if (_device_agent->is_demo()) {
        QString opt_mode = _device_agent->get_demo_operation_mode();
        if (opt_mode != _demo_operation_mode) {
            _demo_operation_mode = opt_mode;
            build_dynamic_panel();
        }
    }
}

// ---------------------------------------------------------------------------
// IUiWindow
// ---------------------------------------------------------------------------

void DeviceOptionsDock::UpdateLanguage()
{
    if (_content_built)
        build_content();
}

void DeviceOptionsDock::UpdateFont()
{
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    setFont(font);
}

} // namespace dock
} // namespace pv
