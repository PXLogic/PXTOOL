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

#include "thememanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>

namespace pv {
namespace theme {

QList<ThemeInfo> ThemeManager::builtInThemes()
{
    return {
        {ThemeId::Dark, "Dark", QString(), true, true},
        {ThemeId::Light, "Light", QString(), true, false},
        {ThemeId::Atom, "Atom One Dark", ":/themes/atom.json", false, true},
        {ThemeId::Ayu, "Ayu Light", ":/themes/ayu.json", false, false},
        {ThemeId::DarkCards, "Dark Colored Cards", ":/themes/dark_cards.json", false, true},
        {ThemeId::LightCards, "Light Colored Cards", ":/themes/light_cards.json", false, false},
    };
}

ThemeInfo ThemeManager::themeInfo(const QString &id)
{
    const QString normalizedId = normalizeId(id);
    const QList<ThemeInfo> themes = builtInThemes();
    for (const ThemeInfo &theme : themes) {
        if (theme.id == normalizedId)
            return theme;
    }
    return themes.first();
}

QString ThemeManager::normalizeId(const QString &id)
{
    const QString trimmedId = id.trimmed();
    const QList<ThemeInfo> themes = builtInThemes();
    for (const ThemeInfo &theme : themes) {
        if (theme.id == trimmedId)
            return theme.id;
    }
    return ThemeId::Dark;
}

bool ThemeManager::isKnownTheme(const QString &id)
{
    const QList<ThemeInfo> themes = builtInThemes();
    for (const ThemeInfo &theme : themes) {
        if (theme.id == id)
            return true;
    }
    return false;
}

bool ThemeManager::isLegacyTheme(const QString &id)
{
    return themeInfo(id).legacy;
}

bool ThemeManager::isDarkTheme(const QString &id)
{
    return themeInfo(id).darkLike;
}

QHash<QString, QString> ThemeManager::legacyTokens(const QString &id)
{
    if (normalizeId(id) == ThemeId::Light) {
        return {
            {"@bg-base", "#F8F8F8"},
        };
    }

    return {
        {"@bg-base", "#262626"},
    };
}

bool ThemeManager::loadTokenPreset(const QString &id, QHash<QString, QString> &tokens, QString *errorMessage)
{
    const ThemeInfo theme = themeInfo(id);
    if (theme.legacy) {
        tokens = legacyTokens(theme.id);
        return true;
    }

    QFile file(theme.jsonResource);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        if (errorMessage)
            *errorMessage = QString("Unable to open theme preset: %1").arg(theme.jsonResource);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage)
            *errorMessage = QString("Invalid theme preset: %1").arg(theme.jsonResource);
        return false;
    }

    tokens.clear();
    const QJsonObject root = doc.object();
    QJsonObject tokenObject = root;
    if (root.value("tokens").isObject())
        tokenObject = root.value("tokens").toObject();
    for (auto it = tokenObject.constBegin(); it != tokenObject.constEnd(); ++it)
        tokens.insert(it.key(), it.value().toString());

    return true;
}

bool ThemeManager::buildStyleSheet(const QString &id, QHash<QString, QString> &tokens, QString &styleSheet, QString *errorMessage)
{
    const ThemeInfo theme = themeInfo(id);
    if (!loadTokenPreset(theme.id, tokens, errorMessage))
        return false;

    const QString qssResource = theme.darkLike ? ":/dark.qss" : ":/light.qss";
    QFile file(qssResource);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        if (errorMessage)
            *errorMessage = QString("Unable to open stylesheet: %1").arg(qssResource);
        return false;
    }

    styleSheet = QString::fromUtf8(file.readAll());
    replaceTokens(styleSheet, tokens);
    return true;
}

void ThemeManager::replaceTokens(QString &content, const QHash<QString, QString> &tokens)
{
    QList<QString> keys = tokens.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &left, const QString &right) {
        return left.size() > right.size();
    });

    for (const QString &key : keys)
        content.replace(key, tokens.value(key));
}

} // namespace theme
} // namespace pv
