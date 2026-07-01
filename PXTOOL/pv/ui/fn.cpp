/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2023 DreamSourceLab <support@dreamsourcelab.com>
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

#include "fn.h"
#include <assert.h>
#include <QCoreApplication>
#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QAction>
#include <QToolBar>
#include <QWidget>
#include <QLineEdit>
#include <QTabWidget>
#include <QGroupBox>
#include <QTextEdit>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QDebug>
#include <QFontMetrics>

#include "../config/appconfig.h"
#include "../ui/xtoolbutton.h"

namespace ui
{
    static QString menu_font_family_qss(const QFont &font)
    {
        QString family = font.family();
        family.replace("\\", "\\\\");
        family.replace("\"", "\\\"");
        return family;
    }

    static void apply_menu_style_recursive(QMenu *menu, const QString &style)
    {
        if (menu == nullptr)
            return;

        menu->setStyleSheet(style);
        for (QAction *action : menu->actions()) {
            if (action == nullptr)
                continue;
            if (QMenu *sub = action->menu())
                apply_menu_style_recursive(sub, style);
        }
    }

    static void apply_menu_popup_style(QMenu *menu, const QFont &font)
    {
        if (!menu)
            return;

        set_menu_font(menu, font);

        // QMenu::setFont alone can be ignored by the Windows native style.
        // Keep the popup metrics aligned with sampling bar combo popups.
        const int px = font.pixelSize();
        const int itemHeight = qMax(20, px + 8);
        const QString family = menu_font_family_qss(font);
        const bool dark = AppConfig::Instance().IsDarkStyle();
        const QString fg = dark ? QStringLiteral("#eff0f1")
                                : QStringLiteral("#2A2A2A");
        const QString style = QString(
            "QMenu { color: %2; font-family: \"%3\"; font-size: %1px; font-weight: normal; "
            "padding: 6px 0; }"
            "QMenu::item { color: %2; font-family: \"%3\"; font-size: %1px; font-weight: normal; "
            "padding: 4px 12px; min-height: %4px; border: 1px solid transparent; }"
            "QMenu::item:selected { background-color: #7c3aed; color: #ffffff; }")
            .arg(px)
            .arg(fg)
            .arg(family)
            .arg(itemHeight);
        apply_menu_style_recursive(menu, style);
    }

    void set_font_param(QFont &font, struct FontParam &param)
    {
        font.setPixelSize(qRound(param.size >= 9.0f ? param.size : 9.0f));

        if (param.name != ""){
            font.setFamily(param.name);
        }
    }

    void set_toolbar_font(QToolBar *bar, QFont font)
    {
        assert(bar);

        // Enforce the app-level font family (same logic as set_form_font).
        const QString appFamily = QApplication::font().family();
        if (!appFamily.isEmpty())
            font.setFamily(appFamily);

        auto buttons = bar->findChildren<QToolButton*>();
        for(auto o : buttons)
        { 
            o->setFont(font);
        }

        auto buttons2 = bar->findChildren<QPushButton*>();
        for(auto o : buttons2)
        { 
            o->setFont(font);
        }

        // ComboBoxes: skip setFont() so QSS can control font-size via stylesheet selectors
        // (same approach as sidebar panel labels)
        // auto comboxs = bar->findChildren<QComboBox*>();
        // for(auto o : comboxs)
        // {
        //     o->setFont(font);
        // }

        // Labels: skip setFont() so QSS can control font-size via stylesheet selectors
        // (same approach as sidebar panel labels, which are not overridden by C++)
        // auto labels = bar->findChildren<QLabel*>();
        // for(auto o : labels)
        // {
        //     o->setFont(font);
        // }

        auto actions = bar->findChildren<QAction*>();
        for(auto o : actions)
        { 
            o->setFont(font);
        }
    }

    void set_menu_font(QMenu *menu, QFont font)
    {
        if (menu == nullptr)
            return;

        // Enforce the app-level font family so menu items always use the system
        // CJK font, not whatever the QSS engine may have resolved.
        const QString appFamily = QApplication::font().family();
        if (!appFamily.isEmpty())
            font.setFamily(appFamily);

        menu->setFont(font);

        for (QAction *action : menu->actions()) {
            if (action == nullptr || action->isSeparator())
                continue;
            action->setFont(font);
            if (QMenu *sub = action->menu())
                set_menu_font(sub, font);
        }
    }

    QFont application_menu_font()
    {
        // Start from the app-level font so the family is always the system CJK
        // font; then override only size and weight.
        QFont font = QApplication::font();
        const int px = qMax(12, qRound(AppConfig::Instance().appOptions.fontSize));
        font.setPixelSize(px);
        font.setWeight(QFont::Normal);
        font.setBold(false);
        font.setStyleStrategy(QFont::PreferAntialias);
        return font;
    }

    void apply_application_menu_font(QMenu *menu)
    {
        if (!menu)
            return;

        apply_menu_popup_style(menu, application_menu_font());
    }

    QFont compact_menu_font()
    {
        QFont font = QApplication::font();
        float fSize = AppConfig::Instance().appOptions.fontSize;
        if (fSize > 10)
            fSize = 10;
        font.setPixelSize(qRound(fSize));
        font.setWeight(QFont::Normal);
        font.setBold(false);
        return font;
    }

    void apply_compact_menu_font(QMenu *menu)
    {
        if (!menu)
            return;

        apply_menu_popup_style(menu, compact_menu_font());
    }
    
    void set_form_font(QWidget *wid, QFont font)
    {
        assert(wid);

        // Always use the application-level font family (set explicitly in main.cpp
        // to "Microsoft YaHei UI" on Windows) as the source of truth.
        // Callers pass `this->font()` which may have been resolved by Qt's QSS
        // engine to a different family after qApp->setStyleSheet() is called.
        // Overriding the family here guarantees every widget gets the correct
        // system font regardless of QSS inheritance quirks.
        const QString appFamily = QApplication::font().family();
        if (!appFamily.isEmpty())
            font.setFamily(appFamily);

        auto buttons2 = wid->findChildren<QPushButton*>();
        for(auto o : buttons2)
        { 
            o->setFont(font);
        }

        auto comboxs = wid->findChildren<QComboBox*>();
        for(auto o : comboxs)
        { 
            o->setFont(font);
        }

        auto labels = wid->findChildren<QLabel*>();
        for(auto o : labels)
        { 
            o->setFont(font);
        }

        auto edits = wid->findChildren<QLineEdit*>();
        for(auto o : edits)
        { 
            o->setFont(font);
        }

        auto textEdits = wid->findChildren<QTextEdit*>();
        for(auto o : textEdits)
        { 
            o->setFont(font);
        }

        auto radios = wid->findChildren<QRadioButton*>();
        for(auto o : radios)
        { 
            o->setFont(font);
        }

        auto checks = wid->findChildren<QCheckBox*>();
        for(auto o : checks)
        { 
            o->setFont(font);
        }

        auto spins = wid->findChildren<QSpinBox*>();
        for (auto o : spins)
        {
            o->setFont(font);
        }

        auto dblSpins = wid->findChildren<QDoubleSpinBox*>();
        for (auto o : dblSpins)
        {
            o->setFont(font);
        }

        // Magnify the size.
        font.setPixelSize(font.pixelSize() + 1);

        auto tabs = wid->findChildren<QTabWidget*>();
        for(auto o : tabs)
        { 
            o->setFont(font); 
        }

        auto groups = wid->findChildren<QGroupBox*>();
        for(auto o : groups)
        { 
            o->setFont(font);
        }
    }

    QSize measure_string(QFont font, QString str)
    {
        QFontMetrics fm(font); 
        QRect rc = fm.boundingRect(str);
        return rc.size();
    }

    void adjust_label_size(QLabel *ctrl, AdjustSizeAction action)
    {
        QSize sz = measure_string(ctrl->font(), ctrl->text());

        if (action == ADJUST_WIDTH){
            ctrl->setFixedWidth(sz.width() + 5);
        }
        else if (action == ADJUST_HEIGHT){
            ctrl->setFixedHeight(sz.height() + 5);
        }
        else{
            ctrl->setFixedHeight(sz.height() + 5);
            ctrl->setFixedWidth(sz.width() + 5);
        }
    }

    QIcon application_icon()
    {
        static const int kSizes[] = {16, 24, 32, 48, 64, 128, 256};

        auto icon_from_ico = [&](const QString &path) {
            QIcon icon;
            for (int sz : kSizes)
                icon.addFile(path, QSize(sz, sz));
            return icon;
        };

        const QString besideExe =
            QCoreApplication::applicationDirPath()
            + QStringLiteral("/win-app-logo.ico");
        if (QFile::exists(besideExe)) {
            const QIcon fromFile = icon_from_ico(besideExe);
            if (!fromFile.availableSizes().isEmpty())
                return fromFile;
        }

        QIcon fromQrc;
        for (int sz : kSizes)
            fromQrc.addFile(QStringLiteral(":/icons/app_logo.ico"), QSize(sz, sz));
        if (!fromQrc.availableSizes().isEmpty())
            return fromQrc;

        return icon_from_ico(besideExe);
    }

} // namespace ui
