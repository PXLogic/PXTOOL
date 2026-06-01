/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

#include "searchcombobox.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPoint>
#include <QLineEdit>
#include <QPointer>
#include <QScrollBar>
#include <QIcon>
#include "../config/appconfig.h"
#include "../appcontrol.h"
#include "../ui/fn.h"

//----------------------ComboButtonItem

ComboButtonItem::ComboButtonItem(QWidget *parent, ISearchItemClick *click, void *data_handle)
:QPushButton(parent)
{
    _click = click;
    _data_handle = data_handle;
}

void ComboButtonItem::mousePressEvent(QMouseEvent *e)
{
    (void)e;
    
    if (_click != NULL){
        _click->OnItemClick(this, _data_handle);
    }
}

//----------------------SearchComboBox

SearchComboBox::SearchComboBox(QWidget *parent)
    : QDialog(parent)
{ 
    _bShow = false;
    _item_click = NULL;
    _scroll = NULL;
    _search_edit = NULL;
    _clear_btn = NULL;
    _ignore_activation = false;
    setObjectName("decode_protocol_picker");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
}

SearchComboBox::~SearchComboBox(){
    //release resource
    for (auto o : _items){
        delete o;
    }
    _items.clear();  
}

void SearchComboBox::ShowDlg(QWidget *editline)
{
    if (_bShow)
        return;   

    _bShow = true;
    
    int w = 350; 
    int h = 550;
    const int toolbar_btn_h = 28;
    int eh = toolbar_btn_h;

    if (editline != NULL){
       w = editline->width();
    }

    this->setFixedSize(w, h);

    QVBoxLayout *grid = new QVBoxLayout(this);
    this->setLayout(grid);
    grid->setContentsMargins(0,0,0,0);
    grid->setAlignment(Qt::AlignTop);
    grid->setSpacing(2);

    QWidget *search_row = new QWidget(this);
    QHBoxLayout *search_lay = new QHBoxLayout(search_row);
    search_lay->setContentsMargins(0, 0, 2, 0);
    search_lay->setSpacing(2);

    search_row->setFixedHeight(eh);

    _search_edit = new QLineEdit(search_row);
    _search_edit->setFixedHeight(eh);
    _search_edit->setMaximumWidth(this->width());

    _clear_btn = new QPushButton(search_row);
    _clear_btn->setFixedSize(14, 14);
    _clear_btn->setIcon(QIcon(":/icons/sidebar/x.svg"));
    _clear_btn->setIconSize(QSize(10, 10));
    _clear_btn->setFlat(true);
    _clear_btn->setVisible(false);
    _clear_btn->setToolTip(tr("Clear"));

    search_lay->addWidget(_search_edit, 1);
    search_lay->addWidget(_clear_btn);
    grid->addWidget(search_row);

    QWidget *panel= new QWidget(this);
    panel->setContentsMargins(0,0,0,0);
    panel->setFixedSize(w, h - eh);
    grid->addWidget(panel);   

    QWidget *listPanel = new QWidget(panel);
    listPanel->setObjectName("decode_protocol_list");
    QVBoxLayout *listLay = new QVBoxLayout(listPanel);
    listLay->setContentsMargins(2, 2, 20, 2);
    listLay->setSpacing(4);
    listLay->setAlignment(Qt::AlignTop);

    QFont font = this->font();
    font.setPixelSize(qRound(AppConfig::Instance().appOptions.fontSize));

    for (auto o : _items)
    { 
        ComboButtonItem *bt = new ComboButtonItem(listPanel, this, o);
        bt->setText(o->_name);
        bt->setObjectName("flat");
        bt->setMaximumWidth(w - 20);
        bt->setMinimumWidth(w - 20);
        o->_control = bt;
        bt->setFont(font);

        listLay->addWidget(bt);        
    } 
 
    _scroll = new QScrollArea(panel);
    _scroll->setWidget(listPanel);
    _scroll->setStyleSheet("QScrollArea{border:none;}");
    _scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scroll->setFixedSize(w, h - eh);

    connect(_search_edit, SIGNAL(textChanged(const QString &)),
            this, SLOT(on_search_text_changed(const QString &)));
    connect(_clear_btn, SIGNAL(clicked()), this, SLOT(on_clear_clicked()));

    _ignore_activation = true;
    show();

    // Top-level popup: screen coordinates from anchor top-left (keyword container).
    if (editline != NULL)
        move(editline->mapToGlobal(QPoint(0, 0)));

    _search_edit->setFocus();
    _ignore_activation = false;
}

void SearchComboBox::AddDataItem(QString id, QString name, void *data_handle)
{
    SearchDataItem *item = new SearchDataItem();
    item->_id = id;
    item->_name = name;
    item->_data_handle = data_handle;
    this->_items.push_back(item);
}

 void SearchComboBox::changeEvent(QEvent *event)
 {
    if (!_ignore_activation
        && event->type() == QEvent::ActivationChange
        && this->isActiveWindow() == false){
        this->close();
        this->deleteLater();
        return;
    }
    
    QWidget::changeEvent(event);
 }

 void SearchComboBox::OnItemClick(void *sender, void *data_handle)
 {
    (void)sender;

     if (data_handle != NULL && _item_click){
         SearchDataItem *item = (SearchDataItem*)data_handle;
         ISearchItemClick *click = _item_click;
         void *handle = item->_data_handle;

         /* close() loses activation -> changeEvent() would otherwise call
          * deleteLater() on us first; combined with the trailing deleteLater()
          * below that becomes a use-after-free once click->OnItemClick opens
          * a modal dialog (DecoderOptionsDlg) and runs a nested event loop,
          * which processes the queued deletion before we return.
          * Suppress the activation-change auto-delete for the duration of
          * this call so only the trailing deleteLater() takes effect. */
         _ignore_activation = true;
         this->close();

         /* Defensive QPointer in case `click` decides to delete us anyway
          * (e.g. via parent reset). */
         QPointer<SearchComboBox> alive(this);
         click->OnItemClick(this, handle);
         if (alive)
             alive->deleteLater();
     }
 }

 void SearchComboBox::on_search_text_changed(const QString &value)
 {
     if (_clear_btn != NULL)
         _clear_btn->setVisible(!value.isEmpty());
     on_keyword_changed(value);
 }

 void SearchComboBox::on_clear_clicked()
 {
     if (_search_edit == NULL)
         return;
     _search_edit->clear();
 }

 void SearchComboBox::on_keyword_changed(const QString &value)
 {
     if (_items.size() == 0)
        return;

     for(auto o : _items)
     {
         const bool text_match = value.isEmpty()
            || o->_name.indexOf(value, 0, Qt::CaseInsensitive) >= 0
            || o->_id.indexOf(value, 0, Qt::CaseInsensitive) >= 0;
         const bool engine_match = !_engine_filter || _engine_filter(o->_data_handle);
         const bool visible = text_match && engine_match;

         if (visible) {
             if (o->_control->isHidden())
                 o->_control->show();
         } else if (!o->_control->isHidden()) {
             o->_control->hide();
         }
     }
     
    if (_scroll != NULL && _scroll->verticalScrollBar() != NULL)
        _scroll->verticalScrollBar()->setValue(0);
 }

void SearchComboBox::SetEngineFilter(std::function<bool(void*)> fn)
{
    _engine_filter = std::move(fn);
}

void SearchComboBox::RefreshFilter()
{
    if (_search_edit != NULL)
        on_keyword_changed(_search_edit->text());
}
