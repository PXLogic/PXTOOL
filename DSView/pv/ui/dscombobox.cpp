/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
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

#include "dscombobox.h"
#include <QAbstractItemView>
#include <QFontMetrics>
#include <QString>
#include <QGuiApplication>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QListView>
#include <QFrame>
#include <QPainter>
#include <QStyle>
#include "../config/appconfig.h"

namespace {

class DsComboBoxItemDelegate : public QStyledItemDelegate
{
    int _rowH;

public:
    DsComboBoxItemDelegate(int rowH, QObject *parent)
        : QStyledItemDelegate(parent), _rowH(rowH)
    {
    }

    static QColor highlightColor()
    {
        return QColor(0x11, 0x85, 0xD1);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QSize sz = QStyledItemDelegate::sizeHint(opt, index);
        sz.setHeight(_rowH);
        return sz;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const bool highlighted = opt.state & QStyle::State_MouseOver
                || opt.state & QStyle::State_Selected;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);

        if (highlighted) {
            painter->fillRect(opt.rect, highlightColor());
            opt.state &= ~(QStyle::State_MouseOver | QStyle::State_Selected);
            opt.palette.setColor(QPalette::Text, Qt::white);
            opt.palette.setColor(QPalette::HighlightedText, Qt::white);
        }

        QStyledItemDelegate::paint(painter, opt, index);
        painter->restore();
    }
};

} // namespace

DsComboBox::DsComboBox(QWidget *parent) 
    :QComboBox(parent)
{
    _bPopup = false;
    _popupFitContents = false;
    QComboBox::setSizeAdjustPolicy(QComboBox::AdjustToContents);   
}

void DsComboBox::setPopupFitContents(bool fit)
{
    _popupFitContents = fit;
}

int DsComboBox::maxItemTextWidth() const
{
    int maxWidth = 0;
    const QFontMetrics fm(fontMetrics());

    for (int i = 0; i < count(); ++i) {
        const int w = fm.boundingRect(itemText(i)).width();
        if (w > maxWidth)
            maxWidth = w;
    }

    return maxWidth;
}

int DsComboBox::popupContentWidth() const
{
    const int textW = maxItemTextWidth();
    if (textW <= 0)
        return 0;

    // QAbstractItemView::item uses 12px horizontal padding in device bar QSS.
    return textW + 24 + 6;
}

void DsComboBox::adjustToItemContents(int maxWidgetWidth)
{
    if (count() <= 0)
        return;

    const int textW = maxItemTextWidth();
    if (textW <= 0)
        return;

    // QComboBox padding (8px each side) + drop-down (16px).
    int comboW = textW + 16 + 16 + 16;
    int popupW = popupContentWidth();

    if (maxWidgetWidth > 0) {
        comboW = qMin(comboW, maxWidgetWidth);
        popupW = qMin(popupW, maxWidgetWidth);
    }

    setMinimumWidth(comboW);
    if (maxWidgetWidth > 0)
        setMaximumWidth(maxWidgetWidth);

    if (QAbstractItemView *itemView = view()) {
        itemView->setMinimumWidth(popupW);
        itemView->setMaximumWidth(popupW);
    }
}

DsComboBox::~DsComboBox()
{

}

void DsComboBox::setPopupItemHeight(int height)
{
    if (height <= 0)
        return;

    setItemDelegate(new DsComboBoxItemDelegate(height, this));

    if (QListView *lv = qobject_cast<QListView*>(view()))
        lv->setUniformItemSizes(true);
}

void DsComboBox::measureSize()
{
    int num = this->count();
    int maxWidth = 0;
    int height = 30;
    QFontMetrics fm = this->fontMetrics();

    for (int i=0; i<num; i++){
        QString text = this->itemText(i);
        QRect rc = fm.boundingRect(text);

        if (rc.width() > maxWidth){
            maxWidth = rc.width();
        }
        height = rc.height();

    }

    // Append the size rule to whatever stylesheet is already set so we don't
    // wipe the background-color that reStyle() applied to this combo.
    QString sizeRule = QString("QAbstractItemView{min-width:%1px; min-height:%2px;}")
                .arg(maxWidth + 30)
                .arg(height + 5);
    QString base = this->styleSheet();
    // Strip any previously appended size rule before re-appending.
    int idx = base.indexOf("QAbstractItemView{min-width:");
    if (idx != -1)
        base = base.left(idx);
    this->setStyleSheet(base + sizeRule);
}

void DsComboBox::showPopup()
{
    _bPopup = true;

    if (QListView *lv = qobject_cast<QListView*>(view())) {
        lv->setUniformItemSizes(true);
        lv->setMouseTracking(true);
    }

    if (_popupFitContents)
        measureSize();

#ifdef Q_OS_DARWIN

    QComboBox::showPopup();

    QWidget *popup = this->findChild<QFrame*>();
    if (popup) {
        if (_popupFitContents) {
            const int w = popupContentWidth();
            if (w > 0)
                popup->setFixedWidth(w);
        }

        // Reposition popup to appear directly below this combo box.
        // Qt's default calculation can be wrong on macOS in certain widget hierarchies.
        QPoint below = this->mapToGlobal(QPoint(0, this->height()));
        int screenH = QGuiApplication::primaryScreen()->size().height();
        int popH = popup->height();
        if (screenH <= 1080)
            popup->setMaximumHeight(750);
        // Flip above if not enough room below
        if (below.y() + popH > screenH)
            below.setY(this->mapToGlobal(QPoint(0, 0)).y() - popH);
        popup->move(below);
        popup->setStyleSheet("background-color:" + AppConfig::Instance().GetStyleColor().name());
    }

#else
    QComboBox::showPopup();

    if (_popupFitContents) {
        if (QFrame *popup = this->findChild<QFrame*>()) {
            const int w = popupContentWidth();
            if (w > 0)
                popup->setFixedWidth(w);
        }
    }

    if (QListView *lv = qobject_cast<QListView*>(view()))
        lv->viewport()->update();
#endif

}

void DsComboBox::hidePopup()
{
    QComboBox::hidePopup();
    _bPopup = false;
}

void DsComboBox::refreshPopupLayout()
{
    measureSize();
    if (QAbstractItemView *view = this->view())
        view->update();
    update();
}
 