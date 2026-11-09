#!/usr/bin/env python3
"""Screenshot through the QEMU monitor (screendump): reads the emulated display
directly, without asking the watch firmware. `pebble screenshot` asks the
firmware, and on basalt in the slow emulator that crashed the watch while the
search screen was redrawing.

    qemu_shot.py <platform> <out.png>
"""
import json
import os
import socket
import sys
import time

from PIL import Image

SIZES = {"aplite": (144, 168), "basalt": (144, 168), "chalk": (180, 180), "diorite": (144, 168),
         "emery": (200, 228), "flint": (144, 168), "gabbro": (260, 260)}


def monitor_port(platform):
    for base in (os.environ.get("TMPDIR", "/tmp"), "/tmp"):
        path = os.path.join(base, "pb-emulator.json")
        if os.path.exists(path):
            info = json.load(open(path))
            for version in info.get(platform, {}).values():
                if isinstance(version, dict) and "qemu" in version:
                    return version["qemu"]["monitor"]
    raise SystemExit("no emulator for " + platform)


def screendump(platform, out):
    ppm = out + ".ppm"
    if os.path.exists(ppm):
        os.remove(ppm)
    s = socket.create_connection(("127.0.0.1", monitor_port(platform)), timeout=10)
    time.sleep(0.3)
    s.recv(4096)
    s.sendall(("screendump %s\n" % ppm).encode())
    for _ in range(50):
        time.sleep(0.2)
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
    time.sleep(0.3)
    s.close()
    im = Image.open(ppm).convert("RGB")
    w, h = SIZES[platform]
    dx, dy = (im.width - w) // 2, (im.height - h) // 2   # the display has a frame around it
    im = im.crop((dx, dy, dx + w, dy + h))
    os.remove(ppm)
    if platform in BW:  # one level only: anything lit is white
        im = im.convert("L").point(lambda v: 255 if v > 0 else 0).convert("RGB")
    else:
        im = like_pebble_screenshot(im)
    im.save(out)
    return out


LEVELS = (0, 85, 170, 255)
BW = ("aplite", "diorite", "flint")


def correction_table():
    """pebble-tool's colour correction (screenshot.py, _correct_colours), so these
    pictures look like the ones `pebble screenshot` makes."""
    import ast
    import glob
    for path in glob.glob(os.path.expanduser(
            "~/.local/share/uv/tools/pebble-tool/lib/python3*/site-packages/pebble_tool/commands/screenshot.py")):
        src = open(path).read()
        i = src.index("mapping = {", src.index("def _correct_colours"))
        return ast.literal_eval(src[i + len("mapping = "):src.index("}", i) + 1])
    return {}


def brightest_level(im):
    """The QEMU display is dimmed by a factor (emery: levels 0/33/66/100, basalt:
    0/56/113/170): the value that stands for 255 is the one that puts every
    channel value on the four palette levels."""
    values = sorted({c for _, px in im.getcolors(1 << 16) for c in px} - {0})
    if not values:
        return 255
    best, best_err = 255, None
    for v in values:
        for top in (v, v * 1.5, v * 3):
            if top > 255.5:
                continue
            err = sum(min(abs(c * 255 / top - lv) for lv in LEVELS) for c in values)
            if best_err is None or err < best_err - 1e-6 or (abs(err - best_err) < 1e-6 and top > best):
                best, best_err = top, err
    return best


def like_pebble_screenshot(im):
    """The QEMU display dims with the backlight off (255 -> 170): back to the
    64-colour palette, then the same correction as `pebble screenshot`."""
    top = brightest_level(im)
    table = correction_table()
    lut = {}
    out = Image.new("RGB", im.size)
    src, dst = im.load(), out.load()
    for y in range(im.height):
        for x in range(im.width):
            px = src[x, y]
            if px not in lut:
                raw = tuple(min(LEVELS, key=lambda lv: abs(lv - c * 255 / top)) for c in px)
                lut[px] = table.get(raw, raw)
            dst[x, y] = lut[px]
    return out


if __name__ == "__main__":
    print(screendump(sys.argv[1], sys.argv[2]))
