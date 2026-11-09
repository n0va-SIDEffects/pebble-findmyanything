#!/usr/bin/env python3
"""Store screenshots for every platform, in English, from the emulator.

    python3 store/make_screenshots.py [platform ...]     # default: all of package.json

Builds a copy of the watchapp with FMA_DEMO 2 (fake devices; the app walks by
itself through list -> AirTag (near) -> Galaxy SmartTag (medium) -> Tile (far) ->
filter, 25 s each) and photographs each step on a schedule. Button sequences
are not reliable on the slow Hermes VM: clicks get lost and one "back" too many
ends in the watchface. One emulator at a time (3 GB RAM), stopped afterwards;
an emulator that is already running (someone else's) is left alone.

Writes store/screenshots_<platform>/1_very_close.png ... 5_far.png and, from the
release build (watch/build/watch.pbw, build it first), $TMPDIR/fma_shots/
<platform>_6_empty.png (no phone) for checking the empty state.
"""
import json
import os
import shutil
import subprocess
import sys
import time

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
T = os.environ.get("TMPDIR", "/tmp")
WORK = os.path.join(T, "fma_demo")
RAW = os.path.join(T, "fma_shots")
REL = os.path.join(ROOT, "watch", "build", "watch.pbw")
ENV = dict(os.environ, PATH=os.path.expanduser("~/.local/bin") + ":" + os.environ["PATH"],
           NO_PROXY="localhost,127.0.0.1", no_proxy="localhost,127.0.0.1")
STEP = 25.0  # DEMO_STEP_MS in main.c
# tour order -> file name in the store set
NAMES = [("2_list", "2_device_list"), ("1_near", "1_very_close"), ("3_mid", "3_getting_closer"),
         ("5_far", "5_far"), ("4_filter", "4_trackers")]


def run(args, timeout):
    try:
        return subprocess.run(["pebble"] + args, env=ENV, timeout=timeout, capture_output=True, text=True).returncode
    except subprocess.TimeoutExpired:
        return -1


def build_demo():
    shutil.rmtree(WORK, ignore_errors=True)
    shutil.copytree(os.path.join(ROOT, "watch"), os.path.join(WORK, "watch"),
                    ignore=shutil.ignore_patterns("build", ".lock-waf*"))
    main = os.path.join(WORK, "watch", "src", "c", "main.c")
    src = open(main, encoding="utf-8").read()
    assert "#define FMA_DEMO 0" in src
    open(main, "w", encoding="utf-8").write(src.replace("#define FMA_DEMO 0", "#define FMA_DEMO 2"))
    if run(["build"], 900) != 0:
        sys.exit("demo build failed")
    return os.path.join(WORK, "watch", "build", "watch.pbw")


def shot(p, name):
    """Through the QEMU monitor (qemu_shot.py); `pebble screenshot` asks the firmware
    and crashed basalt while the search screen was redrawing."""
    import importlib.util
    spec = importlib.util.spec_from_file_location("qemu_shot", os.path.join(HERE, "qemu_shot.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    out = os.path.join(RAW, f"{p}_{name}.png")
    try:
        return mod.screendump(p, out)
    except Exception as e:  # emulator still starting
        print(p, name, "screendump failed:", e, flush=True)
        return None


def shot_firmware(p, name):
    path = os.path.join(RAW, f"{p}_{name}.png")
    for _ in range(3):
        if run(["screenshot", "--emulator", p, "--no-open", path], 60) == 0 and os.path.exists(path):
            return path
        time.sleep(1)
    return None


def list_ok(path):
    """List header on screen: dark blue in colour, black with a white line in b/w.
    (The launcher's highlight is a light blue.)"""
    if not path:
        return False
    im = Image.open(path).convert("RGB")
    w, h = im.size
    if w == 144 and im.getpixel((72, 40)) in ((0, 0, 0), (255, 255, 255)) and len(set(im.getdata())) <= 2:
        return im.getpixel((72, 23)) == (255, 255, 255)
    r, g, b = im.getpixel((w // 2, 6 if w not in (180, 260) else h // 2 - 60))
    return b > r + 20 and b > g + 20 and b < 130 and g < 80


def emulator_pids(p):
    out = subprocess.run(["pgrep", "-f", f"qemu-pebble.*/{p}/qemu"], capture_output=True, text=True).stdout
    return [int(x) for x in out.split()]


def platform_shots(p, demo):
    for attempt in range(3):
        if run(["install", "--emulator", p, demo], 300) != 0:
            run(["install", "--emulator", p, demo], 300)
        # Sometimes the app stays closed: Select opens the launcher, where the app
        # just installed is highlighted, Select again opens it
        t0 = None
        for k in range(8):
            time.sleep(3)
            before = time.time()
            if list_ok(shot(p, "probe")):
                t0 = before - 3
                break
            if k in (3, 5):
                run(["emu-button", "--emulator", p, "click", "select"], 30)
        if t0 is None:
            print(p, "app does not open, again", flush=True)
            continue
        paths = {}
        for k, (name, _) in enumerate(NAMES):
            wait = t0 + k * STEP + 10 - time.time()
            if wait > 0:
                time.sleep(wait)
            paths[name] = shot(p, name)
            print(p, name, "ok" if paths[name] else "FAILED", round(time.time() - t0, 1), flush=True)
        if list_ok(paths["2_list"]) and all(paths.values()):
            out = os.path.join(HERE, "screenshots_" + p)
            os.makedirs(out, exist_ok=True)
            for name, final in NAMES:
                shutil.copy(paths[name], os.path.join(out, final + ".png"))
            return True
        print(p, "not all steps on screen, again", flush=True)
    return False


def main():
    platforms = sys.argv[1:] or json.load(open(os.path.join(ROOT, "watch", "package.json")))["pebble"]["targetPlatforms"]
    os.makedirs(RAW, exist_ok=True)
    demo = build_demo()
    for p in platforms:
        if emulator_pids(p):
            print(p, "emulator already running (someone else's?), skipped", flush=True)
            continue
        print("==", p, time.strftime("%T"), flush=True)
        ok = platform_shots(p, demo)
        if os.path.exists(REL):
            run(["install", "--emulator", p, REL], 300)
            time.sleep(14)
            print(p, "6_empty", "ok" if shot(p, "6_empty") else "FAILED", flush=True)
        run(["kill", "--emulator", p], 60)
        for pid in emulator_pids(p):
            os.kill(pid, 15)
        time.sleep(2)
        print(p, "done" if ok else "FAILED", flush=True)
    print("Check the pictures before they go to the store (montage).")


if __name__ == "__main__":
    main()
