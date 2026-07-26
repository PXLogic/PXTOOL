/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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


#ifndef DSVIEW_PV_VIEW_DEVMODE_H
#define DSVIEW_PV_VIEW_DEVMODE_H
 
#include <list>
#include <utility>
#include <map>
#include <set>
#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QVector> 
#include <QLabel>
#include <libsigrok.h> 

#include "../interface/icallbacks.h"
#include "../ui/xtoolbutton.h"
#include "../ui/dscombobox.h"
#include "../ui/uimanager.h"

struct dev_mode_name{
    int _mode;
    const char *_logo;
};
 
class DeviceAgent;

namespace pv {

class SigSession;

namespace view {

//devece work mode select list
class DevMode : public QWidget, public IUiWindow
{
	Q_OBJECT

private:
    static const int GRID_COLS = 3;

public:
    DevMode(QWidget *parent, SigSession *session);

    ~DevMode();

private:
    void paintEvent(QPaintEvent *event) override;
    void sync_mode_button(int mode, const QString &iconPath);
    void sync_mode_button_width();
    void apply_mode_style();

private:
    void mousePressEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    const dev_mode_name* get_mode_name(int mode);

     //IUiWindow
    void UpdateLanguage() override;
    void UpdateTheme() override;
    void UpdateFont() override;

public slots:
    void set_device();
    void on_mode_change(int index);
    void on_close();

private slots:

 

private:
    SigSession *_session;
    DsComboBox      *_mode_btn;
    bool             _updating_mode;
    QPoint          _mouse_point;
    XToolButton     *_close_button;
    bool            _bFile;

    DeviceAgent     *_device_agent;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_DEVMODE_H
