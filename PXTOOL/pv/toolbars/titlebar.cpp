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

#include "titlebar.h"
#include <QStyle>
#include <QLabel> 
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent> 
#include <QPainter>
#include <QStyleOption>
#include <QSizePolicy>
#include <assert.h>
#include <QTimer>
#include <QApplication>
#include <QGuiApplication>
#include <QPixmap>
#include <QWindow>

#include "../config/appconfig.h"
#include "../appcontrol.h"
#include "../dsvdef.h"
#include "../ui/fn.h"
#include "../log.h"

namespace pv {
namespace toolbars {

TitleBar::TitleBar(bool top, QWidget *parent, ITitleParent *titleParent, bool hasClose) :
    QWidget(parent)
{ 
   _minimizeButton = NULL;
   _maximizeButton = NULL;
   _closeButton = NULL;
   _moving = false;
   _is_draging = false;
   _parent = parent;
   _isTop = top;
   _hasClose = hasClose;
   _title = NULL;
   _is_native = false;
   _titleParent = titleParent;
   _is_done_moved = false; 
   _is_able_drag = true;

    assert(parent);

    setObjectName("TitleBar");
    setContentsMargins(0, 0, 0, 0);
    // Dialog title bars need a larger icon than the main window bar.
    setFixedHeight(_isTop ? 32 : 44);

    QHBoxLayout *lay1 = new QHBoxLayout(this);

    _title = new QLabel(this);
    _title->setAttribute(Qt::WA_TransparentForMouseEvents);
    lay1->addWidget(_title);

    if (_isTop) {
        _minimizeButton = new QToolButton(this);
        _minimizeButton->setObjectName("MinimizeButton");
        _minimizeButton->setIconSize(QSize(16, 16));
        _maximizeButton = new QToolButton(this);
        _maximizeButton->setObjectName("MaximizeButton");
        _maximizeButton->setIconSize(QSize(16, 16));

        lay1->addWidget(_minimizeButton);
        lay1->addWidget(_maximizeButton);

        connect(this, SIGNAL(normalShow()), parent, SLOT(showNormal()));
        connect(this, SIGNAL( maximizedShow()), parent, SLOT(showMaximized()));
        connect(_minimizeButton, SIGNAL( clicked()), parent, SLOT(showMinimized()));
        connect(_maximizeButton, SIGNAL( clicked()), this, SLOT(showMaxRestore()));
    }

    if (_isTop || _hasClose) {
        _closeButton= new QToolButton(this);
        _closeButton->setObjectName("CloseButton");
        _closeButton->setIconSize(QSize(16, 16));
        lay1->addWidget(_closeButton);
        connect(_closeButton, SIGNAL( clicked()), parent, SLOT(close()));
    }

    lay1->insertStretch(0, 500);
    lay1->insertStretch(2, 500);
    lay1->setContentsMargins(0, 0, 0, 0);
    lay1->setSpacing(0);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); 
    
    ADD_UI(this);
}

TitleBar::~TitleBar(){ 
    DESTROY_QT_OBJECT(_minimizeButton);
    DESTROY_QT_OBJECT(_maximizeButton);
    DESTROY_QT_OBJECT(_closeButton);

    REMOVE_UI(this);
}

void TitleBar::reStyle()
{
    QString iconPath = GetIconPath();

    if (_isTop) {
        _minimizeButton->setIcon(QIcon(iconPath+"/minimize.svg"));
        if (ParentIsMaxsized())
            _maximizeButton->setIcon(QIcon(iconPath+"/restore.svg"));
        else
            _maximizeButton->setIcon(QIcon(iconPath+"/maximize.svg"));
    }
    if (_isTop || _hasClose)
        _closeButton->setIcon(QIcon(iconPath+"/close.svg"));
}

bool TitleBar::ParentIsMaxsized()
{
    if (_titleParent != NULL){
        return _titleParent->ParentIsMaxsized();
    } 
    else{
        return parentWidget()->isMaximized();
    }
}

bool TitleBar::canStartDragAt(const QPoint &pos) const
{
    QWidget *child = childAt(pos);
    return child == nullptr || child == _title;
}

void TitleBar::paintEvent(QPaintEvent *event)
{ 
    // Draw title bar background from style first.
    QStyleOption o;
    o.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &p, this);

    // Draw the unified app icon used inside DSView title bars.
    static const QString kTitleIconRes(":/icons/titlebar_wave_icon.png");
    const QPixmap icon(kTitleIconRes);
    if (!icon.isNull()) {
        const int iconLeft = 10;
        const int iconSize = _isTop ? 20 : 36;
        const QPixmap scaled = icon.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const int iconTop = (height() - scaled.height()) / 2;
        p.drawPixmap(iconLeft, iconTop, scaled);
    }

    QWidget::paintEvent(event);
}

void TitleBar::setTitle(QString title)
{
    if (!_is_native){
        _title->setText(title);
    }
    else if (_parent != NULL){
        _parent->setWindowTitle(title);
    }    
}

void TitleBar::setTitleTextAlignment(Qt::Alignment alignment)
{
    _title->setAlignment(alignment);
    if (alignment.testFlag(Qt::AlignHCenter))
        _title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}
  
QString TitleBar::title()
{
    if (!_is_native){
        return _title->text();
    }
    else if (_parent != NULL){
        return _parent->windowTitle();
    }
    return "";
}

void TitleBar::showMaxRestore()
{
    QString iconPath = GetIconPath();
    if (ParentIsMaxsized()) {
        _maximizeButton->setIcon(QIcon(iconPath+"/maximize.svg"));
        normalShow();
    } else {
        _maximizeButton->setIcon(QIcon(iconPath+"/restore.svg"));
        maximizedShow();
    }   
}

void TitleBar::setRestoreButton(bool max)
{
    QString iconPath = GetIconPath();
    if (!max) {
        _maximizeButton->setIcon(QIcon(iconPath+"/maximize.svg"));
    } else {
        _maximizeButton->setIcon(QIcon(iconPath+"/restore.svg"));
    }
}
  
void TitleBar::mousePressEvent(QMouseEvent* event)
{ 
    bool ableMove = !ParentIsMaxsized();

    if(event->button() == Qt::LeftButton && ableMove && _is_able_drag) 
    {
        int x = event->pos().x();
        int y = event->pos().y(); 
        
        if (!canStartDragAt(event->pos())) {
            QWidget::mousePressEvent(event);
            return;
        }

        bool bTopWidow = AppControl::Instance()->GetTopWindow() == _parent;
        bool bClick = (x >= 6 && y >= 5 && x <= width() - 6);  //top window need resize hit check
 
        if (!bTopWidow || bClick ){
#if defined(Q_OS_LINUX) && QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            QWindow *window = this->window()->windowHandle();
            if (window && window->startSystemMove()) {
                event->accept();
                return;
            }
#endif

            _is_draging = true;             

            _clickPos = event->globalPos(); 

            if (_titleParent != NULL){
                _oldPos = _titleParent->GetParentPos();
            }
            else{
                _oldPos = _parent->pos(); 
            }

            _is_done_moved = false;
                
            event->accept();
            return;
        } 
    }  
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{  
    if(_is_draging){ 

        int datX = 0;
        int datY = 0;

        datX = (event->globalPos().x() - _clickPos.x());
        datY = (event->globalPos().y() - _clickPos.y());

        int x = _oldPos.x() + datX;
        int y = _oldPos.y() + datY;

        if (!_moving){
            if (ABS_VAL(datX) >= 2 || ABS_VAL(datY) >= 2){
                _moving = true;
            }
            else{
                return;
            }
        }

        if (_titleParent != NULL){

            if (!_is_done_moved){
                _is_done_moved = true;
                _titleParent->MoveBegin();
            }

            _titleParent->MoveWindow(x, y);
        }
        else{

#ifdef _WIN32

        QRect screenRect = AppControl::Instance()->_screenRect;

        if (screenRect.width() > 0 && QGuiApplication::screens().size() > 1)
        {
            QRect rect = _parent->frameGeometry();

            if (x < screenRect.left()){
                x = screenRect.left();
            }
            if (x + _parent->frameGeometry().width() > screenRect.right())
            {
                x = screenRect.right() - _parent->frameGeometry().width();
            }
        }
#endif

            _parent->move(x, y);
        }
        
        event->accept();
        return;
    } 
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (_moving && _titleParent != NULL){
        _titleParent->MoveEnd();
    }
    _moving = false;
    _is_draging = false;
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{  
    QWidget::mouseDoubleClickEvent(event); 

    if (_isTop){ 

      QTimer::singleShot(200, this, [this](){
                showMaxRestore();
            });
    }
}

void TitleBar::retranslateUi()
{
    static const char *menuLabels[3] = {
        QT_TR_NOOP("File"), QT_TR_NOOP("Window"), QT_TR_NOOP("Help")
    };
    for (int i = 0; i < 3; i++)
        if (_menuBtns[i]) _menuBtns[i]->setText(tr(menuLabels[i]));
}

void TitleBar::UpdateLanguage()
{
    retranslateUi();
}

void TitleBar::UpdateTheme()
{
    reStyle();
}

void TitleBar::UpdateFont()
{
    // Use the application-level font as the base so the family is always the
    // system CJK font (set explicitly in main.cpp).  Using this->font() would
    // pick up whatever the QSS engine resolved, which may be a different family.
    QFont base = QApplication::font();

    QFont titleFont = base;
    if (objectName() == QStringLiteral("main_window_title_bar"))
        titleFont.setPixelSize(qRound(AppConfig::Instance().appOptions.fontSize));
    else
        titleFont.setPointSizeF(AppConfig::Instance().appOptions.fontSize + 1);
    titleFont.setBold(true);
    _title->setFont(titleFont);

    // Update the embedded File/Window/Help menu buttons and their dropdown menus.
    // These QToolButtons have no explicit font-size in QSS and would otherwise
    // inherit whatever size Qt chose for the app font – which can vary.
    if (_menuBtns[0]) {   // only set when menus were added
        QFont menuBtnFont = base;
        menuBtnFont.setPixelSize(qRound(AppConfig::Instance().appOptions.fontSize));
        menuBtnFont.setBold(false);
        for (int i = 0; i < 3; ++i) {
            if (_menuBtns[i])
                _menuBtns[i]->setFont(menuBtnFont);
            if (_menus[i])
                ui::set_menu_font(_menus[i], menuBtnFont);
        }
    }
}

void TitleBar::EnableAbleDrag(bool bEnabled)
{
    _is_able_drag = bEnabled;
}

void TitleBar::addMenusToTitleBar(QMenu *fileMenu, QMenu *windowMenu, QMenu *helpMenu)
{
    QHBoxLayout *lay = qobject_cast<QHBoxLayout *>(layout());

    // Use QToolButton + InstantPopup to avoid macOS native menu-bar integration
    struct MenuDef { QString label; QMenu *menu; };
    const MenuDef defs[] = {
        { tr("File"),   fileMenu   },
        { tr("Window"), windowMenu },
        { tr("Help"),   helpMenu   },
    };

    // Insert in reverse order so File ends up at the leftmost position (index 0)
    for (int i = 2; i >= 0; --i) {
        QToolButton *btn = new QToolButton(this);
        btn->setText(defs[i].label);
        btn->setMenu(defs[i].menu);
        btn->setPopupMode(QToolButton::InstantPopup);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setObjectName("TitleMenuButton");
        btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        lay->insertWidget(0, btn, 0, Qt::AlignVCenter);
        _menuBtns[i] = btn;
        _menus[i] = defs[i].menu;  // Store menu pointer so UpdateFont() can reach it
    }

    // Left margin reserves space for the painted app logo (~41 px wide)
    lay->setContentsMargins(50, 0, 0, 0);
}

 
} // namespace toolbars
} // namespace pv
