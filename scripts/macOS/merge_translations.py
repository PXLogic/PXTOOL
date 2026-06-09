#!/usr/bin/env python3
"""
Merge Chinese translations from translation_map.json into lupdate-generated zh_CN.ts.
Matches on <source> text.
- If translation is empty and source is in mapping: fill with mapping value.
- If translation already has Chinese text (from heuristic) and source is in mapping: remove unfinished marker.
- If translation already has Chinese text but source NOT in mapping: also remove unfinished marker (trust lupdate heuristic).
"""
import json, re, os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DSVIEW_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
MAP_FILE = os.path.join(SCRIPT_DIR, "translation_map.json")
TS_FILE  = os.path.join(DSVIEW_ROOT, "PXTOOL", "languages", "zh_CN.ts")

with open(MAP_FILE, encoding="utf-8") as f:
    mapping = json.load(f)
with open(TS_FILE, encoding="utf-8") as f:
    ts = f.read()

filled = 0
promoted = 0

# Pass 1: fill empty unfinished translations from mapping
for english, chinese in mapping.items():
    eng_esc = english.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    pattern = (
        r'<source>' + re.escape(eng_esc) + r'</source>(\s*'
        r'(?:<comment>[^<]*</comment>\s*)?)'
        r'<translation type="unfinished"></translation>'
    )
    replacement = (
        r'<source>' + eng_esc + r'</source>\1'
        r'<translation>' + chinese.replace('\\', '\\\\') + r'</translation>'
    )
    new_ts, n = re.subn(pattern, replacement, ts)
    if n > 0:
        ts = new_ts
        filled += n

# Pass 2: promote unfinished translations that already have content
# (these came from lupdate's same-text heuristic — trust them)
pattern_promote = r'<translation type="unfinished">([^<]+)</translation>'
new_ts, n = re.subn(pattern_promote, r'<translation>\1</translation>', ts)
if n > 0:
    ts = new_ts
    promoted = n

with open(TS_FILE, "w", encoding="utf-8") as f:
    f.write(ts)
print(f"Filled {filled} empty unfinished translations from mapping")
print(f"Promoted {promoted} heuristic-matched translations to finished")
remaining = ts.count('type="unfinished"')
print(f"Remaining unfinished: {remaining}")
