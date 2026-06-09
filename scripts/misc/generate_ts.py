#!/usr/bin/env python3
"""
Generate a zh_CN.ts stub from translation_map.json.
Groups all entries under a single "PXTOOL" context.
"""
import json, os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PXTOOL_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
MAP_FILE = os.path.join(SCRIPT_DIR, "translation_map.json")
OUT_FILE = os.path.join(PXTOOL_ROOT, "PXTOOL", "languages", "zh_CN.ts")

mapping = json.load(open(MAP_FILE, encoding="utf-8"))

messages = []
for english, chinese in mapping.items():
    english_esc = english.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")
    chinese_esc = chinese.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")
    messages.append(f"""    <message>
        <source>{english_esc}</source>
        <translation>{chinese_esc}</translation>
    </message>""")

ts_content = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN" sourcelanguage="en">
<context>
    <name>PXTOOL</name>
""" + "\n".join(messages) + """
</context>
</TS>
"""

with open(OUT_FILE, "w", encoding="utf-8") as f:
    f.write(ts_content)
print(f"Wrote {len(messages)} translations to {OUT_FILE}")
