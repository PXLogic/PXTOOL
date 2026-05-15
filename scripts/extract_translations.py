#!/usr/bin/env python3
"""
Extract L_S() calls from DSView source and map English defaults to
Chinese translations from JSON files. Outputs translation_map.json.

Usage: python3 scripts/extract_translations.py
Output: scripts/translation_map.json
"""
import re, json, os, sys

DSVIEW_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LANG_CN_DIR = os.path.join(DSVIEW_ROOT, "lang", "cn")
SOURCE_DIR  = os.path.join(DSVIEW_ROOT, "DSView", "pv")

PAGE_TO_JSON = {
    "STR_PAGE_MSG":     ["msg.json"],
    "STR_PAGE_TOOLBAR": ["toolbar.json"],
    "STR_PAGE_DLG":     ["dlg.json"],
}

# Match: L_S(PAGE_ID, S_ID(STRING_ID), "default_value")
L_S_RE = re.compile(
    r'L_S\s*\(\s*(STR_PAGE_(?:MSG|TOOLBAR|DLG))\s*,'
    r'\s*S_ID\s*\(\s*(\w+)\s*\)\s*,\s*"([^"]*)"\s*\)'
)

def load_cn(page_id):
    trans = {}
    for fname in PAGE_TO_JSON.get(page_id, []):
        fpath = os.path.join(LANG_CN_DIR, fname)
        if os.path.exists(fpath):
            with open(fpath, encoding="utf-8") as f:
                for item in json.load(f):
                    trans[item["id"]] = item["text"]
    return trans

def main():
    mapping = {}
    total = 0

    cn = {}
    for page in PAGE_TO_JSON:
        cn[page] = load_cn(page)

    for root, _, files in os.walk(SOURCE_DIR):
        for fname in files:
            if not fname.endswith((".cpp", ".h")):
                continue
            fpath = os.path.join(root, fname)
            with open(fpath, encoding="utf-8", errors="ignore") as f:
                content = f.read()
            for m in L_S_RE.finditer(content):
                page_id, string_id, english = m.groups()
                chinese = cn.get(page_id, {}).get(string_id, "")
                if english and chinese:
                    mapping[english] = chinese
                total += 1

    out = os.path.join(os.path.dirname(__file__), "translation_map.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(mapping, f, ensure_ascii=False, indent=2)
    print(f"Scanned {total} L_S() calls (MSG/TOOLBAR/DLG only)")
    print(f"Built {len(mapping)} unique English→Chinese mappings")
    print(f"Saved to {out}")

if __name__ == "__main__":
    main()
