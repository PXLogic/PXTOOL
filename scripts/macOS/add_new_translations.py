"""
Fills in new Chinese translations added for i18n retranslation fixes.
Targets zh_CN.ts — replaces empty or wrong unfinished translations for the
new labels: Trigger, Decode, Measure, Search, Options, Log (sidebar tabs),
Device, Sample Rate, Buffer, Mode (sampling bar labels).
"""
import re
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DSVIEW_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
TS_FILE = os.path.join(DSVIEW_ROOT, "DSView", "languages", "zh_CN.ts")

# Map of English source -> desired Chinese translation
new_trans = {
    "Log":         "日志",
    "Sample Rate": "采样率",
    "Buffer":      "缓存",
}

# For these, only fill if the translation is currently EMPTY (others have
# heuristic-filled values that may or may not be appropriate)
also_update_nonempty = {
    "Trigger":  "触发",
    "Decode":   "解码",
    "Measure":  "测量",
    "Search":   "搜索",
    "Options":  "选项",
    "Device":   "设备",
    "Mode":     "模式",
}

with open(TS_FILE, encoding="utf-8") as f:
    ts = f.read()

filled = 0

# Fill empty unfinished translations
for eng, cn in new_trans.items():
    pattern = (r'(<source>' + re.escape(eng) + r'</source>\s*'
               r'(?:<comment>[^<]*</comment>\s*)?'
               r'<translation type="unfinished">)(</translation>)')
    new_ts, n = re.subn(pattern, r'\g<1>' + cn + r'</translation>', ts)
    if n:
        ts = new_ts
        filled += n
        print(f"  Filled empty: {eng} -> {cn} ({n} occurrence(s))")

# For entries that may have heuristic-filled but wrong translations,
# replace the full translation content (empty or not)
for eng, cn in also_update_nonempty.items():
    pattern = (r'(<source>' + re.escape(eng) + r'</source>\s*'
               r'(?:<comment>[^<]*</comment>\s*)?'
               r'<translation type="unfinished">)[^<]*(</translation>)')
    new_ts, n = re.subn(pattern, r'\g<1>' + cn + r'</translation>', ts)
    if n:
        ts = new_ts
        filled += n
        print(f"  Updated: {eng} -> {cn} ({n} occurrence(s))")

with open(TS_FILE, "w", encoding="utf-8") as f:
    f.write(ts)

print(f"\nTotal: filled/updated {filled} translations")
