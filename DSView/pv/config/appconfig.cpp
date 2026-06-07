/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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

#include "appconfig.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QLocale>
#include <QDir> 
#include <assert.h>
#include <QStandardPaths>
#include "../log.h"
  
#define MAX_PROTOCOL_FORMAT_LIST 15

StringPair::StringPair(const std::string &key, const std::string &value)
{
    m_key = key;
    m_value = value;
}

//------------function
static QString FormatArrayToString(std::vector<StringPair> &protocolFormats)
{
    QString str;

    for (StringPair &o : protocolFormats){
         if (!str.isEmpty()){
             str += ";";
         } 
         str += o.m_key.c_str();
         str += "=";
         str += o.m_value.c_str(); 
    }

    return str;
}

static void StringToFormatArray(const QString &str, std::vector<StringPair> &protocolFormats)
{
    QStringList arr = str.split(";");
    for (int i=0; i<arr.size(); i++){
        QString line = arr[i];
        if (!line.isEmpty()){
            QStringList vs = line.split("=");
            if (vs.size() == 2){
                protocolFormats.push_back(StringPair(vs[0].toStdString(), vs[1].toStdString()));
            }
        }
    }
}

//----------------read write field
static void getFiled(const char *key, QSettings &st, QString &f, const char *dv)
{
    f = st.value(key, dv).toString();
}

static void setFiled(const char *key, QSettings &st, QString f)
{
    st.setValue(key, f);
}

static void getFiled(const char *key, QSettings &st, int &f, int dv)
{
    f = st.value(key, dv).toInt();
}

static void setFiled(const char *key, QSettings &st, int f)
{
    st.setValue(key, f);
}

static void getFiled(const char *key, QSettings &st, bool &f, bool dv)
{
    f = st.value(key, dv).toBool();
}

static void setFiled(const char *key, QSettings &st, bool f){
    st.setValue(key, f);
}

static void getFiled(const char *key, QSettings &st, float &f, float dv)
{
    f = st.value(key, dv).toInt();
}

static void setFiled(const char *key, QSettings &st, float f)
{
    st.setValue(key, f);
}

///------ app
static void _loadApp(AppOptions &o, QSettings &st)
{
    st.beginGroup("Application"); 
    getFiled("quickScroll", st, o.quickScroll, true);
    getFiled("warnofMultiTrig", st, o.warnofMultiTrig, true);
    getFiled("originalData", st, o.originalData, false);
    getFiled("ableSaveLog", st, o.ableSaveLog, false);
    getFiled("appendLogMode", st, o.appendLogMode, false);
    getFiled("logLevel", st, o.logLevel, 3);
    getFiled("transDecoderDlg", st, o.transDecoderDlg, true);
    getFiled("trigPosDisplayInMid", st, o.trigPosDisplayInMid, true);
    getFiled("displayProfileInBar", st, o.displayProfileInBar, false);
    getFiled("swapBackBufferAlways", st, o.swapBackBufferAlways, false);
    getFiled("fontSize", st, o.fontSize, 9.0);
    getFiled("autoScrollLatestData", st, o.autoScrollLatestData, true);
    getFiled("triggersVisible", st, o.triggersVisible, true);
    getFiled("signalHeightMultiple", st, o.signalHeightMultiple, 0);
    getFiled("version", st, o.version, 1);

    o.warnofMultiTrig = true;

    QString fmt;
    getFiled("protocalFormats", st, fmt, "");
    if (fmt != ""){
        StringToFormatArray(fmt, o.m_protocolFormats);
    }

    float minSize = 0;
    float maxSize = 0;
    AppConfig::GetFontSizeRange(&minSize, &maxSize);

    // Reset font size when:
    //  • fresh install (version == 1, no config file yet), OR
    //  • saved value is outside the now-unified [7, 15] range, OR
    //  • config predates version 4 — the release that extended the Windows
    //    font range beyond 9 and added DPI-aware defaults.  Users upgrading
    //    from version ≤ 3 will have fontSize ≤ 9 (the old Windows cap),
    //    which is too small for Chinese on 96-DPI / 100%-scaled displays.
    if (o.version < APP_CONFIG_VERSION || o.fontSize < minSize || o.fontSize > maxSize)
    {
        o.fontSize = AppConfig::GetDefaultFontSize();
    }

#ifdef Q_OS_MACOS
    if (o.fontSize == 9.0f)
        o.fontSize = AppConfig::GetDefaultFontSize();
#endif
   
    st.endGroup();
}

static void _saveApp(AppOptions &o, QSettings &st)
{
    st.beginGroup("Application");
    setFiled("quickScroll", st, o.quickScroll);
    setFiled("warnofMultiTrig", st, o.warnofMultiTrig);
    setFiled("originalData", st, o.originalData);
    setFiled("ableSaveLog", st, o.ableSaveLog);
    setFiled("appendLogMode", st, o.appendLogMode);
    setFiled("logLevel", st, o.logLevel);
    setFiled("transDecoderDlg", st, o.transDecoderDlg);
    setFiled("trigPosDisplayInMid", st, o.trigPosDisplayInMid);
    setFiled("displayProfileInBar", st, o.displayProfileInBar);
    setFiled("swapBackBufferAlways", st, o.swapBackBufferAlways);
    setFiled("fontSize", st, o.fontSize);
    setFiled("autoScrollLatestData", st, o.autoScrollLatestData);
    setFiled("triggersVisible", st, o.triggersVisible);
    setFiled("signalHeightMultiple", st, o.signalHeightMultiple);
    setFiled("version", st, APP_CONFIG_VERSION);

    QString fmt =  FormatArrayToString(o.m_protocolFormats);
    setFiled("protocalFormats", st, fmt);
    st.endGroup();  
}

//-----frame

static void _loadDockOptions(DockOptions &o, QSettings &st, const char *group)
{
    st.beginGroup(group);
    getFiled("decodeDoc", st, o.decodeDock, false);
    getFiled("triggerDoc", st, o.triggerDock, false);
    getFiled("measureDoc", st, o.measureDock, false);
    getFiled("searchDoc", st, o.searchDock, false);
    st.endGroup();
}

static void _saveDockOptions(DockOptions &o, QSettings &st, const char *group)
{
    st.beginGroup(group);
    setFiled("decodeDoc", st, o.decodeDock);
    setFiled("triggerDoc", st, o.triggerDock);
    setFiled("measureDoc", st, o.measureDock);
    setFiled("searchDoc", st, o.searchDock);
    st.endGroup();
}

static void _loadFrame(FrameOptions &o, QSettings &st)
{
    st.beginGroup("MainFrame"); 
    getFiled("style", st, o.style, THEME_STYLE_DARK);
    getFiled("language", st, o.language, -1);
    getFiled("isMax", st, o.isMax, false);  
    getFiled("left", st, o.left, 0);
    getFiled("top", st, o.top, 0);
    getFiled("right", st, o.right, 0);
    getFiled("bottom", st, o.bottom, 0);
    getFiled("x", st, o.x, NO_POINT_VALUE);
    getFiled("y", st, o.y, NO_POINT_VALUE);
    getFiled("ox", st, o.ox, NO_POINT_VALUE);
    getFiled("oy", st, o.oy, NO_POINT_VALUE);
    getFiled("displayName", st, o.displayName, "");

    _loadDockOptions(o._logicDock, st, "LOGIC_DOCK");
    _loadDockOptions(o._analogDock, st, "ANALOG_DOCK");
    _loadDockOptions(o._dsoDock, st, "DSO_DOCK");

    o.windowState = st.value("windowState", QByteArray()).toByteArray();
    st.endGroup();

    if (o.language == -1 || (o.language != LAN_CN && o.language != LAN_TW && o.language != LAN_EN)){
        //get local language
        QLocale locale;

        if (locale.language() == QLocale::Chinese){
            // Use script/country to distinguish Simplified and Traditional Chinese.
            if (locale.script() == QLocale::TraditionalChineseScript ||
                locale.country() == QLocale::Taiwan ||
                locale.country() == QLocale::HongKong ||
                locale.country() == QLocale::Macau){
                o.language = LAN_TW;
            }
            else{
                o.language = LAN_CN;
            }
        }
        else{
            o.language = LAN_EN;
        }
    }
}

static void _saveFrame(FrameOptions &o, QSettings &st)
{
    st.beginGroup("MainFrame");
    setFiled("style", st, o.style);
    setFiled("language", st, o.language);
    setFiled("isMax", st, o.isMax);  
    setFiled("left", st, o.left);
    setFiled("top", st, o.top);
    setFiled("right", st, o.right);
    setFiled("bottom", st, o.bottom);
    setFiled("x", st, o.x);
    setFiled("y", st, o.y);
    setFiled("ox", st, o.ox);
    setFiled("oy", st, o.oy);
    setFiled("displayName", st, o.displayName);

    st.setValue("windowState", o.windowState); 

    _saveDockOptions(o._logicDock, st, "LOGIC_DOCK");
    _saveDockOptions(o._analogDock, st, "ANALOG_DOCK");
    _saveDockOptions(o._dsoDock, st, "DSO_DOCK");
    
    st.endGroup();
}

//------history
static void _loadHistory(UserHistory &o, QSettings &st)
{
    st.beginGroup("UserHistory");
    getFiled("exportDir", st, o.exportDir, ""); 
    getFiled("saveDir", st, o.saveDir, ""); 
    getFiled("showDocuments", st, o.showDocuments, true);
    getFiled("screenShotPath", st, o.screenShotPath, ""); 
    getFiled("sessionDir", st, o.sessionDir, ""); 
    getFiled("openDir", st, o.openDir, ""); 
    getFiled("protocolExportPath", st, o.protocolExportPath, ""); 
    getFiled("exportFormat", st, o.exportFormat, ""); 
    st.endGroup();
}
 
static void _saveHistory(UserHistory &o, QSettings &st)
{
    st.beginGroup("UserHistory");
    setFiled("exportDir", st, o.exportDir); 
    setFiled("saveDir", st, o.saveDir); 
    setFiled("showDocuments", st, o.showDocuments); 
    setFiled("screenShotPath", st, o.screenShotPath); 
    setFiled("sessionDir", st, o.sessionDir); 
    setFiled("openDir", st, o.openDir); 
    setFiled("protocolExportPath", st, o.protocolExportPath);
    setFiled("exportFormat", st, o.exportFormat); 
    st.endGroup();
}

/*
//------font
static void _loadFont(FontOptions &o, QSettings &st)
{
    st.beginGroup("FontSetting");
    getFiled("toolbarName", st, o.toolbar.name, "");
    getFiled("toolbarSize", st, o.toolbar.size, 9);
    getFiled("channelLabelName", st, o.channelLabel.name, "");
    getFiled("channelLabelSize", st, o.channelLabel.size, 9);
    getFiled("channelBodyName", st, o.channelBody.name, "");
    getFiled("channelBodySize", st, o.channelBody.size, 9);
    getFiled("rulerName", st, o.ruler.name, "");
    getFiled("ruleSize", st, o.ruler.size, 9);
    getFiled("titleName", st, o.title.name, "");
    getFiled("titleSize", st, o.title.size, 9);
    getFiled("otherName", st, o.other.name, "");
    getFiled("otherSize", st, o.other.size, 9);

    st.endGroup();
}

static void _saveFont(FontOptions &o, QSettings &st)
{
    st.beginGroup("FontSetting");
    setFiled("toolbarName", st, o.toolbar.name);
    setFiled("toolbarSize", st, o.toolbar.size);
    setFiled("channelLabelName", st, o.channelLabel.name);
    setFiled("channelLabelSize", st, o.channelLabel.size);
    setFiled("channelBodyName", st, o.channelBody.name);
    setFiled("channelBodySize", st, o.channelBody.size);
    setFiled("rulerName", st, o.ruler.name);
    setFiled("ruleSize", st, o.ruler.size);
    setFiled("titleName", st, o.title.name);
    setFiled("titleSize", st, o.title.size);
    setFiled("otherName", st, o.other.name);
    setFiled("otherSize", st, o.other.size);

    st.endGroup();
}
*/

//------shortcuts
static void _loadShortcuts(ShortcutOptions &o, QSettings &st)
{
    st.beginGroup("Shortcuts");
    getFiled("startCollecting",   st, o.startCollecting,   "F1");
    getFiled("stopCollecting",    st, o.stopCollecting,    "F2");
    getFiled("switchVernierUp",   st, o.switchVernierUp,   "F3");
    getFiled("switchVernierDown", st, o.switchVernierDown, "F4");
    getFiled("parameterMeasure",  st, o.parameterMeasure,  "Ctrl+G");
    getFiled("vernierCreate",     st, o.vernierCreate,     "Ctrl+H");
    getFiled("switchPageUp",      st, o.switchPageUp,      "F5");
    getFiled("switchPageDown",    st, o.switchPageDown,    "F6");
    getFiled("jumpZero",          st, o.jumpZero,          "F7");
    getFiled("zoomIn",            st, o.zoomIn,            "F8");
    getFiled("zoomOut",           st, o.zoomOut,           "F9");
    getFiled("zoomFull",          st, o.zoomFull,          "F10");
    getFiled("saveFile",          st, o.saveFile,          "Ctrl+S");
    getFiled("saveAs",            st, o.saveAs,            "Ctrl+Shift+S");
    getFiled("exportFile",        st, o.exportFile,        "Ctrl+E");
    getFiled("deviceConfig",      st, o.deviceConfig,      "Ctrl+1");
    getFiled("protocolDecode",    st, o.protocolDecode,    "Ctrl+2");
    getFiled("labelMeasurement",  st, o.labelMeasurement,  "Ctrl+3");
    getFiled("dataSearch",        st, o.dataSearch,        "Ctrl+F");
    getFiled("closeSession",      st, o.closeSession,      "Ctrl+W");
    st.endGroup();
}

static void _saveShortcuts(ShortcutOptions &o, QSettings &st)
{
    st.beginGroup("Shortcuts");
    setFiled("startCollecting",   st, o.startCollecting);
    setFiled("stopCollecting",    st, o.stopCollecting);
    setFiled("switchVernierUp",   st, o.switchVernierUp);
    setFiled("switchVernierDown", st, o.switchVernierDown);
    setFiled("parameterMeasure",  st, o.parameterMeasure);
    setFiled("vernierCreate",     st, o.vernierCreate);
    setFiled("switchPageUp",      st, o.switchPageUp);
    setFiled("switchPageDown",    st, o.switchPageDown);
    setFiled("jumpZero",          st, o.jumpZero);
    setFiled("zoomIn",            st, o.zoomIn);
    setFiled("zoomOut",           st, o.zoomOut);
    setFiled("zoomFull",          st, o.zoomFull);
    setFiled("saveFile",          st, o.saveFile);
    setFiled("saveAs",            st, o.saveAs);
    setFiled("exportFile",        st, o.exportFile);
    setFiled("deviceConfig",      st, o.deviceConfig);
    setFiled("protocolDecode",    st, o.protocolDecode);
    setFiled("labelMeasurement",  st, o.labelMeasurement);
    setFiled("dataSearch",        st, o.dataSearch);
    setFiled("closeSession",      st, o.closeSession);
    st.endGroup();
}

//------------AppConfig

AppConfig::AppConfig()
{ 
}

AppConfig::AppConfig(AppConfig &o) 
{
    (void)o;
}

AppConfig::~AppConfig()
{
}

 AppConfig& AppConfig::Instance()
 {
     static AppConfig *ins = NULL;
     if (ins == NULL){
         ins = new AppConfig();
     }
     return *ins;
 }

void AppConfig::LoadAll()
{
    QSettings st(QApplication::organizationName(), QApplication::applicationName());
    _loadApp(appOptions, st);
    _loadHistory(userHistory, st);
    _loadFrame(frameOptions, st);
    _loadShortcuts(shortcutOptions, st);

    //dsv_dbg("Config file path:\"%s\"", st.fileName().toUtf8().data());
}

void AppConfig::SaveApp()
{
    QSettings st(QApplication::organizationName(), QApplication::applicationName());
    _saveApp(appOptions, st);
    _saveShortcuts(shortcutOptions, st);
}

void AppConfig::SaveHistory()
{
    QSettings st(QApplication::organizationName(), QApplication::applicationName());
    _saveHistory(userHistory, st);
}

void AppConfig::SaveFrame()
{
    QSettings st(QApplication::organizationName(), QApplication::applicationName());
    _saveFrame(frameOptions, st);
}

void AppConfig::SetProtocolFormat(const std::string &protocolName, const std::string &value)
{
    bool bChange = false;
    for (StringPair &o : appOptions.m_protocolFormats){
        if (o.m_key == protocolName){
            o.m_value = value;
            bChange = true;
            break;
        }    
    }

    if (!bChange)
    {
        if (appOptions.m_protocolFormats.size() > MAX_PROTOCOL_FORMAT_LIST)
        {
            while (appOptions.m_protocolFormats.size() < MAX_PROTOCOL_FORMAT_LIST)
            {
                appOptions.m_protocolFormats.erase(appOptions.m_protocolFormats.begin());
            }
        }
        appOptions.m_protocolFormats.push_back(StringPair(protocolName, value));
        bChange = true;
    }

    if (bChange){
        SaveApp();
    }
}

std::string AppConfig::GetProtocolFormat(const std::string &protocolName)
{
     for (StringPair &o : appOptions.m_protocolFormats){
        if (o.m_key == protocolName){ 
            return o.m_value;
        }
    }
    return "";
}

void AppConfig::GetFontSizeRange(float *minSize, float *maxSize)
{
    assert(minSize);
    assert(maxSize);
    // Unified range across all platforms: allow the user to pick 7–15 px.
    // Previously Windows was capped at 9, which made Chinese text too small
    // on 1080p displays (96 DPI, 100% scaling) where 9 physical pixels are
    // insufficient for legible CJK characters.
    *minSize = 7;
    *maxSize = 15;
}

float AppConfig::GetDefaultFontSize()
{
#ifdef Q_OS_MACOS
    return 12.0f;
#else
    // Target ~12 effective physical pixels so Chinese characters are legible
    // on any screen, regardless of its DPI / Windows scaling factor.
    //
    // With AA_EnableHighDpiScaling:
    //   setPixelSize(n) uses LOGICAL pixels; physical = n × devicePixelRatio
    //
    //  96 DPI / 100% → scale=1.00 → logical 12 → 12 physical  ✓
    // 120 DPI / 125% → scale=1.25 → logical 10 → 12.5 physical ✓
    // 144 DPI / 150% → scale=1.50 → logical  8 → 12 physical   ✓
    // 192 DPI / 200% → scale=2.00 → logical  6 → clamped to 7  (large enough)

    QScreen *screen = QGuiApplication::primaryScreen();
    float dpi = screen ? screen->logicalDotsPerInch() : 96.0f;
    float scale = dpi / 96.0f;
    if (scale < 0.5f) scale = 0.5f;

    float size = qRound(12.0f / scale);

    float minSize = 0, maxSize = 0;
    GetFontSizeRange(&minSize, &maxSize);
    if (size < minSize) size = minSize;
    if (size > maxSize) size = maxSize;

    return size;
#endif
}

bool AppConfig::IsDarkStyle()
{
    if (frameOptions.style == THEME_STYLE_DARK){
        return true;
    }
    return false;
}

QColor AppConfig::GetStyleColor()
{
    if (IsDarkStyle()){
        return QColor(38, 38, 38);
    }
    else{
        return QColor(248, 248, 248);
    }
}


//-------------api
QString GetIconPath()
{   
    QString style = AppConfig::Instance().frameOptions.style;
    if (style == ""){
        style = THEME_STYLE_DARK;
    }
    return ":/icons/" + style;
}

QString GetAppDataDir()
{
//applicationDirPath not end with '/'
#ifdef Q_OS_LINUX
    QDir dir(QCoreApplication::applicationDirPath());
    if (dir.cd("..") && dir.cd("share") && dir.cd("DSView"))
    {
         return dir.absolutePath();        
    }
    QDir dir1("/usr/local/share/DSView");
    if (dir1.exists()){
        return dir1.absolutePath();
    }

    dsv_err("Data directory is not exists: ../share/DSView");
    assert(false);   
#else

#ifdef Q_OS_DARWIN
    // QDir dir1(QCoreApplication::applicationDirPath());
    // //"./res" is not exists
    // if (dir1.cd("res") == false){
    //     QDir dir(QCoreApplication::applicationDirPath());
    //     // ../share/DSView
    //     if (dir.cd("..") && dir.cd("share") && dir.cd("DSView"))
    //     {
    //         return dir.absolutePath();
    //     }
    // }

    QDir dir1(QCoreApplication::applicationDirPath());
    // "../Resources/share/DSView"
    if (dir1.cd("..") && dir1.cd("Resources") && dir1.cd("share") && dir1.cd("DSView")){
        return dir1.absolutePath();
    }

#endif

    // The bin location
    return QCoreApplication::applicationDirPath();
#endif
}

QString GetFirmwareDir()
{
    QDir dir1 =  GetAppDataDir() + "/res";
    // ./res
    if (dir1.exists()){
        return dir1.absolutePath();
    }

    QDir dir(QCoreApplication::applicationDirPath());
    // ../share/DSView/res
    if (dir.cd("..") && dir.cd("share") && dir.cd("DSView") && dir.cd("res"))
    {
         return dir.absolutePath();
    }
 
#ifdef Q_OS_DARWIN
    // macOS bundle (../Resources/share/PXView/res)
    if (dir.cd("..") && dir.cd("Resources") && dir.cd("share") && dir.cd("DSView") && dir.cd("res"))
    {
         return dir.absolutePath();
    }
#endif

    dsv_err("%s%s", "Resource directory is not exists:", dir1.absolutePath().toUtf8().data());
    return dir1.absolutePath();
}

QString GetUserDataDir()
{
    #if QT_VERSION >= 0x050400
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    #else
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    #endif
}

QString GetDecodeScriptDir()
{
    QString path = GetAppDataDir() + "/decoders";

    QDir dir1;
    // ./decoders
    if (dir1.exists(path))
    {
         return path;     QColor GetStyleColor();
    }

    // QDir dir(QCoreApplication::applicationDirPath());
    // if (dir.cd("..") && dir.cd("share") && dir.cd("libsigrokdecode4DSL") && dir.cd("decoders"))
    // {
    //      return dir.absolutePath();        
    // }

#ifdef Q_OS_DARWIN
    dir1.cd(QCoreApplication::applicationDirPath());
    //if (dir1.cd("..") && dir1.cd("Resources") && dir1.cd("share") && dir1.cd("DSView") &&
    if (dir1.cd("..") && dir1.cd("Resources") && dir1.cd("share") &&
        dir1.cd("libsigrokdecode4DSL") && dir1.cd("decoders"))
    {
         return dir1.absolutePath();
    }

#elif defined(Q_OS_UNIX)
    QDir dir(QCoreApplication::applicationDirPath());
    // ../share/PXView/libsigrokdecode4DSL/decoders
    //if (dir.cd("..") && dir.cd("share")&& dir.cd("PXView")  && dir.cd("libsigrokdecode4DSL") && dir.cd("decoders"))
    if (dir.cd("..") && dir.cd("share") && dir.cd("libsigrokdecode4DSL") && dir.cd("decoders"))
    {
        return dir.absolutePath();        
    }
#endif
    return "";
}

QString GetCDecodeDir()
{
    // Use the user-writable data directory so users can install C decoders
    // without modifying the app bundle. The exact directory comes from Qt's
    // QStandardPaths::AppDataLocation, which expands to
    //   <OS user data root>/<QCoreApplication::organizationName()>/<applicationName()>/cdecoders
    // For DSView/PXTOOL that resolves to (at runtime, may differ per build):
    //   macOS:   ~/Library/Application Support/DreamSourceLab/PXTOOL/cdecoders
    //   Linux:   ~/.local/share/DreamSourceLab/PXTOOL/cdecoders
    //   Windows: %APPDATA%/DreamSourceLab/PXTOOL/cdecoders
    // The startup log always prints the resolved path (see appcontrol.cpp).
    return GetUserDataDir() + "/cdecoders";
}

QString GetBundledCDecodeDir()
{
    // C decoders shipped with the app live next to res/, demo/, lang/ under the
    // resource share/DSView directory. CMake installs them via:
    //   install(TARGETS spi LIBRARY DESTINATION ${MAC_RES_PREFIX}share/DSView/cdecoders)
    // So on macOS the dylib ends up at:
    //   <App>.app/Contents/Resources/share/DSView/cdecoders/spi.dylib
    // and on Linux at:
    //   <prefix>/share/DSView/cdecoders/spi.so
    // Both paths are reachable as GetAppDataDir() + "/cdecoders".
    QString path = GetAppDataDir() + "/cdecoders";
    if (QDir(path).exists())
        return path;
    return QString();
}

QString GetProfileDir()
{
 #if QT_VERSION >= 0x050400
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    #else
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    #endif
}
