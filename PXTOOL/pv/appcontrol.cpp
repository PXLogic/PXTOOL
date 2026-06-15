/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 * 
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
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

#include "appcontrol.h"

#include <libsigrok.h>
#include <libsigrokdecode.h>
#include <QDir>
#include <QCoreApplication>
#include <QWidget>
#include <string>
#include <assert.h>
#include "sigsession.h"
#include "api/iapp_service.h"
#include "api/app_service.h"
#include "api/mcp_transport.h"
#if __has_include("api/rpc_dispatcher.h")
#include "api/rpc_dispatcher.h"
#define PXTOOL_HAS_RPC_DISPATCHER 1
#else
#define PXTOOL_HAS_RPC_DISPATCHER 0
#endif
#include "dsvdef.h"
#include "config/appconfig.h"
#include "log.h"
#include "utility/path.h"
#include "utility/encoding.h"
#include "c_decoder_registry.h"

AppControl::AppControl()
{
    _topWindow = NULL; 
    _session = new pv::SigSession();
}

AppControl::AppControl(AppControl &o)
{
    (void)o;
}
 
AppControl::~AppControl()
{ 
   // DESTROY_OBJECT(_session);
}

AppControl* AppControl::Instance()
{
    static AppControl *ins = NULL;
    if (ins == NULL){
        ins = new AppControl();
    }
    return ins;
}

void AppControl::Destroy(){
     
} 

bool AppControl::Init()
{  
    pv::encoding::init();

    QString qs;
    std::string cs;

    qs = GetAppDataDir();
    cs = pv::path::ToUnicodePath(qs);
    dsv_info("GetAppDataDir:\"%s\"", cs.c_str());
    cs = pv::path::ConvertPath(qs);
    ds_set_user_data_dir(cs.c_str());

    qs = GetFirmwareDir();
    cs = pv::path::ToUnicodePath(qs);
    dsv_info("GetFirmwareDir:\"%s\"", cs.c_str());

    qs = GetUserDataDir();
    cs = pv::path::ToUnicodePath(qs);
    dsv_info("GetUserDataDir:\"%s\"", cs.c_str());

    qs = GetDecodeScriptDir();
    cs = pv::path::ToUnicodePath(qs);
    dsv_info("GetDecodeScriptDir:\"%s\"", cs.c_str());
    //---------------end print directorys.

    _session->init();

    srd_log_set_context(dsv_log_context());

#if defined(_WIN32)
    // On Windows (release & debug), point Python at the bundled stdlib that
    // lives in <app_dir>/lib/pythonX.Y/ so that no system Python installation
    // is required on the end-user's machine.
    {
        QString appDir = QCoreApplication::applicationDirPath();
        QDir pyLibDir(appDir + "/lib");
        if (pyLibDir.exists()) {
            // Bundled stdlib found — use it.
            const wchar_t *pyhome = reinterpret_cast<const wchar_t*>(appDir.utf16());
            srd_set_python_home(pyhome);
            dsv_info("Python home -> bundled: %s", appDir.toUtf8().constData());
        }
#if defined(DEBUG_INFO)
        else {
            // Development fallback when stdlib is not bundled yet.
            QString devHome = "c:/python";
            if (QDir(devHome).exists()) {
                const wchar_t *pyhome = reinterpret_cast<const wchar_t*>(devHome.utf16());
                srd_set_python_home(pyhome);
                dsv_info("Python home -> dev fallback: c:/python");
            }
        }
#endif
    }
#endif
    
    //the python script path of decoder
    char path[256] = {0};
    QString dir = GetDecodeScriptDir();   
    strcpy(path, dir.toUtf8().data());

    // Initialise libsigrokdecode
    if (srd_init(path) != SRD_OK)
    { 
        dsv_err("ERROR: libsigrokdecode init failed.");
        return false;
    }

    // Load the protocol decoders
    if (srd_decoder_load_all() != SRD_OK)
    {
        dsv_err("ERROR: load the protocol decoders failed.");
        return false;
    }

    // Two-tier C decoder loading:
    //   1) Bundled decoders shipped inside the app (always loaded first).
    //      On macOS: <App>.app/Contents/Resources/share/PXTOOL/cdecoders/.
    //   2) User-installed decoders in the writable data directory
    //      (~/Library/Application Support/.../cdecoders on macOS, etc.).
    //      Any ID present here is skipped because the bundled one already
    //      registered it — but a user may add new IDs the app didn't ship.
    QString bundled_cdir = GetBundledCDecodeDir();
    if (!bundled_cdir.isEmpty()) {
        std::string s = pv::path::ConvertPath(bundled_cdir);
        dsv_info("Loading bundled C decoders from: %s", s.c_str());
        pv::cdecoders::CDecoderRegistry::instance().load_c_decoders(s);
    }

    QString cdir = GetCDecodeDir();
    std::string cdir_str = pv::path::ConvertPath(cdir);
    dsv_info("Loading user C decoders from: %s", cdir_str.c_str());
    pv::cdecoders::CDecoderRegistry::instance().load_c_decoders(cdir_str);

    return true;
}

bool AppControl::Start()
{  
    _session->Open();
    _app_service = new pv::api::AppService(this);
    _app_service->initialize();
#if PXTOOL_HAS_RPC_DISPATCHER
    _rpc_dispatcher = new pv::api::RpcDispatcher(_app_service);
    _mcp_transport = new pv::api::McpTransport(_rpc_dispatcher, 10110);
    _mcp_transport->start();
#else
    dsv_warn("MCP RpcDispatcher is not available; transport start deferred until Task 3.");
#endif
    return true;
}

 void AppControl::Stop()
 {
    if (_mcp_transport) {
        _mcp_transport->stop();
        delete _mcp_transport;
        _mcp_transport = nullptr;
    }
#if PXTOOL_HAS_RPC_DISPATCHER
    delete _rpc_dispatcher;
#endif
    _rpc_dispatcher = nullptr;
    if (_app_service) {
        _app_service->shutdown();
        delete _app_service;
        _app_service = nullptr;
    }
    _session->Close();  
 }

void AppControl::UnInit()
{
    pv::cdecoders::CDecoderRegistry::instance().unload_all();

    // Destroy libsigrokdecode
    srd_exit();

    _session->uninit();
}

bool AppControl::TopWindowIsMaximized()
{
    if (_topWindow != NULL){
        return _topWindow->isMaximized();
    }
    return false;
}

pv::api::IAppService* AppControl::GetAppService()
{
    return _app_service;
}
