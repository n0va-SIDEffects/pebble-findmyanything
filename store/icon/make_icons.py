#!/usr/bin/env python3
"""Icons for Find My Anything: store icons (144, 80, 48) and the launcher icon (25).

    python3 store/icon/make_icons.py            # concept sheet + all sizes

Style of the SIDEffect's icons: rounded square (radius 20 %) on dark navy, a
flat motif with thick strokes. The motif is the app's own colour language:
green = right next to it, amber = getting there, red = far away (the colours
the search screen uses, slightly saturated for the navy ground).

Drawn at 1024 px and reduced with LANCZOS. The launcher icon (25 px) is drawn
directly at 25 px without anti-aliasing: the SDK reduces PNGs to the 64-colour
palette, and soft edges turn into stray pixels there.

Decision: concept "c" (pin with waves). "a" read as the RSS symbol, "b" as a\ngeneric target, "d" turns to mush below 48 px. Switch with FINAL below.
"""
import math
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
RES = os.path.join(ROOT, "watch", "resources", "images")

N = 1024
NAVY = (22, 30, 42)
WHITE = (245, 248, 252)
GREEN = (150, 225, 110)    # "very close" as the Time 2 shows it, more saturated
AMBER = (245, 175, 80)     # "medium"
RED = (232, 95, 90)        # "far"

FINAL = "c"


def tile(size=N, radius=0.2, color=NAVY):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, size - 1, size - 1], int(size * radius), fill=color + (255,))
    return img, d


def arc(d, cx, cy, r, w, a0, a1, col):
    """Arc from a0 to a1 (clock degrees, 0 = up, clockwise), outer radius r, width w, round caps."""
    d.arc([cx - r, cy - r, cx + r, cy + r], a0 - 90, a1 - 90, fill=col, width=w)
    for a in (a0, a1):
        rm = r - w / 2
        x = cx + rm * math.sin(math.radians(a))
        y = cy - rm * math.cos(math.radians(a))
        d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=col)


def dot(d, x, y, r, col):
    d.ellipse([x - r, y - r, x + r, y + r], fill=col)


def icon_a():
    """Signal waves from a dot in the corner: green near, red far (the launcher's shape)."""
    img, d = tile()
    cx, cy = 270, 754
    dot(d, cx, cy, 92, WHITE)
    for r, col in ((330, GREEN), (520, AMBER), (710, RED)):
        arc(d, cx, cy, r, 118, 8, 82, col)
    return img


def icon_b():
    """Warmer/colder target: red outside, amber, green in the middle."""
    img, d = tile()
    c = N // 2
    arc(d, c, c, 400, 92, 0, 360, RED)
    arc(d, c, c, 272, 92, 0, 360, AMBER)
    dot(d, c, c, 146, GREEN)
    dot(d, c, c, 52, NAVY)
    return img


def icon_c():
    """Map pin with waves to both sides."""
    img, d = tile()
    c = N // 2
    # pin: circle on top of a triangle
    d.polygon([(c - 150, 470), (c + 150, 470), (c, 800)], fill=WHITE)
    dot(d, c, 400, 175, WHITE)
    dot(d, c, 400, 72, NAVY)
    for r, col in ((300, GREEN), (440, RED)):
        arc(d, c, 400, r, 74, 52, 128, col)
        arc(d, c, 400, r, 74, 232, 308, col)
    return img


def icon_d():
    """Magnifier, inside it the waves."""
    img, d = tile()
    cx, cy, r = 440, 440, 270
    arc(d, cx, cy, r, 84, 0, 360, WHITE)
    # handle
    x0, y0 = cx + (r - 10) * math.cos(math.radians(45)), cy + (r - 10) * math.sin(math.radians(45))
    x1, y1 = x0 + 250 * math.cos(math.radians(45)), y0 + 250 * math.sin(math.radians(45))
    d.line([(x0, y0), (x1, y1)], fill=WHITE, width=120)
    dot(d, x1, y1, 60, WHITE)
    sx, sy = cx - 95, cy + 95
    dot(d, sx, sy, 38, WHITE)
    for rr, col in ((130, GREEN), (210, AMBER)):
        arc(d, sx, sy, rr, 52, 10, 80, col)
    return img


CONCEPTS = {"a": ("Waves", icon_a), "b": ("Target", icon_b), "c": ("Pin", icon_c), "d": ("Magnifier", icon_d)}


def launcher():
    """25 x 25 for the watch launcher: the pin with one green and one red wave on
    each side, pixel-exact (a 4 px pin, 2 px waves, 1 px gaps)."""
    s = 25
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    white, green, red = (255, 255, 255), (85, 255, 0), (255, 85, 85)   # Pebble palette
    cx, cy = 12, 9
    d.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], fill=white)
    d.polygon([(cx - 3, cy + 3), (cx + 3, cy + 3), (cx, cy + 12)], fill=white)
    d.rectangle([cx - 1, cy - 1, cx + 1, cy + 1], fill=(0, 0, 0, 0))
    for r, col in ((8, green), (11, red)):
        d.arc([cx - r, cy - r, cx + r, cy + r], 142, 218, fill=col, width=2)
        d.arc([cx - r, cy - r, cx + r, cy + r], -38, 38, fill=col, width=2)
    return img


def sheet(out):
    sizes = (144, 80, 48, 25)
    cw = sum(sizes) + 20 * len(sizes) + 20
    img = Image.new("RGB", (cw * 2, len(CONCEPTS) * 184 + 20), (255, 255, 255))
    d = ImageDraw.Draw(img)
    img.paste((34, 38, 46), (cw, 0, cw * 2, img.height))
    for i, (key, (name, fn)) in enumerate(sorted(CONCEPTS.items())):
        master = fn()
        y0 = 20 + i * 184
        d.text((20, y0), "%s  %s%s" % (key.upper(), name, "  (chosen)" if key == FINAL else ""), fill=(30, 30, 30))
        for base in (0, cw):
            x = base + 20
            for s in sizes:
                im = master.resize((s, s), Image.LANCZOS)
                img.paste(im, (x, y0 + 20 + (144 - s)), im)
                x += s + 20
    img.save(out)


def main():
    sheet(os.path.join(HERE, "concepts.png"))
    master = CONCEPTS[FINAL][1]()
    master.save(os.path.join(HERE, "master_1024.png"))
    for s in (144, 80, 48):
        im = master.resize((s, s), Image.LANCZOS)
        im.save(os.path.join(HERE, "icon_%d_alpha.png" % s))
        bg = Image.new("RGBA", (s, s), (255, 255, 255, 255))
        bg.alpha_composite(im)
        bg.convert("RGB").save(os.path.join(HERE, "icon_%d.png" % s))   # portal: no alpha
    os.makedirs(RES, exist_ok=True)
    launcher().save(os.path.join(RES, "menu_icon.png"))
    print("written: concepts.png, master_1024.png, icon_{144,80,48}.png (+ _alpha), "
          "watch/resources/images/menu_icon.png")


if __name__ == "__main__":
    main()
