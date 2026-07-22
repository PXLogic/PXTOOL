/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "inputoutputoptionsdlg.h"

#include <stdexcept>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QWidget>

#include "inputoutputoptioneditor.h"

namespace pv {
namespace dialogs {

InputOutputOptionsDlg::InputOutputOptionsDlg(const QString &title,
                                             const sr_option *const *options,
                                             QWidget *parent) :
    InputOutputOptionsDlg(title, options, pv::data::IoOptions(options), parent)
{
}

InputOutputOptionsDlg::InputOutputOptionsDlg(const QString &title,
                                             const sr_option *const *options,
                                             const pv::data::IoOptions &initialOptions,
                                             QWidget *parent) :
    DSDialog(parent),
    options_(initialOptions),
    button_box_(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this))
{
    setObjectName("inputOutputOptionsDialog");
    setTitle(title);
    setTitleTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    SetTitleSpace(8);
    layout()->setSpacing(0);
    layout()->setAlignment(Qt::AlignTop);
    layout()->setContentsMargins(0, 5, 0, 0);

    auto *top_sep = new QWidget(this);
    top_sep->setObjectName("device_options_divider");
    top_sep->setFixedHeight(1);
    layout()->addWidget(top_sep);

    auto *form = new QFormLayout;
    form->setContentsMargins(16, 12, 16, 20);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    for (int index = 0; options && options[index]; index++) {
        const sr_option *option = options[index];
        const QString id = QString::fromUtf8(option->id);
        QWidget *editor = detail::makeInputOutputOptionEditor(option, options_.value(id), this);
        editor->setToolTip(QString::fromUtf8(option->desc ? option->desc : ""));
        definitions_.insert(id, option);
        editors_.insert(id, editor);
        form->addRow(QString::fromUtf8(option->name ? option->name : option->id), editor);
    }
    layout()->addLayout(form);

    auto *bot_sep = new QWidget(this);
    bot_sep->setObjectName("device_options_divider");
    bot_sep->setFixedHeight(1);
    layout()->addWidget(bot_sep);

    if (QPushButton *ok = button_box_->button(QDialogButtonBox::Ok))
        ok->setObjectName("device_ok_btn");
    if (QPushButton *cancel = button_box_->button(QDialogButtonBox::Cancel))
        cancel->setObjectName("device_cancel_btn");

    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(12, 10, 12, 10);
    footer->setSpacing(6);
    footer->addStretch();
    footer->addWidget(button_box_);
    layout()->addLayout(footer);

    connect(button_box_, &QDialogButtonBox::accepted, this, &InputOutputOptionsDlg::accept);
    connect(button_box_, &QDialogButtonBox::rejected, this, &InputOutputOptionsDlg::reject);
}

const pv::data::IoOptions &InputOutputOptionsDlg::options() const
{
    return options_;
}

void InputOutputOptionsDlg::accept()
{
    try {
        pv::data::IoOptions updated = options_;
        for (auto definition = definitions_.cbegin(); definition != definitions_.cend(); ++definition)
            updated.set(definition.key(), detail::inputOutputOptionEditorValue(
                definition.value(), editors_.value(definition.key())));
        options_ = updated;
    } catch (const std::invalid_argument &error) {
        QMessageBox::warning(this, tr("Invalid Option"), QString::fromUtf8(error.what()));
        return;
    }

    DSDialog::accept();
}

} // namespace dialogs
} // namespace pv
