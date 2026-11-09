#!/usr/bin/env python3
"""Store banner (720 x 320) for Find My Anything, dark SIDEffect's series.

    python3 store/banner/make_banner.py                 # all platforms
    python3 store/banner/make_banner.py --platform chalk

Layout as Info Dial, Clarity, Stamina and Heart Rate FX: dark navy ground,
title bold and white, subtitle in the accent colour, two light-grey lines, the
SIDEffect's logo bottom left (185 px wide, full opacity) and the watch on the
right. Behind the watch, faint, the app's waves (lower left, upper right): green close to
the watch, amber, red further out, as on the search screen and in the icon.

One banner per platform (the store wants one per asset collection), each with
its own watch from the pebble-uhrmodelle skill (WATCHES) and the platform's
"very close" screenshot in the display (uhr_bild.py), tilted 9 degrees. emery
keeps `pebble_time_2` like the other SIDEffect's banners. Output:
store/icon/banner_720x320_<platform>.png (emery also as banner_720x320.png).
"""
import argparse
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SKILLS = os.path.expanduser("~/.hermes/skills/pebble")
UHR_BILD = os.path.join(SKILLS, "pebble-uhrmodelle", "scripts", "uhr_bild.py")
LOGO = os.path.join(SKILLS, "pebble-publish", "assets", "side_effects_logo.png")
MONT = os.path.join(HERE, "fonts", "Montserrat.ttf")
WATCHES = {
    "emery": "pebble_time_2",
    "basalt": "pebble_time",
    "chalk": "pebble_time_round_schwarz",
    "diorite": "pebble_2_aqua",
    "flint": "pebble_2_duo_schwarz",
    "gabbro": "pebble_round_2_mattschwarz",
}
ICON = os.path.join(ROOT, "store", "icon", "icon_144_alpha.png")

W, H, SS = 720, 320, 3
GROUND_L = (16, 21, 31)
GROUND_R = (22, 32, 50)
WHITE = (245, 248, 252)
GREY = (168, 178, 190)
LOGO_GREY = (225, 232, 240)
GREEN = (150, 225, 110)          # "very close" as the Time 2 shows it, a bit more saturated
AMBER = (245, 175, 80)
RED = (232, 95, 90)
ACCENT = GREEN
LOGO_WIDTH = 185
TEXT_RIGHT = 445                 # text ends here, the watch starts at about 480
WATCH_X, WATCH_Y = 598, H // 2 + 6

TITLE = "Find My Anything"
SUBTITLE = "Find any Bluetooth device nearby"
LINES = ["Your phone scans, your watch guides you", "Warmer · colder · found it"]


def mont(size, weight):
    f = ImageFont.truetype(MONT, size)
    f.set_variation_by_name(weight)
    return f


def prepare_logo():
    """Remove the white ground, lighten the lettering for the dark banner.
    Pac-Man and its black X stay unchanged (pebble-publish references/assets.md)."""
    logo = Image.open(LOGO).convert("RGBA")
    px = logo.load()
    for y in range(logo.height):
        for x in range(logo.width):
            r, g, b, a = px[x, y]
            if r > 235 and g > 235 and b > 235:
                px[x, y] = (r, g, b, 0)
    logo = logo.crop(logo.getbbox())
    px = logo.load()
    start = int(logo.width * 0.42)
    for y in range(logo.height):
        for x in range(start, logo.width):
            r, g, b, a = px[x, y]
            if a and r < 120 and g < 120 and b < 120:
                px[x, y] = LOGO_GREY + (a,)
    h = round(logo.height * LOGO_WIDTH / logo.width)
    return logo.resize((LOGO_WIDTH, h), Image.LANCZOS)


def watch(shot, name, height, max_width=270):
    """Watch with the screenshot in its display; round and wide watches get
    smaller so they do not run into the text."""
    out = os.path.join(os.environ.get("TMPDIR", "/tmp"), "fma_uhr_%s.png" % name)
    for _ in range(2):
        subprocess.run([sys.executable, UHR_BILD, "--shot", shot, "--uhr", name,
                        "--hoehe", str(height), "--out", out], check=True, capture_output=True)
        img = Image.open(out).convert("RGBA")
        if img.width <= max_width:
            return img
        height = int(height * max_width / img.width)
    return img


def ground():
    img = Image.new("RGBA", (W, H))
    d = ImageDraw.Draw(img)
    for x in range(W):
        t = x / (W - 1)
        d.line([(x, 0), (x, H)], fill=tuple(round(a + (b - a) * t) for a, b in zip(GROUND_L, GROUND_R)) + (255,))
    return img


def waves(img, cx, cy):
    """Faint waves out of the watch, green near, red far: towards the lower left
    (into the gap between text and logo) and the upper right, so they never run
    behind the text."""
    lay = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    for r, w, col in ((150, 12, GREEN), (196, 12, AMBER), (242, 12, RED)):
        box = [(cx - r) * SS, (cy - r) * SS, (cx + r) * SS, (cy + r) * SS]
        for a0, a1 in ((112, 164), (292, 344)):
            d.arc(box, a0, a1, fill=col + (78,), width=w * SS)
    img.alpha_composite(lay.resize((W, H), Image.LANCZOS))


def glow(img, cx, cy, rx, ry, color=(150, 168, 195), alpha=46, blur=58):
    """Soft light behind the black watch, otherwise it disappears in the ground."""
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(layer).ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=color + (alpha,))
    img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(blur)))


def place_watch(img, uhr, cx, cy, tilt=0.0):
    if tilt:
        uhr = uhr.rotate(tilt, resample=Image.BICUBIC, expand=True)
    sil = Image.new("RGBA", uhr.size, (0, 0, 0, 0))
    sil.paste((0, 0, 0, 150), (0, 0), uhr.split()[-1])
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    shadow.alpha_composite(sil, (cx - uhr.width // 2 + 4, cy - uhr.height // 2 + 8))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(9)))
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    layer.alpha_composite(uhr, (cx - uhr.width // 2, cy - uhr.height // 2))
    img.alpha_composite(layer)


def fit(text, weight, size, width):
    while size > 14 and mont(size, weight).getbbox(text)[2] > width:
        size -= 1
    return size


def text_block(img, logo):
    d = ImageDraw.Draw(img)
    x = 40
    size = fit(TITLE, "ExtraBold", 54, TEXT_RIGHT - x)
    d.text((x - 2, 40), TITLE, font=mont(size, "ExtraBold"), fill=WHITE)
    d.text((x, 112), SUBTITLE, font=mont(fit(SUBTITLE, "SemiBold", 21, TEXT_RIGHT - x), "SemiBold"), fill=ACCENT)
    lsize = min(fit(line, "Medium", 17, TEXT_RIGHT - x) for line in LINES)
    for i, line in enumerate(LINES):
        d.text((x, 154 + i * 26), line, font=mont(lsize, "Medium"), fill=GREY)
    img.alpha_composite(logo, (30, H - logo.height - 8))
    return size, lsize


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--platform", choices=sorted(WATCHES), action="append",
                   help="only these platforms (default: all)")
    a = p.parse_args()
    logo = prepare_logo()
    for platform in a.platform or sorted(WATCHES):
        shot = os.path.join(ROOT, "store", "screenshots_" + platform, "1_very_close.png")
        img = ground()
        waves(img, WATCH_X, WATCH_Y)
        glow(img, WATCH_X, H // 2, 120, 150)
        place_watch(img, watch(shot, WATCHES[platform], 360), WATCH_X, WATCH_Y, tilt=9)
        size, lsize = text_block(img, logo)
        out = os.path.join(ROOT, "store", "icon", "banner_720x320_%s.png" % platform)
        img.convert("RGB").save(out)
        if platform == "emery":
            img.convert("RGB").save(os.path.join(ROOT, "store", "icon", "banner_720x320.png"))
        print(out, "title %d pt, lines %d pt" % (size, lsize))


if __name__ == "__main__":
    main()
