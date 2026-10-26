#!/usr/bin/env python3
"""Replace the placeholder mod ids with your own, everywhere they must agree.

    python3 tools/set_mod_id.py YOURID            # e.g. "alice"
    -> alice.predictive-buffer-api and alice.custom-buffer-example

A Geode mod id is "<developer>.<name>" in lowercase. The id appears in mod.json files,
in Api.hpp (the manually defined MY_MOD_ID that GEODE_EVENT_EXPORT needs), in the example's
#include path, and in the docs. Also edit the "developer"/"YOUR NAME" fields by hand.
"""
import pathlib, re, sys

if len(sys.argv) != 2 or not re.fullmatch(r"[a-z0-9_-]+", sys.argv[1]):
    sys.exit("usage: set_mod_id.py <developer-id>   (lowercase letters, digits, - and _)")
new = sys.argv[1]
root = pathlib.Path(__file__).resolve().parent.parent
n = 0
for p in root.rglob("*"):
    if p.is_file() and p.suffix in {".json", ".hpp", ".cpp", ".md", ".txt"} and "geode_stub" not in p.parts:
        t = p.read_text(encoding="utf-8")
        u = t.replace("your-id.", new + ".")
        if u != t:
            p.write_text(u, encoding="utf-8"); n += 1
print(f"updated {n} files: ids are now {new}.predictive-buffer-api / {new}.custom-buffer-example")
