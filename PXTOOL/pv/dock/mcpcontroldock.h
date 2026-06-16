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

#include <QList>
#include <QScrollArea>
#include "../ui/uimanager.h"

class QLabel;
class QPushButton;
class QVBoxLayout;
class QGroupBox;

namespace pv {

namespace api {
class McpTransport;
}

namespace dock {

class McpControlDock : public QScrollArea, public IUiWindow
{
    Q_OBJECT

public:
    explicit McpControlDock(QWidget *parent = nullptr);
    ~McpControlDock() override;

    void refresh_status();

    void UpdateLanguage() override;
    void UpdateTheme() override;
    void UpdateFont() override;

private slots:
    void on_open_web_console();
    void on_restart_mcp();
    void on_copy_command();

private:
    void setup_ui();
    void add_command_row(QVBoxLayout *layout, const QString &tool_name,
                         const QString &command);
    void retranslateUi();
    void apply_font();
    pv::api::McpTransport *get_mcp_transport() const;

private:
    QWidget *_inner = nullptr;
    QGroupBox *_web_box = nullptr;
    QGroupBox *_connect_box = nullptr;
    QLabel *_web_desc_label = nullptr;
    QLabel *_connect_desc_label = nullptr;
    QLabel *_status_label = nullptr;
    QLabel *_address_label = nullptr;
    QPushButton *_btn_open_web = nullptr;
    QPushButton *_btn_restart = nullptr;
    QList<QPushButton*> _copy_buttons;
};

} // namespace dock
} // namespace pv
