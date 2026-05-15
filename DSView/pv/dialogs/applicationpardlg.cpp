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

#include "applicationpardlg.h"
#include "dsdialog.h"
#include <QBoxLayout>
#include <QWidget>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QString>
#include <QFontDatabase>
#include <QGroupBox>
#include <QLabel>
#include <vector>
#include <QGridLayout>
#include <QPushButton>

#include "../config/appconfig.h"
#include "../appcontrol.h"
#include "../sigsession.h"
#include "../ui/dscombobox.h"
#include "../log.h"

namespace pv
{
namespace dialogs
{

ApplicationParamDlg::ApplicationParamDlg()
{
   
}

ApplicationParamDlg::~ApplicationParamDlg()
{
}

void ApplicationParamDlg::bind_font_name_list(QComboBox *box, QString v)
{   
    int selDex = -1;

    QString defName(QObject::tr("Default"));
    box->addItem(defName);

    if (_font_name_list.size() == 0)
    {
        QFontDatabase fDataBase;
        _font_name_list = fDataBase.families();
    }
   
    for (QString family : _font_name_list) {
        if (family.indexOf("[") == -1)
        {
            box->addItem(family);

            if (selDex == -1 && family == v){
                selDex = box->count() - 1;
            }
        }
    }

    if (selDex == -1)
        selDex = 0;

    box->setCurrentIndex(selDex);
}

void ApplicationParamDlg::bind_font_size_list(QComboBox *box, float size)
{   
    int selDex = -1;

    float minSize = 0;
    float maxSize = 0;

    AppConfig::GetFontSizeRange(&minSize, &maxSize);

    for(int i=minSize; i<=maxSize; i++)
    {
        box->addItem(QString::number(i));
        if (i == size){
            selDex = box->count() - 1;
        }
    }
    if (selDex == -1)
        selDex = 2;
    box->setCurrentIndex(selDex);
}

bool ApplicationParamDlg::ShowDlg(QWidget *parent)
{
    /* Match DeviceOptions: no title-bar close; centered title; divider under title row;
     * custom footer: outline Cancel (device_cancel_btn), primary OK (device_ok_btn). */
    DSDialog dlg(parent, false, false);
    dlg.setObjectName("displayOptionsDialog");
    dlg.setTitle(QObject::tr("Display options"));
    dlg.setTitleTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    dlg.SetTitleSpace(8);
    dlg.layout()->setSpacing(0);
    dlg.layout()->setDirection(QBoxLayout::TopToBottom);
    dlg.layout()->setAlignment(Qt::AlignTop);
    dlg.layout()->setContentsMargins(0, 5, 0, 0);
    dlg.setMinimumSize(300, 230);

    auto *top_sep = new QWidget(&dlg);
    top_sep->setObjectName("device_options_divider");
    top_sep->setFixedHeight(1);
    dlg.layout()->addWidget(top_sep);

    QVBoxLayout *lay = new QVBoxLayout();
    lay->setContentsMargins(16, 12, 16, 20);
    lay->setSpacing(8);

    //show config
    AppConfig &app = AppConfig::Instance(); 

    QCheckBox *ck_quickScroll = new QCheckBox();
    ck_quickScroll->setChecked(app.appOptions.quickScroll);

    QCheckBox *ck_trigInMid = new QCheckBox();
    ck_trigInMid->setChecked(app.appOptions.trigPosDisplayInMid);

    QCheckBox *ck_profileBar = new QCheckBox();
    ck_profileBar->setChecked(app.appOptions.displayProfileInBar);

    QCheckBox *ck_abortData = new QCheckBox();
    ck_abortData->setChecked(app.appOptions.swapBackBufferAlways);

    QCheckBox *ck_autoScrollLatestData = new QCheckBox();
    ck_autoScrollLatestData->setChecked(app.appOptions.autoScrollLatestData);

    QComboBox *ftCbSize = new DsComboBox();
    ftCbSize->setFixedWidth(50);
    bind_font_size_list(ftCbSize, app.appOptions.fontSize);
   
    // Logic group
    QGroupBox *logicGroup = new QGroupBox(QObject::tr("Logic"));
    QGridLayout *logicLay = new QGridLayout();
    logicLay->setContentsMargins(10,15,15,10);
    logicLay->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    logicGroup->setLayout(logicLay);
    logicLay->addWidget(new QLabel(QObject::tr("Quick scroll")), 0, 0, Qt::AlignLeft); 
    logicLay->addWidget(ck_quickScroll, 0, 1, Qt::AlignRight);
    logicLay->addWidget(new QLabel(QObject::tr("Used abort data")), 1, 0, Qt::AlignLeft); 
    logicLay->addWidget(ck_abortData, 1, 1, Qt::AlignRight);
    logicLay->addWidget(new QLabel(QObject::tr("Auto scoll latest")), 2, 0, Qt::AlignLeft); 
    logicLay->addWidget(ck_autoScrollLatestData, 2, 1, Qt::AlignRight);
    lay->addWidget(logicGroup);

    //Scope group
    QGroupBox *dsoGroup = new QGroupBox(QObject::tr("Scope"));
    QGridLayout *dsoLay = new QGridLayout();
    dsoLay->setContentsMargins(10,15,15,10);
    dsoLay->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    dsoGroup->setLayout(dsoLay);
    dsoLay->addWidget(new QLabel(QObject::tr("Tig pos in middle")), 0, 0, Qt::AlignLeft);
    dsoLay->addWidget(ck_trigInMid, 0, 1, Qt::AlignRight);
    lay->addWidget(dsoGroup);

    //UI
    QGroupBox *uiGroup = new QGroupBox(QObject::tr("UI"));
    QGridLayout *uiLay = new QGridLayout();
    uiLay->setContentsMargins(10,15,15,10);
    uiLay->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    uiGroup->setLayout(uiLay);
    uiLay->addWidget(new QLabel(QObject::tr("Profile in bar")), 0, 0, Qt::AlignLeft);
    uiLay->addWidget(ck_profileBar, 0, 1, Qt::AlignRight);
    uiLay->addWidget(new QLabel(QObject::tr("Font size")), 1, 0, Qt::AlignLeft);
    uiLay->addWidget(ftCbSize, 1, 1, Qt::AlignRight);
    lay->addWidget(uiGroup);

    dlg.layout()->addLayout(lay);

    auto *bot_sep = new QWidget(&dlg);
    bot_sep->setObjectName("device_options_divider");
    bot_sep->setFixedHeight(1);
    dlg.layout()->addWidget(bot_sep);

    auto *cancel_btn = new QPushButton(
        QObject::tr("Cancel"), &dlg);
    cancel_btn->setObjectName("device_cancel_btn");
    auto *ok_btn = new QPushButton("OK", &dlg);
    ok_btn->setObjectName("device_ok_btn");
    auto *footer_lay = new QHBoxLayout();
    footer_lay->setContentsMargins(12, 10, 12, 10);
    footer_lay->addStretch();
    footer_lay->addWidget(cancel_btn);
    footer_lay->addSpacing(8);
    footer_lay->addWidget(ok_btn);
    dlg.layout()->addLayout(footer_lay);

    QObject::connect(ok_btn, &QPushButton::clicked, &dlg, &DSDialog::slotAccept);
    QObject::connect(cancel_btn, &QPushButton::clicked, &dlg, &DSDialog::slotReject);

    dlg.exec();
    bool ret = dlg.IsClickYes();

    //save config
    if (ret){

        bool bAppChanged = false;
        bool bFontChanged = false;
        float fSize = ftCbSize->currentText().toFloat();

        if (app.appOptions.quickScroll != ck_quickScroll->isChecked()){
            app.appOptions.quickScroll = ck_quickScroll->isChecked();
            bAppChanged = true;
        }       
        if (app.appOptions.trigPosDisplayInMid != ck_trigInMid->isChecked()){
            app.appOptions.trigPosDisplayInMid = ck_trigInMid->isChecked();
            bAppChanged = true;
        }
        if (app.appOptions.displayProfileInBar != ck_profileBar->isChecked()){
            app.appOptions.displayProfileInBar = ck_profileBar->isChecked();
            bAppChanged = true;
        }
        if (app.appOptions.swapBackBufferAlways != ck_abortData->isChecked()){
            app.appOptions.swapBackBufferAlways = ck_abortData->isChecked();
            bAppChanged = true;
        }        
        if (app.appOptions.fontSize != fSize){
            app.appOptions.fontSize = fSize;
            bFontChanged = true;
        }
        if (app.appOptions.autoScrollLatestData != ck_autoScrollLatestData->isChecked()){
            app.appOptions.autoScrollLatestData = ck_autoScrollLatestData->isChecked();
            bAppChanged = true;
        }
 
        if (bAppChanged){
            app.SaveApp();
            AppControl::Instance()->GetSession()->broadcast_msg(DSV_MSG_APP_OPTIONS_CHANGED);
        }
        
        if (bFontChanged){
            if (!bAppChanged){
                app.SaveApp();
            }
            AppControl::Instance()->GetSession()->broadcast_msg(DSV_MSG_FONT_OPTIONS_CHANGED);
        }
    }
   
   return ret;
}
 

} //
}//

