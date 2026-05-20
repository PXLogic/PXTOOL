/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
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

int DSMessageBox::exec()
{
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    ui::set_form_font(this, font);

    if (_titlebar != NULL){
        _titlebar->update_font();
    }

    /* Replace the system-native icon with a theme-aware version.
     *
     * Qt's QMessageBox renders its icon inside the first QLabel child that
     * carries a pixmap.  On macOS the platform-provided Question/Warning icons
     * are black, which is invisible on dark backgrounds.  We swap the pixmap
     * for a simple one we draw ourselves: the standard icon re-coloured to
     * contrast with the current theme background.
     *
     * Only QMessageBox::Question and QMessageBox::Warning need special handling
     * because those are the two icons MsgBox::Confirm / MsgBox::Show use; all
     * other icons (NoIcon, Critical, Information) are left unchanged. */
    if (_msg) {
        QMessageBox::Icon icon_type = _msg->icon();
        if (icon_type == QMessageBox::Question
                || icon_type == QMessageBox::Warning
                || icon_type == QMessageBox::Information) {

            const bool isDark = AppConfig::Instance().IsDarkStyle();

            /* Choose a foreground colour that reads on the dialog background.
             * Dark theme: light grey; light theme: dark charcoal. */
            const QColor icon_fg = isDark ? QColor(0xCC, 0xCC, 0xCC)
                                          : QColor(0x33, 0x33, 0x33);

            /* Get the standard pixmap from the current style so the shape is
             * always OS-correct (circle with ?, triangle with !, etc.), then
             * re-colour every non-transparent pixel to icon_fg. */
            int px_size = 32;
            QPixmap base_px;
            if (icon_type == QMessageBox::Question) {
                base_px = _msg->style()->standardIcon(
                    QStyle::SP_MessageBoxQuestion, nullptr, _msg).pixmap(px_size, px_size);
            } else if (icon_type == QMessageBox::Warning) {
                base_px = _msg->style()->standardIcon(
                    QStyle::SP_MessageBoxWarning, nullptr, _msg).pixmap(px_size, px_size);
            } else {
                base_px = _msg->style()->standardIcon(
                    QStyle::SP_MessageBoxInformation, nullptr, _msg).pixmap(px_size, px_size);
            }

            /* Re-colour: convert to image, paint icon_fg over all opaque
             * pixels using SourceIn compositing so the alpha mask is preserved. */
            QImage img = base_px.toImage().convertToFormat(QImage::Format_ARGB32);
            QPainter ip(&img);
            ip.setCompositionMode(QPainter::CompositionMode_SourceIn);
            ip.fillRect(img.rect(), icon_fg);
            ip.end();

            _msg->setIconPixmap(QPixmap::fromImage(img));
        }
    }

    PopupDlgList::AddDlgTolist(this);

    return QDialog::exec();
}

} // namespace dialogs
} // namespace pv
