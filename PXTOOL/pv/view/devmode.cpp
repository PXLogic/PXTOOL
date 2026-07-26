/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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

#include "devmode.h"
#include "view.h"
#include "trace.h"
#include "../sigsession.h" 

#include <assert.h> 
#include <QStyleOption>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QHBoxLayout>

#include "../config/appconfig.h"
#include "../ui/msgbox.h"
#include "../log.h"
#include "../appcontrol.h"
#include "../ui/fn.h"


static const struct dev_mode_name dev_mode_name_list[] =
{
    {LOGIC, "la.svg"},
    {ANALOG, "daq.svg"},
    {DSO, "osc.svg"},
};

static QIcon tinted_mode_icon(const QString &path, const QColor &color,
                              const QSize &size)
{
    QPixmap source = QIcon(path).pixmap(size);
    if (source.isNull())
        return QIcon();

    QPixmap tinted(source.size());
    tinted.fill(Qt::transparent);
    QPainter painter(&tinted);
    painter.drawPixmap(0, 0, source);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    painter.end();
    return QIcon(tinted);
}
  
namespace pv {
namespace view {

DevMode::DevMode(QWidget *parent, SigSession *session) :
    QWidget(parent) 
{
    setObjectName("DevMode");
    setAccessibleName(tr("Capture mode switch"));
    setAccessibleDescription(
        tr("Switch between Logic Analyzer, Data Acquisition, and Oscilloscope."));

    _bFile = false;
    _updating_mode = false;

    _session = session;
    _device_agent = session->get_device();

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    _close_button = new XToolButton();
    _close_button->setObjectName("FileCloseButton");
    _close_button->setContentsMargins(0, 0, 0, 0);
    _close_button->setFixedWidth(10);
    _close_button->setFixedHeight(height());
    _close_button->setIconSize(QSize(10, 10));
    _close_button->setToolButtonStyle(Qt::ToolButtonIconOnly); 
    _close_button->setMinimumWidth(10);

    _mode_btn = new DsComboBox(this);
    _mode_btn->setObjectName("ModeButton");
    _mode_btn->setFrame(false);
    apply_mode_style();
    // Keep the mode switch as a real popup button so keyboard and accessibility
    // clients can discover and activate it without relying on the icon.
    _mode_btn->setAccessibleName(tr("Mode"));
    _mode_btn->setAccessibleDescription(
        tr("Select the active capture mode. Opens a menu of available modes."));
    _mode_btn->setToolTip(tr("Capture mode"));
    _mode_btn->setStatusTip(tr("Select Logic Analyzer, Data Acquisition, or Oscilloscope"));
    _mode_btn->setFocusPolicy(Qt::StrongFocus);
    _mode_btn->setIconSize(QSize(24, 18));
    _mode_btn->setContentsMargins(0, 0, 0, 0);
    _mode_btn->setMinimumHeight(28);
    _mode_btn->setMinimumWidth(150);
    _mode_btn->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    _mode_btn->setPopupItemHeight(28);
    _mode_btn->setPopupFitContents(true);
    connect(_mode_btn, SIGNAL(currentIndexChanged(int)), this, SLOT(on_mode_change(int)));

    layout->addWidget(_close_button);
    layout->addWidget(_mode_btn); 

    layout->setStretch(1, 100); 
    setLayout(layout);

    connect(_close_button, SIGNAL(clicked()), this, SLOT(on_close()));

    ADD_UI(this);
}

DevMode::~DevMode()
{
    REMOVE_UI(this);
}

void DevMode::set_device()
{ 
     if (_device_agent->have_instance() == false){
        dsv_detail("DevMode::set_device, Have no device.");   
        return;
     }

    _bFile = false;
   
    _updating_mode = true;
    _mode_btn->clear();

    _close_button->setIcon(QIcon());
    _close_button->setDisabled(true); 

    QString iconPath = GetIconPath() + "/";
    const QColor modeColor(AppConfig::Instance().IsDarkStyle()
                               ? QStringLiteral("#c084fc")
                               : QStringLiteral("#7c3aed"));
    auto dev_mode_list  = _device_agent->get_device_mode_list();

    for (const GSList *l = dev_mode_list; l; l = l->next)
    {
        const sr_dev_mode *mode = (const sr_dev_mode *)l->data;
        auto *mode_name = get_mode_name(mode->mode);
        QString icon_name = QString::fromLocal8Bit(mode_name->_logo);

        int md = mode->mode;
        QString label;

        if (md == LOGIC)
            label = tr("Logic Analyzer");
        else if (md == ANALOG)
            label = tr("Data Acquisition");
        else if (md == DSO)
            label = tr("Oscilloscope");

        _mode_btn->addItem(tinted_mode_icon(iconPath + icon_name, modeColor,
                                             QSize(24, 18)), label, md);
    }
    _updating_mode = false;

    _close_button->setVisible(false);

    if (_device_agent->is_file()){
        _close_button->setVisible(true);
        _close_button->setDisabled(false);
        _close_button->setIcon(QIcon(iconPath + "/close.svg"));
        _bFile = true;
    }

    sync_mode_button(_device_agent->get_work_mode(), iconPath);

    UpdateFont();
    apply_mode_style();
    update();    
}

void DevMode::apply_mode_style()
{
    if (!_mode_btn)
        return;

    const int fontSize = qMax(1, qRound(AppConfig::Instance().appOptions.fontSize));
    const QString purple = AppConfig::Instance().IsDarkStyle()
        ? QStringLiteral("#a855f7") : QStringLiteral("#7c3aed");
    const QString purpleBg = AppConfig::Instance().IsDarkStyle()
        ? QStringLiteral("rgba(147, 51, 234, 0.20)")
        : QStringLiteral("rgba(124, 58, 237, 0.12)");
    const QString purpleBorder = AppConfig::Instance().IsDarkStyle()
        ? QStringLiteral("#9333ea") : QStringLiteral("#7c3aed");

    QFont font = _mode_btn->font();
    font.setPixelSize(fontSize);
    font.setBold(true);
    font.setUnderline(true);
    _mode_btn->setFont(font);
    _mode_btn->setStyleSheet(
        QStringLiteral("QComboBox#ModeButton, QComboBox#ModeButton:hover, "
                       "QComboBox#ModeButton:focus, QComboBox#ModeButton:on "
                       "{ background: transparent; border: none; color: %1; "
                       "font-size: %2px; font-weight: 600; padding: 3px 8px 6px 4px; }"
                       "QComboBox#ModeButton::drop-down { width: 0px; border: none; }"
                       "QComboBox#ModeButton::down-arrow { image: none; width: 0px; "
                       "height: 0px; }"
                       "QComboBox#ModeButton QAbstractItemView { color: %1; }")
            .arg(purple, QString::number(fontSize)));
    setStyleSheet(QStringLiteral(
        "QWidget#DevMode { background-color: %1; border: 1px solid %2; "
        "border-radius: 4px; }" ).arg(purpleBg, purpleBorder));
}

void DevMode::paintEvent(QPaintEvent*)
{  
    using pv::view::Trace;

    QStyleOption o;
    o.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &painter, this);
}

void DevMode::sync_mode_button(int mode, const QString &iconPath)
{
    QString label;
    if (mode == LOGIC)
        label = tr("Logic Analyzer");
    else if (mode == ANALOG)
        label = tr("Data Acquisition");
    else if (mode == DSO)
        label = tr("Oscilloscope");
    else
        label = tr("Capture Mode");

    (void)iconPath;
    const bool oldUpdating = _updating_mode;
    _updating_mode = true;
    for (int i = 0; i < _mode_btn->count(); ++i) {
        if (_mode_btn->itemData(i).toInt() == mode) {
            _mode_btn->setCurrentIndex(i);
            break;
        }
    }
    _updating_mode = oldUpdating;
    _mode_btn->setAccessibleName(tr("Mode"));
    _mode_btn->setAccessibleDescription(
        tr("Current mode: %1. Opens a menu to switch capture mode.").arg(label));
    _mode_btn->setToolTip(tr("Capture mode: %1 (click to switch)").arg(label));

}

void DevMode::on_mode_change(int index)
{
    if (_updating_mode || index < 0 || index >= _mode_btn->count())
        return;

    if (_device_agent->have_instance() == false){
        assert(false);
    }

    const int mode = _mode_btn->itemData(index).toInt();
    if (_device_agent->get_work_mode() == mode){
        return;
    }

    QString iconPath = GetIconPath();

    _session->stop_capture();
    _session->session_save();
    _session->switch_work_mode(mode);
    sync_mode_button(mode, iconPath);

    UpdateFont();
}

void DevMode::on_close()
{
   if (_device_agent->have_instance() == false){
        assert(false);
    }

    if (_bFile && MsgBox::Confirm(tr("Are you sure to close the device?"))){
        _session->close_file(_device_agent->handle());
    }
}

void DevMode::mousePressEvent(QMouseEvent *event)
{
	assert(event);
	(void)event;
}

void DevMode::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);
        (void)event;
}

void DevMode::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
	_mouse_point = event->pos();
	update();
}

void DevMode::leaveEvent(QEvent*)
{
	_mouse_point = QPoint(-1, -1);
	update();
}

const struct dev_mode_name* DevMode::get_mode_name(int mode) 
{
    for(auto &o : dev_mode_name_list)
        if (mode == o._mode){
            return &o;
    }
    assert(false);
}

void DevMode::UpdateLanguage()
{
    set_device();
}

void DevMode::UpdateTheme()
{
    apply_mode_style();
    set_device();
}

void DevMode::UpdateFont()
{
    QFont font = this->font();
    font.setPixelSize(qRound(AppConfig::Instance().appOptions.fontSize));
    
    auto buttons = this->findChildren<QToolButton*>();
    for(auto o : buttons)
    { 
        o->setFont(font);
    }

    _mode_btn->setFont(font);
    apply_mode_style();
}

} // namespace view
} // namespace pv
