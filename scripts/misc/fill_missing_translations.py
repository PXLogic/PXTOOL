#!/usr/bin/env python3
"""Fill in missing Chinese translations in zh_CN.ts."""
import re, os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PXTOOL_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
TS_FILE = os.path.join(PXTOOL_ROOT, "PXTOOL", "languages", "zh_CN.ts")

# Map: English source → Chinese translation
# Format: "english": "中文"
TRANSLATIONS = {
    # MainWindow menus
    "Bug Report": "问题反馈",
    "Capture...": "采集...",
    "Check for Updates": "检查更新",
    "Config...": "配置...",
    "Default...": "默认...",
    "Device": "设备",
    "Display Options...": "显示选项...",
    "Export...": "导出...",
    "Keyboard Shortcuts...": "键盘快捷键...",
    "Language": "语言",
    "Load...": "加载...",
    "Manual": "使用手册",
    "Open file in new comparison tab": "在新对比标签页中打开文件",
    "Open...": "打开...",
    "Save...": "保存...",
    "Session %1": "会话 %1",
    "Session 1": "会话 1",
    "Store...": "存储...",
    "Window": "窗口",
    "Default": "默认",
    # MainWindow messages
    "Current loading file has an old format. \nThis will lead to a slow loading speed. \nPlease resave it after loaded.":
        "当前加载文件格式较旧。\n这将导致加载速度缓慢。\n加载完成后请重新保存。",
    "Memory is not enough for this sample!\nPlease reduce the sample depth!":
        "内存不足以支持当前采样！\n请减小采样深度！",
    "Plug the device into a USB 2.0 port will seriously affect its performance.\nPlease replug it into a USB 3.0 port.":
        "设备插入USB 2.0端口会严重影响性能。\n请将其重新插入USB 3.0端口。",
    "USB bandwidth can not support current sample rate! \nPlease reduce the sample rate!":
        "USB带宽不支持当前采样率！\n请降低采样率！",
    # StoreSession
    "DSView does not currently support\nfile export for multiple data types.":
        "DSView 暂不支持\n多数据类型的文件导出。",
    "DSView does not currently support\nfile saving for multiple data types.":
        "DSView 暂不支持\n多数据类型的文件保存。",
    "Failed to create zip file. Malloc error.": "创建zip文件失败，内存分配错误。",
    "Failed to create zip file. Please check write permission of this path.":
        "创建zip文件失败，请检查该路径的写入权限。",
    "Invalid export format.": "无效的导出格式。",
    "No set file name.": "未设置文件名。",
    "data type don't support.": "不支持该数据类型。",
    "xbuffer malloc failed.": "xbuffer内存分配失败。",
    # About dialog
    "<font size=16>Changelogs</font><br />": "<font size=16>更新日志</font><br />",
    "© DreamSourceLab. All rights reserved.": "© DreamSourceLab. 版权所有。",
    # DecoderOptionsDlg
    "Decode Range": "解码范围",
    # DeviceOptions
    "Auto Calibration program will be started. Don't connect any probes. \nIt can take a while!":
        "自动校准程序即将启动，请勿连接任何探针。\n这可能需要一些时间！",
    # DsoMeasure labels
    "+Count": "+计数",
    "+Duty": "+占空比",
    "+Over": "+过冲",
    "+Width": "+宽度",
    "-Duty": "-占空比",
    "-Over": "-过冲",
    "-Width": "-宽度",
    "Ampl": "幅值",
    "BrstW": "突发宽",
    "Fall": "下降",
    "Freq": "频率",
    "High": "高",
    "Low": "低",
    "Max": "最大",
    "Mean": "均值",
    "Min": "最小",
    "NULL": "空",
    "PK-PK": "峰峰值",
    "Period": "周期",
    "RMS": "有效值",
    "Rise": "上升",
    # FftOptions
    "Y-axis Mode: ": "Y轴模式：",
    # Search pattern help
    "X: Don't care\n0: Low level\n1: High level\nR: Rising edge\nF: Falling edge\nC: Rising/Falling edge":
        "X: 不关心\n0: 低电平\n1: 高电平\nR: 上升沿\nF: 下降沿\nC: 上升/下降沿",
    # ShortcutDlg
    "'%1' is already assigned to another action.": "“%1”已分配给其他操作。",
    "Action": "操作",
    "Add Cursor": "添加光标",
    "Close Session": "关闭会话",
    "Data Search": "数据搜索",
    "Device Config": "设备配置",
    "Double-click a shortcut cell to edit.  Leave empty to disable.":
        "双击快捷键单元格进行编辑，留空则禁用。",
    "Duplicate Shortcut": "快捷键重复",
    "Jump to Zero": "跳转到零点",
    "Key Sequence": "按键序列",
    "Keyboard Shortcuts": "键盘快捷键",
    "Label Measurement": "标签测量",
    "Measure Panel": "测量面板",
    "Next Cursor": "下一光标",
    "Next Tab": "下一标签页",
    "OK": "确定",
    "Prev Cursor": "上一光标",
    "Previous Tab": "上一标签页",
    "Protocol Decode": "协议解码",
    "Reset Defaults": "恢复默认",
    "Start Collecting": "开始采集",
    "Stop Collecting": "停止采集",
    "Zoom Fit": "适应窗口",
    "Zoom In": "放大",
    "Zoom Out": "缩小",
    # DeviceOptionsDock
    "Apply": "应用",
    # LogDock
    "Debug": "调试",
    "Error": "错误",
    "Info": "信息",
    "Level:": "级别：",
    "Verbose": "详细",
    "Warning": "警告",
    # SearchDock
    "Click the search pattern\nto configure search options":
        "点击搜索模式\n配置搜索选项",
    # SideBar
    "Decode Protocol": "协议解码",
    "Measurement": "测量",
    "Trigger Setting": "触发设置",
    # TriggerDock
    "Trigger setted on multiple channels!\nCapture will Only triggered when all setted channels fullfill at one sample":
        "触发设置在多个通道！\n仅当所有设置通道在同一采样点同时满足条件时才会触发采集",
    # SamplingBar
    "Show/Hide trigger settings": "显示/隐藏触发设置",
    # Header
    "1X  (30px)": "1X  (30像素)",
    "2X  (60px)": "2X  (60像素)",
    "4X (120px)": "4X (120像素)",
    "8X (240px)": "8X (240像素)",
    "Auto (fit to view)": "自动（适应窗口）",
    "C Decoder": "C语言解码器",
    "Decode Engine": "解码引擎",
    "Python Decoder": "Python解码器",
    "Reset All Row Heights": "重置所有行高",
    "Reset Row Height": "重置行高",
    "Set Channel Height": "设置通道高度",
}

def xml_escape(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("'", "&apos;")

def main():
    with open(TS_FILE, encoding="utf-8") as f:
        ts = f.read()

    filled = 0
    not_found = []

    for english, chinese in TRANSLATIONS.items():
        eng_esc = xml_escape(english)
        # Match unfinished translation block for this source (empty translation only)
        pattern = (
            r'(<source>' + re.escape(eng_esc) + r'</source>\s*'
            r'(?:<comment>[^<]*</comment>\s*)?'
            r')<translation type="unfinished"></translation>'
        )
        cn_esc = xml_escape(chinese)
        repl = r'\g<1>' + '<translation>' + cn_esc.replace('\\', r'\\') + '</translation>'
        new_ts, n = re.subn(pattern, repl, ts)
        if n > 0:
            ts = new_ts
            filled += n
        else:
            not_found.append(english)

    # Also strip type="unfinished" from any already-filled translations that still carry it
    ts, n2 = re.subn(
        r'<translation type="unfinished">(.+?)</translation>',
        r'<translation>\1</translation>',
        ts,
        flags=re.DOTALL,
    )

    with open(TS_FILE, "w", encoding="utf-8") as f:
        f.write(ts)

    print(f"Filled: {filled}")
    if not_found:
        print(f"Not found in ts ({len(not_found)}):")
        for s in not_found:
            print(f"  {repr(s)}")

if __name__ == "__main__":
    main()
