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

The Android app uses the same motif: android_icons() writes the adaptive icon
(foreground, themed monochrome) and the status-bar icon as vector drawables from
the same geometry (PIN, WAVES below), plus SVG previews in store/icon/preview/.

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


# Concept "c", shared with the Android vectors (1024 grid)
PIN = dict(cx=512, cy=400, r=175, hole=72, tip=800)
WAVES = [dict(r=300, w=74, col=GREEN), dict(r=440, w=74, col=RED)]   # outer radius, width
WAVE_SPANS = [(52, 128), (232, 308)]                                 # clock degrees


def pin_tangents():
    """Points where the straight sides of the pin touch the circle."""
    cx, cy, r, tip = PIN["cx"], PIN["cy"], PIN["r"], PIN["tip"]
    phi = math.acos(r / (tip - cy))
    return (cx + r * math.sin(phi), cy + r * math.cos(phi)), (cx - r * math.sin(phi), cy + r * math.cos(phi))


def pin_polygon(steps=180):
    """Teardrop outline: tip, right tangent, over the top, left tangent."""
    cx, cy, r, tip = PIN["cx"], PIN["cy"], PIN["r"], PIN["tip"]
    (rx, ry), _ = pin_tangents()
    a0 = math.atan2(ry - cy, rx - cx)
    span = 2 * math.pi - 2 * (math.pi / 2 - a0)
    pts = [(cx, tip)]
    for i in range(steps + 1):
        a = a0 - span * i / steps
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def icon_c():
    """Map pin with waves to both sides."""
    img, d = tile()
    d.polygon(pin_polygon(), fill=WHITE)
    dot(d, PIN["cx"], PIN["cy"], PIN["hole"], NAVY)
    for wave in WAVES:
        for a0, a1 in WAVE_SPANS:
            arc(d, PIN["cx"], PIN["cy"], wave["r"], wave["w"], a0, a1, wave["col"])
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


def _hex(col):
    return "#FF%02X%02X%02X" % col


def vector_paths(scale, ox, oy, colors=True):
    """Path elements of concept "c" for an Android vector (or SVG): grid point
    (x, y) -> (ox + x * scale, oy + y * scale). The pin is one path with the hole
    wound the other way (nonZero), so it is transparent in every layer."""
    def P(x, y):
        return "%.2f,%.2f" % (ox + x * scale, oy + y * scale)

    cx, cy, r, hole, tip = PIN["cx"], PIN["cy"], PIN["r"], PIN["hole"], PIN["tip"]
    (rx, ry), (lx, ly) = pin_tangents()
    rs, hs = "%.2f" % (r * scale), "%.2f" % (hole * scale)
    pin = ("M%s L%s A%s,%s 0 1,0 %s Z " % (P(cx, tip), P(rx, ry), rs, rs, P(lx, ly)) +
           "M%s A%s,%s 0 1,1 %s A%s,%s 0 1,1 %s Z" % (P(cx - hole, cy), hs, hs, P(cx + hole, cy), hs, hs,
                                                      P(cx - hole, cy)))
    out = [("fill", WHITE if colors else (255, 255, 255), pin, 0)]
    for wave in WAVES:
        rc = wave["r"] - wave["w"] / 2
        for a0, a1 in WAVE_SPANS:
            p0 = P(cx + rc * math.sin(math.radians(a0)), cy - rc * math.cos(math.radians(a0)))
            p1 = P(cx + rc * math.sin(math.radians(a1)), cy - rc * math.cos(math.radians(a1)))
            rr = "%.2f" % (rc * scale)
            out.append(("stroke", wave["col"] if colors else (255, 255, 255),
                        "M%s A%s,%s 0 0,1 %s" % (p0, rr, rr, p1), wave["w"] * scale))
    return out


def content_box():
    """Centre and radius around the centre that holds the whole motif (incl. round caps)."""
    cx, cy = PIN["cx"], PIN["cy"]
    top = cy - max((w["r"] - w["w"] / 2) * math.cos(math.radians(WAVE_SPANS[0][0])) + w["w"] / 2 for w in WAVES)
    mid = (top + PIN["tip"]) / 2
    far = 0
    for w in WAVES:
        rc = w["r"] - w["w"] / 2
        for a in (WAVE_SPANS[0][0], WAVE_SPANS[0][1]):
            x = rc * math.sin(math.radians(a))
            y = cy - rc * math.cos(math.radians(a)) - mid
            far = max(far, math.hypot(x, y) + w["w"] / 2)
    far = max(far, PIN["tip"] - mid)
    return cx, mid, far


def write_vector(path, size_dp, radius_dp, colors, comment):
    cx, mid, far = content_box()
    scale = radius_dp / far
    ox, oy = size_dp / 2 - cx * scale, size_dp / 2 - mid * scale
    lines = ['<?xml version="1.0" encoding="utf-8"?>',
             "<!-- %s Generated by store/icon/make_icons.py, edit it there. -->" % comment,
             '<vector xmlns:android="http://schemas.android.com/apk/res/android"',
             '        android:width="%ddp"' % size_dp, '        android:height="%ddp"' % size_dp,
             '        android:viewportWidth="%d"' % size_dp, '        android:viewportHeight="%d">' % size_dp]
    for kind, col, data, width in vector_paths(scale, ox, oy, colors):
        if kind == "fill":
            lines.append('    <path android:fillColor="%s"\n          android:pathData="%s"/>' % (_hex(col), data))
        else:
            lines.append('    <path android:strokeColor="%s" android:strokeWidth="%.2f"\n'
                         '          android:strokeLineCap="round" android:fillColor="#00000000"\n'
                         '          android:pathData="%s"/>' % (_hex(col), width, data))
    lines.append("</vector>")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return scale, ox, oy


def write_svg(path, size, radius, colors, bg=None, mask=None):
    """Same paths as SVG, to look at the Android icons in a browser."""
    cx, mid, far = content_box()
    scale = radius / far
    ox, oy = size / 2 - cx * scale, size / 2 - mid * scale
    body = []
    if bg:
        if mask == "circle":
            body.append('<circle cx="%g" cy="%g" r="%g" fill="rgb%s"/>' % (size / 2, size / 2, size / 3, bg))
        else:
            body.append('<rect width="%g" height="%g" fill="rgb%s"/>' % (size, size, bg))
    for kind, col, data, width in vector_paths(scale, ox, oy, colors):
        if kind == "fill":
            body.append('<path fill="rgb%s" d="%s"/>' % (col, data))
        else:
            body.append('<path fill="none" stroke="rgb%s" stroke-width="%.2f" stroke-linecap="round" d="%s"/>'
                        % (col, width, data))
    with open(path, "w", encoding="utf-8") as f:
        f.write('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %g %g">%s</svg>\n'
                % (size, size, size, size, "".join(body)))


ANDROID_RES = os.path.join(ROOT, "android", "app", "src", "main", "res")


def android_icons():
    """Adaptive icon (108 dp layers, motif inside the 66 dp safe circle), themed
    monochrome layer, status-bar icon (24 dp, white, 1 dp margin)."""
    d = os.path.join(ANDROID_RES, "drawable")
    write_vector(os.path.join(d, "ic_launcher_foreground.xml"), 108, 32.5, True,
                 "Pin with waves, same motif as the watch and store icons.")
    write_vector(os.path.join(d, "ic_launcher_monochrome.xml"), 108, 32.5, False,
                 "Themed icon (Android 13+): one colour, the system tints it.")
    write_vector(os.path.join(d, "ic_stat_pin.xml"), 24, 11, False,
                 "Status-bar icon: white silhouette.")
    prev = os.path.join(HERE, "preview")
    os.makedirs(prev, exist_ok=True)
    write_svg(os.path.join(prev, "android_adaptive_circle.svg"), 108, 32.5, True, NAVY, "circle")
    write_svg(os.path.join(prev, "android_adaptive_square.svg"), 108, 32.5, True, NAVY)
    write_svg(os.path.join(prev, "android_status_bar.svg"), 24, 11, False, (40, 40, 40))


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
    android_icons()
    print("written: concepts.png, master_1024.png, icon_{144,80,48}.png (+ _alpha), "
          "watch/resources/images/menu_icon.png, android drawables, preview/*.svg")


if __name__ == "__main__":
    main()
