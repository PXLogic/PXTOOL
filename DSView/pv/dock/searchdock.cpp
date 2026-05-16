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

#include "searchdock.h"
#include "../sigsession.h"
#include "../view/cursor.h"
#include "../view/view.h"
#include "../view/timemarker.h"
#include "../view/ruler.h"
#include "../dialogs/search.h"
#include "../data/snapshot.h"
#include "../data/logicsnapshot.h"
#include "../dialogs/dsmessagebox.h"

#include <QObject>
#include <QPainter> 
#include <QRect>
#include <QMouseEvent>
#include <QFuture>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <stdint.h> 
#include "../config/appconfig.h"
#include "../ui/msgbox.h"
#include "../appcontrol.h"
#include "../ui/fn.h"

namespace pv {
namespace dock {

using namespace pv::view;
using namespace pv::widgets;

SearchDock::SearchDock(QWidget *parent, View &view, SigSession *session) :
    QWidget(parent),
    _session(session),
    _view(view)
{
    // Search icon (inside container, not embedded in FakeLineEdit)
    _search_button = new QPushButton(this);
    _search_button->setFixedSize(14, 14);
    _search_button->setDisabled(true);
    _search_button->setObjectName("search_icon_btn");

    QLineEdit *_search_parent = new QLineEdit(this);
    _search_parent->setVisible(false);
    _search_value = new FakeLineEdit(_search_parent);
    _search_value->setObjectName("search_pattern_edit");
    _search_value->setReadOnly(true);
    _search_value->setFrame(false);

    connect(_search_value, SIGNAL(trigger()), this, SLOT(on_set()));

    // Container: [icon | FakeLineEdit] — same pattern as decode_search_container
    auto *pattern_container = new QWidget(this);
    pattern_container->setObjectName("search_pattern_container");
    auto *pc_layout = new QHBoxLayout(pattern_container);
    pc_layout->setContentsMargins(6, 2, 6, 2);
    pc_layout->setSpacing(4);
    pc_layout->addWidget(_search_button);
    pc_layout->addWidget(_search_value, 1);

    // Nav buttons: 24×24, same size as decode prev/next
    _pre_button.setObjectName("search_nav_btn");
    _pre_button.setFixedSize(24, 24);
    _pre_button.setIconSize(QSize(16, 16));
    _nxt_button.setObjectName("search_nav_btn");
    _nxt_button.setFixedSize(24, 24);
    _nxt_button.setIconSize(QSize(16, 16));

    // Hint label
    _hint_label = new QLabel(tr("Click the search pattern\nto configure search options"), this);
    _hint_label->setObjectName("search_hint_label");
    _hint_label->setAlignment(Qt::AlignCenter);
    _hint_label->setWordWrap(true);

    // Controls row: [prev] [pattern_container] [next]
    auto *ctrl_lay = new QHBoxLayout();
    ctrl_lay->setSpacing(8);
    ctrl_lay->setContentsMargins(0, 0, 0, 0);
    ctrl_lay->addWidget(&_pre_button);
    ctrl_lay->addWidget(pattern_container, 1);
    ctrl_lay->addWidget(&_nxt_button);

    // Outer layout: vertically centered
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 16);
    layout->setSpacing(12);
    layout->addStretch(1);
    layout->addLayout(ctrl_lay);
    layout->addWidget(_hint_label);
    layout->addStretch(1);
    setLayout(layout);

    connect(&_pre_button, SIGNAL(clicked()), this, SLOT(on_previous()));
    connect(&_nxt_button, SIGNAL(clicked()), this, SLOT(on_next()));

    ADD_UI(this);
}

SearchDock::~SearchDock()
{
    REMOVE_UI(this);
}

void SearchDock::retranslateUi()
{
    _search_value->setPlaceholderText(tr("search"));
    if (_hint_label)
        _hint_label->setText(tr("Click the search pattern\nto configure search options"));
}

void SearchDock::reStyle()
{
    _pre_button.setIcon(QIcon(":/icons/sidebar/chevron-left.svg"));
    _nxt_button.setIcon(QIcon(":/icons/sidebar/chevron-right.svg"));
    _search_button->setIcon(QIcon(":/icons/sidebar/search.svg"));
    _search_button->setIconSize(QSize(14, 14));
}

void SearchDock::paintEvent(QPaintEvent *)
{
//    QStyleOption opt;
//    opt.init(this);
//    QPainter p(this);
//    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void SearchDock::on_previous()
{
    bool ret;
    int64_t last_pos;
    bool last_hit;
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    assert(snapshot);
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot);

    if (logic_snapshot == NULL || logic_snapshot->empty()) {
        QString strMsg(tr("No Sample data!"));
        MsgBox::Show(strMsg);        
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    last_pos = _view.get_search_pos();
    last_hit = _view.get_search_hit();
    if (last_pos == 0) {
        QString strMsg(tr("Search cursor at the start position!"));
        MsgBox::Show(strMsg);
        return;
    }
    else {
        QFuture<void> future;
        future = QtConcurrent::run([&]{
            last_pos -= last_hit;
            ret = logic_snapshot->pattern_search(0, end, last_pos, _pattern, false);
        });
        Qt::WindowFlags flags = Qt::CustomizeWindowHint;
        QProgressDialog dlg(tr("Search Previous..."),
                            tr("Cancel"),0,0,this,flags);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                           Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        dlg.setCancelButton(NULL);

        QFutureWatcher<void> watcher;
        connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
        watcher.setFuture(future);
        dlg.exec();

        if (!ret) {
            QString strMsg(tr("Pattern not found!"));
            MsgBox::Show(strMsg);
            return;
        } else {
            _view.set_search_pos(last_pos, true);
        }
    }
}

void SearchDock::on_next()
{
    bool ret;
    int64_t last_pos;
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    assert(snapshot);
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot);

    if (logic_snapshot == NULL || logic_snapshot->empty()) {
        QString strMsg(tr("No Sample data!"));
        MsgBox::Show(strMsg);
        return;
    }

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    last_pos = _view.get_search_pos() + _view.get_search_hit();
    if (last_pos >= end) {
        QString strMsg(tr("Search cursor at the end position!"));
        MsgBox::Show(strMsg);
        return;
    } else {
        QFuture<void> future;
        future = QtConcurrent::run([&]{
            ret = logic_snapshot->pattern_search(0, end, last_pos, _pattern, true);
        });
        Qt::WindowFlags flags = Qt::CustomizeWindowHint;
        QProgressDialog dlg(tr("Search Next..."),
                            tr("Cancel"),0,0,this,flags);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                           Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        dlg.setCancelButton(NULL);

        QFutureWatcher<void> watcher;
        connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
        watcher.setFuture(future);
        dlg.exec();

        if (!ret) {
            QString strMsg(tr("Pattern not found!"));
            MsgBox::Show(strMsg);
            return;
        } else {
            _view.set_search_pos(last_pos, true);
        }
    }
}

void SearchDock::on_set()
{
    dialogs::Search dlg(this, _session, _pattern);
    connect(_session->device_event_object(), SIGNAL(device_updated()), &dlg, SLOT(reject()));

    if (dlg.exec()) {
        std::map<uint16_t, QString> new_pattern = dlg.get_pattern();

        QString search_label;
        for (auto& iter:new_pattern) {
            iter.second.remove(QChar(' '), Qt::CaseInsensitive);
            iter.second = iter.second.toUpper();
            search_label.push_back(iter.second);
        }

        _search_value->setText(search_label);
        QFontMetrics fm = this->fontMetrics();
        //fm.width(search_label)
        int tw = fm.boundingRect(search_label).width();
        _search_value->setFixedWidth(tw + _search_button->width()+20);

        if (new_pattern != _pattern) {
            _view.set_search_pos(_view.get_search_pos(), false);
            _pattern = new_pattern;
        }
    }
}

void SearchDock::UpdateLanguage()
{
    retranslateUi();
}

void SearchDock::UpdateTheme()
{
    reStyle();
}

void SearchDock::UpdateFont()
{
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    _search_value->setFont(font);
}

} // namespace dock
} // namespace pv
