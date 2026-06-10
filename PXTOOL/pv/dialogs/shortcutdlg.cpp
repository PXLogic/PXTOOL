/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
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

#include "shortcutdlg.h"

#include <QBoxLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QWidget>
#include <QPalette>

#include "../config/appconfig.h"

namespace pv {
namespace dialogs {

ShortcutDlg::ShortcutDlg(QWidget *parent)
    : DSDialog(parent, false, false)
{
    setObjectName("shortcutDialog");
    setTitle(tr("Keyboard Shortcuts"));
    setTitleTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    SetTitleSpace(8);
    layout()->setSpacing(0);
    layout()->setDirection(QBoxLayout::TopToBottom);
    layout()->setAlignment(Qt::AlignTop);
    layout()->setContentsMargins(0, 5, 0, 0);
    setMinimumSize(420, 480);

    bool dark = AppConfig::Instance().IsDarkStyle();
    QColor divColor = dark ? QColor(0x33, 0x33, 0x33) : QColor(0xdd, 0xdd, 0xdd);
    auto makeDivider = [&]() -> QWidget* {
        auto *sep = new QWidget(this);
        sep->setObjectName("device_options_divider");
        sep->setFixedHeight(1);
        sep->setAutoFillBackground(true);
        QPalette pal = sep->palette();
        pal.setColor(QPalette::Window, divColor);
        sep->setPalette(pal);
        return sep;
    };

    layout()->addWidget(makeDivider());

    // Build rows from config
    auto &opts = AppConfig::Instance().shortcutOptions;
    _rows = {
        { tr("Start Collecting"),  &opts.startCollecting,   "F1"           },
        { tr("Stop Collecting"),   &opts.stopCollecting,    "F2"           },
        { tr("Prev Cursor"),       &opts.switchVernierUp,   "F3"           },
        { tr("Next Cursor"),       &opts.switchVernierDown, "F4"           },
        { tr("Previous Tab"),      &opts.switchPageUp,      "F5"           },
        { tr("Next Tab"),          &opts.switchPageDown,    "F6"           },
        { tr("Jump to Zero"),      &opts.jumpZero,          "F7"           },
        { tr("Zoom In"),           &opts.zoomIn,            "F8"           },
        { tr("Zoom Out"),          &opts.zoomOut,           "F9"           },
        { tr("Zoom Fit"),          &opts.zoomFull,          "F10"          },
        { tr("Add Cursor"),        &opts.vernierCreate,     "Ctrl+H"       },
        { tr("Measure Panel"),     &opts.parameterMeasure,  "Ctrl+G"       },
        { tr("Device Config"),     &opts.deviceConfig,      "Ctrl+1"       },
        { tr("Protocol Decode"),   &opts.protocolDecode,    "Ctrl+2"       },
        { tr("Label Measurement"), &opts.labelMeasurement,  "Ctrl+3"       },
        { tr("Data Search"),       &opts.dataSearch,        "Ctrl+F"       },
        { tr("Save"),              &opts.saveFile,          "Ctrl+S"       },
        { tr("Save As"),           &opts.saveAs,            "Ctrl+Shift+S" },
        { tr("Export"),            &opts.exportFile,        "Ctrl+E"       },
        { tr("Close Session"),     &opts.closeSession,      "Ctrl+W"       },
    };

    // Content area
    _table = new QTableWidget((int)_rows.size(), 2, this);
    _table->setHorizontalHeaderLabels({ tr("Action"), tr("Key Sequence") });
    _table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    _table->horizontalHeader()->resizeSection(1, 160);
    _table->setSelectionMode(QAbstractItemView::SingleSelection);
    _table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    _table->verticalHeader()->setVisible(false);
    _table->setAlternatingRowColors(true);
    _table->setShowGrid(false);

    populate_table();
    connect(_table, &QTableWidget::cellChanged, this, &ShortcutDlg::on_cell_changed);

    auto *content_lay = new QVBoxLayout();
    content_lay->setContentsMargins(16, 10, 16, 10);
    content_lay->setSpacing(8);
    content_lay->addWidget(_table);

    auto *hint = new QLabel(tr("Double-click a shortcut cell to edit.  Leave empty to disable."), this);
    hint->setObjectName("shortcut_hint_label");
    hint->setWordWrap(true);
    content_lay->addWidget(hint);

    layout()->addLayout(content_lay);

    layout()->addWidget(makeDivider());

    // Footer: Reset Defaults (left)  |  stretch  |  Cancel  OK (right)
    auto *reset_btn  = new QPushButton(tr("Reset Defaults"), this);
    reset_btn->setObjectName("device_cancel_btn");   // outline style
    auto *cancel_btn = new QPushButton(tr("Cancel"), this);
    cancel_btn->setObjectName("device_cancel_btn");
    auto *ok_btn     = new QPushButton(tr("OK"), this);
    ok_btn->setObjectName("device_ok_btn");          // primary style

    auto *footer_lay = new QHBoxLayout();
    footer_lay->setContentsMargins(12, 10, 12, 10);
    footer_lay->addWidget(reset_btn);
    footer_lay->addStretch();
    footer_lay->addWidget(cancel_btn);
    footer_lay->addSpacing(8);
    footer_lay->addWidget(ok_btn);
    layout()->addLayout(footer_lay);

    connect(reset_btn,  &QPushButton::clicked, this, &ShortcutDlg::on_reset_defaults);
    connect(ok_btn,     &QPushButton::clicked, this, &DSDialog::slotAccept);
    connect(cancel_btn, &QPushButton::clicked, this, &DSDialog::slotReject);
    connect(this, &DSDialog::accepted, this, &ShortcutDlg::apply_to_config);
}

void ShortcutDlg::populate_table()
{
    _updating = true;
    for (int i = 0; i < _rows.size(); ++i) {
        _rows[i].lastGoodValue = *_rows[i].configField;

        auto *nameItem = new QTableWidgetItem(_rows[i].label);
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        _table->setItem(i, 0, nameItem);

        auto *keyItem = new QTableWidgetItem(*_rows[i].configField);
        keyItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        keyItem->setTextAlignment(Qt::AlignCenter);
        _table->setItem(i, 1, keyItem);
    }
    _updating = false;
}

void ShortcutDlg::on_cell_changed(int row, int col)
{
    if (_updating || col != 1)
        return;

    auto *item = _table->item(row, col);
    if (!item)
        return;

    QString seq = item->text().trimmed();

    auto revert = [&](const QString &prev) {
        _updating = true;
        item->setText(prev);
        _updating = false;
    };

    // Invalid key sequence (non-empty but unrecognised)
    if (!seq.isEmpty() && QKeySequence(seq).toString().isEmpty()) {
        revert(_rows[row].lastGoodValue);
        return;
    }

    // Duplicate check
    if (has_duplicate(row, seq)) {
        QMessageBox::warning(this, tr("Duplicate Shortcut"),
            tr("'%1' is already assigned to another action.").arg(seq));
        revert(_rows[row].lastGoodValue);
        return;
    }

    _rows[row].lastGoodValue = seq;
}

bool ShortcutDlg::has_duplicate(int row, const QString &seq)
{
    if (seq.isEmpty())
        return false;

    QString norm = QKeySequence(seq).toString();
    for (int i = 0; i < _rows.size(); ++i) {
        if (i == row) continue;
        auto *item = _table->item(i, 1);
        if (!item) continue;
        if (QKeySequence(item->text().trimmed()).toString() == norm)
            return true;
    }
    return false;
}

void ShortcutDlg::on_reset_defaults()
{
    _updating = true;
    for (int i = 0; i < _rows.size(); ++i) {
        _rows[i].lastGoodValue = _rows[i].defaultValue;
        auto *item = _table->item(i, 1);
        if (item) {
            item->setText(_rows[i].defaultValue);
            item->setTextAlignment(Qt::AlignCenter);
        }
    }
    _updating = false;
}

void ShortcutDlg::apply_to_config()
{
    for (int i = 0; i < _rows.size(); ++i) {
        auto *item = _table->item(i, 1);
        if (item)
            *_rows[i].configField = item->text().trimmed();
    }
    AppConfig::Instance().SaveApp();
}

} // namespace dialogs
} // namespace pv
