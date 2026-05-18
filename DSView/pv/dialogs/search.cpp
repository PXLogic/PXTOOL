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

/*
 * NOTE: dialogs::Search is @deprecated since 2026-05-18 — its UI has been merged
 * into SearchDock. See docs/superpowers/specs/2026-05-18-search-options-merge-into-dock.md.
 * The class is kept here only for backwards compatibility; no DSView code path
 * currently instantiates it.
 */

#include "search.h"
#include "../view/logicsignal.h"
#include <assert.h>
#include <QRegularExpressionValidator>
#include <QPushButton>
#include <QHBoxLayout>
 

namespace pv {
namespace dialogs {

// SearchEdgeFlagEdit implementation moved to pv::widgets — see
// DSView/pv/widgets/searchedgeflagedit.{h,cpp}. The `using` declaration in
// search.h keeps existing callers compiling.

Search::Search(QWidget *parent, SigSession *session, std::map<uint16_t, QString> pattern) :
    DSDialog(parent),
    _session(session)
{
    setObjectName("searchDialog");
    setMinimumWidth(320);

    QFont monoFont("Monaco");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setFixedPitch(true);

    QRegularExpression value_rx("[10XRFCxrfc]+");
    QValidator *value_validator = new QRegularExpressionValidator(value_rx, this);

    // Remove left/right margins from _main_layout so dividers span full width;
    // content areas carry their own padding via setContentsMargins.
    layout()->setContentsMargins(0, 5, 0, 0);
    layout()->setSpacing(0);

    // Keep 8px gap below TitleBar to give the title area visual breathing room
    SetTitleSpace(8);
    auto *top_sep = new QWidget(this);
    top_sep->setObjectName("search_divider");
    top_sep->setFixedHeight(1);
    layout()->addWidget(top_sep);

    QGridLayout *search_layout = new QGridLayout();
    search_layout->setVerticalSpacing(4);
    search_layout->setHorizontalSpacing(8);
    search_layout->setContentsMargins(12, 12, 12, 12);

    int index = 0;

    for(auto s : _session->get_signals()) {
        if (s->signal_type() == SR_CHANNEL_LOGIC) {
            view::LogicSignal *logicSig = (view::LogicSignal*)s;

            auto *name_lbl = new QLabel(logicSig->get_name()+":", this);
            name_lbl->setObjectName("search_ch_name");

            auto *idx_lbl = new QLabel(QString::number(logicSig->get_index()), this);
            idx_lbl->setObjectName("search_ch_idx");

            SearchEdgeFlagEdit *search_lineEdit = new SearchEdgeFlagEdit(this);
            search_lineEdit->setObjectName("search_ch_edit");
            if (pattern.find(logicSig->get_index()) != pattern.end())
                search_lineEdit->setText(pattern[logicSig->get_index()]);
            else
                search_lineEdit->setText("X");
            search_lineEdit->setValidator(value_validator);
            search_lineEdit->setMaxLength(1);
            search_lineEdit->setInputMask("X");
            search_lineEdit->setFont(monoFont);
            search_lineEdit->setFixedWidth(80);
            search_lineEdit->setAlignment(Qt::AlignCenter);
            _search_lineEdit_vec.push_back(search_lineEdit);

            search_layout->addWidget(name_lbl, index, 0, Qt::AlignRight | Qt::AlignVCenter);
            search_layout->addWidget(idx_lbl,  index, 1, Qt::AlignRight | Qt::AlignVCenter);
            search_layout->addWidget(search_lineEdit, index, 2);

            connect(search_lineEdit, SIGNAL(editingFinished()), this, SLOT(format()));
            index++;
        }
    }

    auto *legend_lbl = new QLabel(tr("X: Don't care\n0: Low level\n1: High level\nR: Rising edge\nF: Falling edge\nC: Rising/Falling edge"), this);
    legend_lbl->setObjectName("search_legend");
    search_layout->addWidget(legend_lbl, 0, 3, index, 1, Qt::AlignTop);
    search_layout->setColumnStretch(3, 100);

    layout()->addLayout(search_layout);

    // Divider above footer buttons
    auto *bot_sep = new QWidget(this);
    bot_sep->setObjectName("search_divider");
    bot_sep->setFixedHeight(1);
    layout()->addWidget(bot_sep);

    // Footer: plain QPushButtons (QDialogButtonBox ignores app QSS background on macOS)
    auto *cancel_btn = new QPushButton(tr("Cancel"), this);
    cancel_btn->setObjectName("device_cancel_btn");
    auto *ok_btn = new QPushButton(tr("OK"), this);
    ok_btn->setObjectName("device_ok_btn");
    /* macOS native default-button chrome overrides app QSS; match Device Options (plain QPushButton + flat). */
    cancel_btn->setFlat(true);
    ok_btn->setFlat(true);
    cancel_btn->setAutoDefault(false);
    ok_btn->setAutoDefault(false);
    cancel_btn->setDefault(false);
    ok_btn->setDefault(false);

    auto *btn_lay = new QHBoxLayout();
    btn_lay->setContentsMargins(12, 10, 12, 10);
    btn_lay->setSpacing(8);
    btn_lay->addStretch();
    btn_lay->addWidget(cancel_btn);
    btn_lay->addWidget(ok_btn);
    layout()->addLayout(btn_lay);

    setTitle(tr("Search Options"));

    connect(ok_btn, &QPushButton::clicked, this, [this]() { accept(); });
    connect(cancel_btn, &QPushButton::clicked, this, [this]() { reject(); });
}

Search::~Search()
{
}

void Search::accept()
{
    using namespace Qt;

    QDialog::accept();
}

void Search::format()
{
    QLineEdit *sc = qobject_cast<QLineEdit *>(sender());
    sc->setText(sc->text().toUpper());
}

std::map<uint16_t, QString> Search::get_pattern()
{
    std::map<uint16_t, QString> pattern;

    int index = 0;
    for(auto s :_session->get_signals()) {
        if (s->signal_type() == SR_CHANNEL_LOGIC) {
            view::LogicSignal *logicSig = (view::LogicSignal*)s;
            pattern[logicSig->get_index()] = _search_lineEdit_vec[index]->text();
            index++;
        }
    }

    return pattern;
}

} // namespace dialogs
} // namespace pv
