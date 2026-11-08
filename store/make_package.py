#!/usr/bin/env python3
"""Store package: store/release/ and store/FindMyAnything_Store_Paket.zip.

    python3 store/make_package.py

Takes the watchapp from watch/build/watch.pbw (build it first: cd watch && rm -rf
build && pebble build; the store wants version Major.Minor and the clean build,
waf misses changes in package.json otherwise), the icons and the banner from
store/icon/ (make_icons.py, banner/make_banner.py), the screenshots from
store/screenshots_emery/ and the texts from store/.
"""
import glob
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
REL = os.path.join(HERE, "release")
LIMIT = 1600

version = json.load(open(os.path.join(ROOT, "watch", "package.json")))["version"]
if version.count(".") != 1:
    sys.exit(f"version {version!r} must be Major.Minor for the store")

pbw_src = os.path.join(ROOT, "watch", "build", "watch.pbw")
data = open(pbw_src, "rb").read()
info = json.loads(zipfile.ZipFile(io.BytesIO(data)).read("appinfo.json"))
if info["versionLabel"] != version:
    sys.exit(f"PBW is {info['versionLabel']}, package.json {version}: rebuild cleanly")
if "configurable" not in info.get("capabilities", []):
    sys.exit("PBW lacks capabilities: configurable (no settings button in the Pebble app)")
demo = subprocess.run(["grep", "-c", "define FMA_DEMO 1", os.path.join(ROOT, "watch", "src", "c", "main.c")],
                      capture_output=True, text=True).stdout.strip()
if demo != "0":
    sys.exit("main.c is in demo mode")

text = open(os.path.join(HERE, "description_en.txt"), encoding="utf-8").read()
short, body = text.split("\n", 1)
if len(body.strip()) > LIMIT:
    sys.exit(f"description {len(body.strip())} > {LIMIT} characters")

shutil.rmtree(REL, ignore_errors=True)
os.makedirs(REL)
pbw = f"FindMyAnything-{version}.pbw"
shutil.copy(pbw_src, os.path.join(REL, pbw))
for f in ("icon_80.png", "icon_144.png", "icon_48.png", "banner_720x320.png"):
    shutil.copy(os.path.join(HERE, "icon", f), REL)
shutil.copytree(os.path.join(HERE, "screenshots_emery"), os.path.join(REL, "screenshots_emery"))
for f in ("description_en.txt", "RELEASE_NOTES.md"):
    shutil.copy(os.path.join(HERE, f), REL)

zpath = os.path.join(HERE, "FindMyAnything_Store_Paket.zip")
with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
    for path in sorted(glob.glob(os.path.join(REL, "**", "*"), recursive=True)):
        if os.path.isfile(path):
            z.write(path, os.path.relpath(path, HERE))
    z.write(os.path.join(HERE, "VEROEFFENTLICHEN.md"), "VEROEFFENTLICHEN.md")

print("version", version, "| description", len(body.strip()), "of", LIMIT, "| short", len(short))
print(pbw, "sha256", hashlib.sha256(data).hexdigest())
with zipfile.ZipFile(zpath) as z:
    for n in z.namelist():
        print("  ", n)
print(zpath)
