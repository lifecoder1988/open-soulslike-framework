"""Extracts every LOCTEXT / NSLOCTEXT of the OSF plugin into a translation table template.

usage: python Tools/Localization/extract_osf_text.py [existing.json] > osf_text.json

Output: {"<namespace>": {"<key>": {"source": "<English>", "text": "<translation or empty>"}}}
Passing an existing table keeps its translations (new strings get an empty "text").
Games load a filled table with UOSFLocalization::LoadTranslationTable (see Docs/Localization.md).
"""
import json
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Plugins", "OpenSoulslikeFramework", "Source")
STRING = r'"((?:[^"\\]|\\.)*)"'
NS = re.compile(r'#define\s+LOCTEXT_NAMESPACE\s+"([^"]+)"')
LT = re.compile(r'(?<!NS)LOCTEXT\(\s*"([^"]+)"\s*,\s*' + STRING + r'\s*\)')
NSLT = re.compile(r'NSLOCTEXT\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*' + STRING + r'\s*\)')


def main():
    existing = json.load(open(sys.argv[1], encoding="utf-8")) if len(sys.argv) > 1 else {}
    table = {}
    for folder, _, files in os.walk(ROOT):
        for name in sorted(files):
            if not name.endswith((".cpp", ".h")):
                continue
            namespace = None
            for line in open(os.path.join(folder, name), encoding="utf-8"):
                m = NS.search(line)
                if m:
                    namespace = m.group(1)
                for key, source in LT.findall(line):
                    if namespace:
                        table.setdefault(namespace, {})[key] = source
                for ns, key, source in NSLT.findall(line):
                    table.setdefault(ns, {})[key] = source
    out = {}
    for ns in sorted(table):
        for key in sorted(table[ns]):
            prev = existing.get(ns, {}).get(key, {})
            text = prev.get("text", "") if isinstance(prev, dict) else prev
            out.setdefault(ns, {})[key] = {"source": table[ns][key], "text": text}
    sys.stdout.reconfigure(encoding="utf-8")
    print(json.dumps(out, ensure_ascii=False, indent=1))


if __name__ == "__main__":
    main()
