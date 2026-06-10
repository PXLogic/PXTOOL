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

#include "xtoolbutton.h"
#include <QMenu>
#include <QApplication>
#include <QTimer>
#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionToolButton>
#include "../log.h" 

#ifdef _WIN32
#include "../winnativewidget.h"
#endif

namespace
{
    static int _click_times = 0;
    static XToolButton *_lst_button = NULL;
} 

XToolButton::XToolButton(QWidget *parent)
    :QToolButton(parent)
{
    _menu = NULL;
    _is_mouse_down = false;
    _centerContent = false;
}

void XToolButton::setCenterContent(bool center)
{
    if (_centerContent == center)
        return;
    _centerContent = center;
    update();
}

void XToolButton::paintEvent(QPaintEvent *event)
{
    if (!_centerContent || toolButtonStyle() != Qt::ToolButtonTextBesideIcon) {
        QToolButton::paintEvent(event);
        return;
    }

    QStyleOptionToolButton opt;
    initStyleOption(&opt);

    const QIcon icon = opt.icon;
    const QString text = opt.text;
    const QSize iconSz = opt.iconSize.isValid() ? opt.iconSize : iconSize();

    opt.icon = QIcon();
    opt.text = QString();

    QPainter painter(this);
    style()->drawComplexControl(QStyle::CC_ToolButton, &opt, &painter, this);

    if (icon.isNull() && text.isEmpty())
        return;

    const int gap = 4;
    const QFontMetrics fm(font());
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    const int textW = text.isEmpty() ? 0 : fm.horizontalAdvance(text);
#else
    const int textW = text.isEmpty() ? 0 : fm.width(text);
#endif
    const int iconW = icon.isNull() ? 0 : iconSz.width();
    const int totalW = iconW + (icon.isNull() || text.isEmpty() ? 0 : gap) + textW;
    int x = qMax(0, (width() - totalW) / 2);

    if (!icon.isNull()) {
        const int iconY = (height() - iconSz.height()) / 2;
        icon.paint(&painter, x, iconY, iconSz.width(), iconSz.height());
        x += iconSz.width() + gap;
    }

    if (!text.isEmpty()) {
        painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled,
                                       QPalette::ButtonText));
        const int textY = (height() + fm.ascent() - fm.descent()) / 2;
        painter.drawText(x, textY, text);
    }
}

void XToolButton::mousePressEvent(QMouseEvent *event)
{
    _is_mouse_down = true;

    if (_lst_button != this){
        _click_times = 0;
    }

    _lst_button = this;

#ifdef _WIN32
    _menu = this->menu();
    if (_menu == NULL){
        QToolButton::mousePressEvent(event); // is not a popup menu.
        return;
    }

    _click_times++;     

    if (event->button() != Qt::LeftButton){
        setCheckable(true);
        setChecked(false);
        setCheckable(false);        
        return;
    }

    if (_click_times % 2 == 1){
        setCheckable(true);
        setChecked(true);
        QPoint pt = mapToGlobal(rect().bottomLeft());
        connect(_menu, SIGNAL(aboutToHide()), this, SLOT(onHidePopupMenu()));
        pv::WinNativeWidget::EnalbeNoClientArea(false);
        _menu->popup(pt);
    }
    else{
        setCheckable(true);
        setChecked(false);
        setCheckable(false);
    }
    
#else
    QToolButton::mousePressEvent(event);
#endif
}

void XToolButton::mouseReleaseEvent(QMouseEvent *event)
{
    _is_mouse_down = false;
    QToolButton::mouseReleaseEvent(event);
}

void XToolButton::onHidePopupMenu()
{   
#ifdef _WIN32 

    setCheckable(true);
    setChecked(false);
    setCheckable(false);
 
    QWidget *widgetUnderMouse = qApp->widgetAt(QCursor::pos());
    if (widgetUnderMouse != this){
        _is_mouse_down = false;
    }

    if (!_is_mouse_down || _lst_button != this){
       _click_times = 0;
    }
   
    if (_menu != NULL){
        disconnect(_menu, SIGNAL(aboutToHide()), this, SLOT(onHidePopupMenu()));
    } 

    QTimer::singleShot(300, this, [this](){
                pv::WinNativeWidget::EnalbeNoClientArea(true);
            });

#endif
}
