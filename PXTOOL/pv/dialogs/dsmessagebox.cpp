/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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


#include "dsmessagebox.h"
#include "shadow.h"

#include <QObject>
#include <QEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QAbstractButton>
#include "../dsvdef.h"
#include "../config/appconfig.h"
#include "../ui/fn.h"
#include "../ui/popupdlglist.h"

namespace pv {
namespace dialogs {

DSMessageBox::DSMessageBox(QWidget *parent,const QString title) :
#ifdef Q_OS_LINUX
    QDialog(NULL)  //enable the popup dialog draged.
#else
    QDialog(parent)
#endif
{
    (void)parent;
    _layout = NULL;
    _main_widget = NULL;
    _msg = NULL;
    _titlebar = NULL;
    _shadow = NULL;  
    _main_layout = NULL;

    _bClickYes = false;

    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);

    _main_widget = new QWidget(this);
    _main_layout = new QVBoxLayout(_main_widget);
    _main_widget->setLayout(_main_layout);
    _main_layout->setContentsMargins(0, 5, 0, 0);
    _main_layout->setSpacing(0);

    _shadow = new Shadow(this);
    _msg = new QMessageBox(this);
    _msg->setObjectName("dsMessageBoxInner");
    _titlebar = new toolbars::TitleBar(false, this, NULL, false);
    _layout = new QVBoxLayout(this);
 
    _shadow->setBlurRadius(10.0);
    _shadow->setDistance(3.0);
    _shadow->setColor(QColor(0, 0, 0, 80));

    _main_widget->setAutoFillBackground(true);
    this->setGraphicsEffect(_shadow);  

    _msg->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);   

    if (!title.isEmpty()){
        _titlebar->setTitle(title);
    }
    else{
        _titlebar->setTitle(tr("Message"));
    }

    setObjectName("dsMessageBoxDialog");

    _main_layout->addWidget(_titlebar);

    auto *title_sep = new QWidget(_main_widget);
    title_sep->setObjectName("device_options_divider");
    title_sep->setFixedHeight(1);
    _main_layout->addWidget(title_sep);

    _main_layout->addWidget(_msg);
    _layout->addWidget(_main_widget);

    setLayout(_layout); 

    connect(_msg, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(on_button(QAbstractButton*)));
}


DSMessageBox::~DSMessageBox()
{
    DESTROY_QT_OBJECT(_layout);
    DESTROY_QT_OBJECT(_main_widget);
    DESTROY_QT_OBJECT(_msg);
    DESTROY_QT_OBJECT(_titlebar);
    DESTROY_QT_OBJECT(_shadow);
    DESTROY_QT_OBJECT(_main_layout);

    PopupDlgList::RemoveDlgFromList(this);
}

void DSMessageBox::accept()
{
    using namespace Qt;

    QDialog::accept();
}

void DSMessageBox::reject()
{
    using namespace Qt;

    QDialog::reject();
}
  
QMessageBox* DSMessageBox::mBox()
{
    return _msg;
}
  
void DSMessageBox::on_button(QAbstractButton *btn)
{
    QMessageBox::ButtonRole role = _msg->buttonRole(btn);

    if (role == QMessageBox::AcceptRole || role == QMessageBox::YesRole){
        _bClickYes = true;
         accept();
    } 
    else
        reject();
}

void DSMessageBox::update_font()
{
    QFont font = this->font();
    const float fs = AppConfig::Instance().appOptions.fontSize;
    font.setPixelSize(qRound(fs >= 9.0f ? fs : 9.0f));
    ui::set_form_font(this, font);

    if (_titlebar != NULL){
        _titlebar->update_font();
    }
}

int DSMessageBox::exec()
{
    update_font();

    PopupDlgList::AddDlgTolist(this);

    return QDialog::exec();
}

} // namespace dialogs
} // namespace pv
