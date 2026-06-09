#!/usr/bin/env python3
"""
Replace L_S() macro calls with Qt tr() in PXTOOL source files.
Only targets MSG, TOOLBAR, DLG pages (static UI strings).
DSL/DECODER dynamic lookups are left unchanged.

Usage: python3 scripts/misc/replace_ls_macro.py [--dry-run]
"""
import re, os, sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PXTOOL_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
SOURCE_DIR  = os.path.join(PXTOOL_ROOT, "PXTOOL", "pv")

DRY_RUN = "--dry-run" in sys.argv

L_S_RE = re.compile(
    r'L_S\s*\(\s*STR_PAGE_(?:MSG|TOOLBAR|DLG)\s*,'
    r'\s*S_ID\s*\(\s*[\w-]+\s*\)\s*,\s*"([^"]*)"\s*\)'
)

def process(fpath):
    with open(fpath, encoding="utf-8") as f:
        content = f.read()
    new_content = L_S_RE.sub(r'tr("\1")', content)
    if new_content == content:
        return False
    if not DRY_RUN:
        with open(fpath, "w", encoding="utf-8") as f:
            f.write(new_content)
    return True

def main():
    modified = []
    for root, _, files in os.walk(SOURCE_DIR):
        for fname in files:
            if fname.endswith((".cpp", ".h")):
                fpath = os.path.join(root, fname)
                if process(fpath):
                    rel = os.path.relpath(fpath, PXTOOL_ROOT)
                    modified.append(rel)
                    print(f"{'[DRY]' if DRY_RUN else 'Modified'}: {rel}")
    print(f"\nTotal files {'would be ' if DRY_RUN else ''}modified: {len(modified)}")

if __name__ == "__main__":
    main()
