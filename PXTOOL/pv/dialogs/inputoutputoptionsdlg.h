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

#ifndef DSVIEW_PV_DIALOGS_INPUTOUTPUTOPTIONSDLG_H
#define DSVIEW_PV_DIALOGS_INPUTOUTPUTOPTIONSDLG_H

#include <QMap>

#include "dsdialog.h"
#include "../data/iooptions.h"

class QDialogButtonBox;
class QWidget;

namespace pv {
namespace dialogs {

class InputOutputOptionsDlg final : public DSDialog {
    Q_OBJECT

public:
    InputOutputOptionsDlg(const QString &title,
                          const sr_option *const *options,
                          QWidget *parent = nullptr);
    InputOutputOptionsDlg(const QString &title,
                          const sr_option *const *definitions,
                          const pv::data::IoOptions &initialOptions,
                          QWidget *parent = nullptr);

    const pv::data::IoOptions &options() const;

protected:
    void accept() override;

private:
    pv::data::IoOptions options_;
    QMap<QString, const sr_option *> definitions_;
    QMap<QString, QWidget *> editors_;
    QDialogButtonBox *button_box_;
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_DIALOGS_INPUTOUTPUTOPTIONSDLG_H
