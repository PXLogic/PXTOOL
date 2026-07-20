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

#ifndef DSVIEW_PV_DIALOGS_INPUTOUTPUTOPTIONEDITOR_H
#define DSVIEW_PV_DIALOGS_INPUTOUTPUTOPTIONEDITOR_H

#include <QVariant>

extern "C" {
#include "libsigrok.h"
}

class QWidget;

namespace pv {
namespace dialogs {
namespace detail {

QWidget *makeInputOutputOptionEditor(const sr_option *option,
                                     const QVariant &value,
                                     QWidget *parent);
QVariant inputOutputOptionEditorValue(const sr_option *option,
                                      const QWidget *editor);

} // namespace detail
} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_DIALOGS_INPUTOUTPUTOPTIONEDITOR_H
