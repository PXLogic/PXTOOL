/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2025 DreamSourceLab <support@dreamsourcelab.com>
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
 */

#ifndef DSVIEW_PV_VIEW_EDGE_NAV_BUTTON_H
#define DSVIEW_PV_VIEW_EDGE_NAV_BUTTON_H

#include <QColor>
#include <QEnterEvent>
#include <QWidget>

namespace pv {
namespace view {

class EdgeNavButton : public QWidget
{
    Q_OBJECT

public:
    enum Direction {
        Previous,
        Next
    };

    explicit EdgeNavButton(Direction dir, QWidget *parent = nullptr);

    void UpdateTheme();

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    Direction _dir;
    bool _hovered;
    QColor _bgColor;
    QColor _hoverBgColor;
    QColor _borderColor;
    QColor _disabledBorderColor;
    QColor _arrowColor;
    QColor _disabledColor;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_EDGE_NAV_BUTTON_H
