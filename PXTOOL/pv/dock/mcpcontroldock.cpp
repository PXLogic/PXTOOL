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

#include "mcpcontroldock.h"

#include "../api/mcp_transport.h"
#include "../appcontrol.h"
#include "../config/appconfig.h"
#include "../ui/fn.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QFrame>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

namespace pv {
namespace dock {

static const int MCP_PORT = 10110;

McpControlDock::McpControlDock(QWidget *parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet("QScrollArea { border: none; }");

    _inner = new QWidget(this);
    _inner->setObjectName("mcpControlWidget");
    setWidget(_inner);

    setup_ui();
    refresh_status();
    ADD_UI(this);
}

McpControlDock::~McpControlDock()
{
    REMOVE_UI(this);
}

void McpControlDock::setup_ui()
{
    auto *outer = new QVBoxLayout(_inner);
    outer->setContentsMargins(12, 8, 12, 8);
    outer->setSpacing(8);

    auto *web_box = new QGroupBox(tr("Web Console"), _inner);
    auto *web_layout = new QVBoxLayout(web_box);
    web_layout->setContentsMargins(8, 18, 8, 8);
    web_layout->setSpacing(6);

    auto *web_desc = new QLabel(
        tr("Open the local MCP console for PXTOOL capture and decoder control."),
        web_box);
    web_desc->setWordWrap(true);
    web_layout->addWidget(web_desc);

    _btn_open_web = new QPushButton(tr("Open Web Console"), web_box);
    _btn_open_web->setMinimumHeight(28);
    connect(_btn_open_web, &QPushButton::clicked,
            this, &McpControlDock::on_open_web_console);
    web_layout->addWidget(_btn_open_web);
    outer->addWidget(web_box);

    auto *connect_box = new QGroupBox(tr("Connect AI Tool"), _inner);
    auto *connect_layout = new QVBoxLayout(connect_box);
    connect_layout->setContentsMargins(8, 18, 8, 8);
    connect_layout->setSpacing(6);

    auto *status_row = new QHBoxLayout();
    _status_label = new QLabel(connect_box);
    status_row->addWidget(_status_label);
    status_row->addStretch();
    _address_label = new QLabel(connect_box);
    status_row->addWidget(_address_label);
    connect_layout->addLayout(status_row);

    auto *desc = new QLabel(
        tr("Run one of these commands in your AI tool environment."),
        connect_box);
    desc->setWordWrap(true);
    connect_layout->addWidget(desc);

    const QString port = QString::number(MCP_PORT);
    add_command_row(connect_layout, QStringLiteral("Claude Code"),
                    QString("claude mcp add --transport http pxtool http://127.0.0.1:%1").arg(port));
    add_command_row(connect_layout, QStringLiteral("Codex"),
                    QString("codex mcp add --url http://127.0.0.1:%1 pxtool").arg(port));
    add_command_row(connect_layout, QStringLiteral("OpenCode"),
                    QString("opencode --mcp http://127.0.0.1:%1").arg(port));

    _btn_restart = new QPushButton(tr("Restart MCP Service"), connect_box);
    _btn_restart->setMinimumHeight(28);
    connect(_btn_restart, &QPushButton::clicked,
            this, &McpControlDock::on_restart_mcp);
    connect_layout->addWidget(_btn_restart);
    outer->addWidget(connect_box);

    outer->addStretch(1);
    apply_font();
}

void McpControlDock::add_command_row(QVBoxLayout *layout,
                                     const QString &tool_name,
                                     const QString &command)
{
    auto *name = new QLabel(tool_name, _inner);
    layout->addWidget(name);

    auto *frame = new QFrame(_inner);
    frame->setObjectName("mcpCmdFrame");
    frame->setFrameShape(QFrame::StyledPanel);
    auto *row = new QHBoxLayout(frame);
    row->setContentsMargins(8, 4, 4, 4);
    row->setSpacing(6);

    auto *cmd = new QLabel(command, frame);
    cmd->setObjectName("mcpCommandLabel");
    cmd->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cmd->setWordWrap(true);
    row->addWidget(cmd, 1);

    auto *copy = new QPushButton(tr("Copy"), frame);
    copy->setObjectName("mcpCopyButton");
    copy->setMinimumHeight(24);
    copy->setProperty("cmd_text", command);
    copy->setToolTip(tr("Copy command"));
    connect(copy, &QPushButton::clicked, this, &McpControlDock::on_copy_command);
    row->addWidget(copy);

    layout->addWidget(frame);
}

void McpControlDock::refresh_status()
{
    auto *transport = get_mcp_transport();
    const bool running = transport && transport->is_running();

    if (running) {
        _status_label->setText(tr("Running"));
        _status_label->setProperty("state", "running");
        _address_label->setText(QString("127.0.0.1:%1").arg(MCP_PORT));
    } else {
        _status_label->setText(tr("Stopped"));
        _status_label->setProperty("state", "stopped");
        _address_label->setText("-");
    }
    _status_label->style()->unpolish(_status_label);
    _status_label->style()->polish(_status_label);
}

void McpControlDock::on_open_web_console()
{
    QDesktopServices::openUrl(QUrl(QString("http://127.0.0.1:%1/").arg(MCP_PORT)));
}

void McpControlDock::on_restart_mcp()
{
    auto *transport = get_mcp_transport();
    if (!transport)
        return;

    transport->stop();
    transport->start();
    refresh_status();
}

void McpControlDock::on_copy_command()
{
    auto *button = qobject_cast<QPushButton*>(sender());
    if (!button)
        return;

    const QString cmd = button->property("cmd_text").toString();
    if (!cmd.isEmpty())
        QGuiApplication::clipboard()->setText(cmd);
}

void McpControlDock::UpdateLanguage()
{
}

void McpControlDock::UpdateTheme()
{
    refresh_status();
}

void McpControlDock::UpdateFont()
{
    apply_font();
}

void McpControlDock::apply_font()
{
    QFont font = this->font();
    font.setPixelSize(qRound(AppConfig::Instance().appOptions.fontSize));
    ui::set_form_font(this, font);
}

pv::api::McpTransport *McpControlDock::get_mcp_transport() const
{
    auto *app = ::AppControl::Instance();
    return app ? app->get_mcp_transport() : nullptr;
}

} // namespace dock
} // namespace pv
