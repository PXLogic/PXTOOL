/*
 * This file is part of the PXTOOL project.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <QHash>
#include <QList>
#include <QString>

namespace pv {
namespace theme {

struct ThemeId {
    static constexpr const char *Dark = "dark";
    static constexpr const char *Light = "light";
    static constexpr const char *Atom = "atom";
    static constexpr const char *Ayu = "ayu";
    static constexpr const char *DarkCards = "dark_cards";
    static constexpr const char *LightCards = "light_cards";
};

struct ThemeInfo {
    QString id;
    QString label;
    QString jsonResource;
    bool legacy;
    bool darkLike;
};

class ThemeManager
{
public:
    static QList<ThemeInfo> builtInThemes();
    static ThemeInfo themeInfo(const QString &id);
    static QString normalizeId(const QString &id);
    static bool isKnownTheme(const QString &id);
    static bool isLegacyTheme(const QString &id);
    static bool isDarkTheme(const QString &id);
    static QHash<QString, QString> legacyTokens(const QString &id);
    static bool loadTokenPreset(const QString &id, QHash<QString, QString> &tokens, QString *errorMessage = nullptr);
    static bool buildStyleSheet(const QString &id, QHash<QString, QString> &tokens, QString &styleSheet, QString *errorMessage = nullptr);
    static void replaceTokens(QString &content, const QHash<QString, QString> &tokens);
};

} // namespace theme
} // namespace pv
