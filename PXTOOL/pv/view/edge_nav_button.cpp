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

#include "edge_nav_button.h"

#include "../config/appconfig.h"
#include "../theme/thememanager.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace pv {
namespace view {

static const int ButtonSize = 24;
static const int BorderRadius = 3;

static QColor themeColor(const QString &token, const QColor &fallback)
{
    QColor color = AppConfig::Instance().GetThemeColor(token);
    return color.isValid() ? color : fallback;
}

EdgeNavButton::EdgeNavButton(Direction dir, QWidget *parent) :
    QWidget(parent),
    _dir(dir),
    _hovered(false)
{
    setFixedSize(ButtonSize, ButtonSize);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    UpdateTheme();
}

void EdgeNavButton::UpdateTheme()
{
    const bool isDark = AppConfig::Instance().IsDarkStyle();
    _bgColor = isDark ? QColor("#1f1f1f") : QColor("#ffffff");
    _hoverBgColor = isDark ? QColor("#2a2a2a") : QColor("#f3f4f6");
    _borderColor = isDark ? QColor("#444444") : QColor("#d4d4d4");
    _disabledBorderColor = isDark ? QColor("#363636") : QColor("#e0e0e0");
    _arrowColor = isDark ? QColor("#eff0f1") : QColor("#2A2A2A");
    _disabledColor = isDark ? QColor("#555555") : QColor("#9ca3af");
    if (!pv::theme::ThemeManager::isLegacyTheme(AppConfig::Instance().frameOptions.style)) {
        _bgColor = themeColor("@panel-bg", _bgColor);
        _hoverBgColor = themeColor("@bg-overlay", _hoverBgColor);
        _borderColor = themeColor("@border-strong", _borderColor);
        _arrowColor = themeColor("@panel-text", _arrowColor);
    }
    update();
}

void EdgeNavButton::paintEvent(QPaintEvent *event)
{
    (void)event;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor bg = (_hovered && isEnabled()) ? _hoverBgColor : _bgColor;
    const QColor border = isEnabled() ? _borderColor : _disabledBorderColor;

    p.setPen(QPen(border, 1));
    p.setBrush(bg);
    p.drawRoundedRect(rect().adjusted(0.5, 0.5, -0.5, -0.5),
                      BorderRadius, BorderRadius);

    p.setPen(Qt::NoPen);
    p.setBrush(isEnabled() ? _arrowColor : _disabledColor);

    const int cx = width() / 2;
    const int cy = height() / 2;
    const int aw = 5;
    const int ah = 6;

    QPainterPath arrow;
    if (_dir == Previous) {
        arrow.moveTo(cx - aw + 1, cy);
        arrow.lineTo(cx + aw - 1, cy - ah);
        arrow.lineTo(cx + aw - 1, cy + ah);
    } else {
        arrow.moveTo(cx + aw - 1, cy);
        arrow.lineTo(cx - aw + 1, cy - ah);
        arrow.lineTo(cx - aw + 1, cy + ah);
    }
    arrow.closeSubpath();
    p.drawPath(arrow);
}

void EdgeNavButton::enterEvent(QEvent *event)
{
    (void)event;
    _hovered = true;
    update();
}

void EdgeNavButton::leaveEvent(QEvent *event)
{
    (void)event;
    _hovered = false;
    update();
}

void EdgeNavButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isEnabled())
        emit clicked();
}

} // namespace view
} // namespace pv
