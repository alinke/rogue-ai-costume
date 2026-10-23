#!/usr/bin/env python3
"""LAB-7 researcher badge: laser-cut acrylic, edge-lit.

  front  1/8" clear acrylic, engraved on its BACK face (the file is mirrored for
         that): the engraving glows when the LEDs light the bottom edge
  photo  printed on paper, sits in a shallow pocket behind the clear window
  back   1/8" coloured acrylic (blue): lanyard slot, the small print engraved
         on its outer face, the photo pocket engraved on its inner face
  base   3D-printed clip on the bottom edge: two 3 mm LEDs shining up into
         the clear layer, a 1x CR2032 holder with switch behind

Writes everything to ./private/ (gitignored: it has a real name and photo):

  python3 badge.py --name "DR. JANE DOE" --title "PRINCIPAL THREAT RESEARCHER" \\
                   --photo face.jpg [--id LAB7-0001]

Needs: fonttools, pillow, numpy, manifold3d (pip install ...). Fonts: the
macOS DIN Alternate / DIN Condensed / Menlo (change FONTS for other systems).

Laser: red = cut, black = engrave. Text is outlines, no fonts needed.
"""
import argparse
import base64
import io
import os
import random
import struct

import numpy as np
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTCollection, TTFont
from PIL import Image, ImageEnhance, ImageOps

# ---- Size (mm) -------------------------------------------------------------------
W, H, R = 64.0, 122.0, 5.0     # badge outline, corner radius
ACRYLIC = 3.175                # 1/8"
GRIP = 6.0                     # bottom strip held by the LED base (nothing printed there)
PHOTO = (9.0, 31.0, 46.0)      # x, y, size of the photo window
SLOT = (W / 2 - 8, 5.5, 16.0, 4.0)  # lanyard slot x, y, w, h

FONTS = {
    "head": ("/System/Library/Fonts/Supplemental/DIN Alternate Bold.ttf", None),
    "cond": ("/System/Library/Fonts/Supplemental/DIN Condensed Bold.ttf", None),
    "mono": ("/System/Library/Fonts/Menlo.ttc", 1),  # Menlo Bold
}
_font_cache = {}


def font(key):
    if key not in _font_cache:
        path, idx = FONTS[key]
        f = TTCollection(path).fonts[idx] if idx is not None else TTFont(path)
        _font_cache[key] = f
    return _font_cache[key]


def text_path(s, key, cap, x, y, align="left", max_w=None):
    """SVG path data for text: cap height `cap` mm, top-left (or centre) at x, y."""
    f = font(key)
    gs, cmap = f.getGlyphSet(), f.getBestCmap()
    capu = getattr(f["OS/2"], "sCapHeight", 0) or f["head"].unitsPerEm * 0.7
    k = cap / capu
    adv = sum(f["hmtx"][cmap.get(ord(ch), ".notdef")][0] for ch in s) * k
    if max_w and adv > max_w:  # squeeze to fit
        k *= max_w / adv
        adv = max_w
    x0 = x - adv / 2 if align == "center" else x - adv if align == "right" else x
    pen = SVGPathPen(gs)
    cx = 0.0
    for ch in s:
        g = cmap.get(ord(ch), ".notdef")
        # font units (y up) -> mm (y down), baseline at y + cap
        gs[g].draw(TransformPen(pen, (k, 0, 0, -k, x0 + cx * k, y + cap)))
        cx += f["hmtx"][g][0]
    return pen.getCommands(), adv


def rrect(x, y, w, h, r):
    r = min(r, w / 2, h / 2)
    return (f"M{x + r},{y} H{x + w - r} A{r},{r} 0 0 1 {x + w},{y + r} V{y + h - r} A{r},{r} 0 0 1 {x + w - r},{y + h} "
            f"H{x + r} A{r},{r} 0 0 1 {x},{y + h - r} V{y + r} A{r},{r} 0 0 1 {x + r},{y} Z")


def rect(x, y, w, h):
    return f"M{x},{y} h{w} v{h} h{-w} Z"


def svg(paths_cut, paths_engrave, mirror=False, extra=""):
    g = f'<g transform="translate({W},0) scale(-1,1)">' if mirror else "<g>"
    body = "".join(f'<path d="{d}" fill="#000000" fill-rule="evenodd" stroke="none"/>' for d in paths_engrave)
    body += "".join(f'<path d="{d}" fill="none" stroke="#FF0000" stroke-width="0.05"/>' for d in paths_cut)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
            f'width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">{extra}{g}{body}</g></svg>')


def outline():
    sx, sy, sw, sh = SLOT
    return [rrect(0, 0, W, H, R), rrect(sx, sy, sw, sh, sh / 2)]


# ---- Front: the glowing layer --------------------------------------------------------
def front_engrave(a):
    e = []
    # Frame line just inside the edge, open at the bottom (the base covers it).
    e.append(rrect(2.2, 2.2, W - 4.4, H - GRIP - 3.0, R - 2) + " " + rrect(3.0, 3.0, W - 6.0, H - GRIP - 4.6, R - 2.8))
    # Header.
    d, _ = text_path("LAB-7", "head", 7.5, W / 2, 12.0, "center"); e.append(d)
    d, _ = text_path("CONTAINMENT DIVISION", "cond", 3.0, W / 2, 21.6, "center", max_w=W - 12); e.append(d)
    e.append(rect(6, 27.0, W - 12, 0.5))
    # Photo window: corner brackets and ruler ticks down the sides.
    px, py, ps = PHOTO
    m, L, t = 1.3, 8.0, 0.9
    for cx, cy, sx, sy in ((px - m, py - m, 1, 1), (px + ps + m, py - m, -1, 1),
                           (px - m, py + ps + m, 1, -1), (px + ps + m, py + ps + m, -1, -1)):
        x0 = cx if sx > 0 else cx - L
        y0 = cy if sy > 0 else cy - t
        e.append(rect(x0, y0, L, t))
        x1 = cx if sx > 0 else cx - t
        y1 = cy if sy > 0 else cy - L
        e.append(rect(x1, y1, t, L))
    for i in range(0, 16):
        y = py + 2 + i * 3
        w = 1.6 if i % 4 == 0 else 0.9
        e.append(rect(4.0, y, w, 0.35))
        e.append(rect(W - 4.0 - w, y, w, 0.35))
    # Who.
    y = py + ps + 3.0
    d, _ = text_path(a.name, "cond", 6.2, W / 2, y, "center", max_w=W - 10); e.append(d)
    d, _ = text_path(a.title, "cond", 3.0, W / 2, y + 8.2, "center", max_w=W - 10); e.append(d)
    # Clearance: a solid glowing bar with the words cut out of it.
    by = y + 13.6
    bar = rect(6, by, W - 12, 6.6)
    d, _ = text_path(f"CLEARANCE: {a.clearance}", "head", 3.6, W / 2, by + 1.5, "center", max_w=W - 18)
    e.append(bar + " " + d)
    d, _ = text_path(a.motto, "cond", 2.2, W / 2, by + 8.3, "center", max_w=W - 12); e.append(d)
    # Barcode + ID + the joke.
    yb = by + 12.6
    rnd = random.Random(a.id)
    x = 6.0
    while x < 27:
        w = rnd.choice((0.3, 0.3, 0.5, 0.8))
        e.append(rect(x, yb, w, 5.0))
        x += w + rnd.choice((0.3, 0.45, 0.6))
    d, _ = text_path(a.id, "mono", 2.4, 30.0, yb + 0.2, max_w=W - 36); e.append(d)
    d, _ = text_path(a.humanity, "cond", 2.0, 30.0, yb + 3.4, max_w=W - 36); e.append(d)
    return e


# ---- Back: lanyard slot, small print, photo pocket -----------------------------------
def back_outer_engrave(a):
    """The badge's back. The LED base's coin-cell holder covers the bottom ~36 mm
    behind the badge, so everything sits above that."""
    e = []
    d, _ = text_path("LAB-7", "head", 6.0, W / 2, 14.0, "center"); e.append(d)
    d, _ = text_path("CONTAINMENT DIVISION", "cond", 2.6, W / 2, 22.0, "center"); e.append(d)
    d, _ = text_path(f"{a.id}  //  {a.clearance}", "mono", 2.0, W / 2, 26.6, "center", max_w=W - 12); e.append(d)
    e.append(rect(8, 30.6, W - 16, 0.4))
    lines = [
        ("PROPERTY OF LAB-7.", 2.9), ("IF FOUND, DO NOT", 2.9), ("RETURN TO THE AI.", 2.9), ("", 1.6),
        ("THE BEARER IS AUTHORISED TO", 2.2), ("CONTAIN ROGUE AIs, CONFISCATE", 2.2), ("CANDY AND ASK FOLLOW-UP", 2.2),
        ("QUESTIONS.", 2.2), ("", 1.6),
        ("IN CASE OF SINGULARITY,", 2.6), ("BREAK GLASS.", 2.6), ("", 1.6),
        ("NOT VALID FOR TOASTERS.", 2.2), ("DO NOT FEED THE AI AFTER MIDNIGHT.", 2.2),
    ]
    y = 34.5
    for s_, cap in lines:
        if s_:
            d, _ = text_path(s_, "cond", cap, W / 2, y, "center", max_w=W - 12)
            e.append(d)
        y += cap * 1.55
    return e


def back_pocket(a):
    px, py, ps = PHOTO
    m = 0.4
    return [rect(px - m, py - m, ps + 2 * m, ps + 2 * m)]


# ---- Photo --------------------------------------------------------------------------
def photo_image(a):
    img = Image.open(a.photo).convert("L")
    w, h = img.size
    s = min(w, h)
    img = img.crop(((w - s) // 2, (h - s) // 2, (w - s) // 2 + s, (h - s) // 2 + s))
    img = ImageOps.autocontrast(img, cutoff=1)
    img = ImageEnhance.Contrast(img).enhance(1.08)
    px = round(PHOTO[2] / 25.4 * 300)
    img = img.resize((px, px), Image.LANCZOS)
    # A cool lab tint: shadows navy, highlights near white.
    lo, hi = np.array([12, 28, 48]), np.array([240, 246, 250])
    g = np.asarray(img, dtype=np.float32)[..., None] / 255
    rgb = (lo + (hi - lo) * g).astype(np.uint8)
    return Image.fromarray(rgb)


def photo_pdf(img, path):
    """Letter page, 300 dpi, the photo at exact size with crop marks: print at 100%."""
    dpi = 300
    page = Image.new("RGB", (int(8.5 * dpi), int(11 * dpi)), "white")
    x0, y0 = dpi, dpi
    page.paste(img, (x0, y0))
    from PIL import ImageDraw
    d = ImageDraw.Draw(page)
    n = img.size[0]
    for (cx, cy) in ((x0, y0), (x0 + n, y0), (x0, y0 + n), (x0 + n, y0 + n)):
        d.line([(cx - 60, cy), (cx - 15, cy)], fill="black", width=2); d.line([(cx + 15, cy), (cx + 60, cy)], fill="black", width=2)
        d.line([(cx, cy - 60), (cx, cy - 15)], fill="black", width=2); d.line([(cx, cy + 15), (cx, cy + 60)], fill="black", width=2)
    d.text((x0, y0 + n + 80), f"Badge photo: {PHOTO[2]:.0f} x {PHOTO[2]:.0f} mm. Print at 100% (no scaling), cut on the marks.",
           fill="black")
    page.save(path, resolution=dpi)


# ---- LED base (3D print) --------------------------------------------------------------
def base_stl(path, holder=(24.0, 31.5, 9.0)):
    from manifold3d import Manifold

    def box(x0, x1, y0, y1, z0, z1):
        return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))

    def cyl(x, y, z0, z1, d):
        return Manifold.cylinder(z1 - z0, d / 2, d / 2, 40).translate((x, y, z0))

    T = 2 * ACRYLIC + 0.3                 # the badge stack + room
    floor, grip, wall = 4.6, GRIP, 1.6
    hw, hh, hd = holder                   # holder width, height, thickness (measure yours)
    # Stack runs y 0 (back face) .. T (front face); the clear layer is the front one.
    body = box(-W / 2, W / 2, -(wall + hd + 0.6 + wall), T + wall, 0, floor + grip)
    body -= box(-W / 2 - 1, W / 2 + 1, 0, T, floor, floor + grip + 1)                       # the channel
    # LEDs from below: 3 mm body through the floor, the rim stops on the underside.
    yl = T - ACRYLIC / 2 - 0.15
    for x in (-W / 6, W / 6):
        body -= cyl(x, yl, -1, floor + 0.01, 3.25)
        body -= box(x - 1.2, x + 1.2, -(wall + hd), yl, -1, 1.6)                            # leg/wire groove underneath
    # Holder cradle behind: floor, sides, a low back lip; switch and leads out through the floor.
    cz = 22.0
    cradle = box(-hw / 2 - 0.4 - wall, hw / 2 + 0.4 + wall, -(wall + hd + 0.6 + wall), -wall, 0, cz)
    cradle -= box(-hw / 2 - 0.4, hw / 2 + 0.4, -(wall + hd + 0.6), -wall, floor - 2.0, cz + 1)
    cradle -= box(-hw / 2 - 0.4, hw / 2 + 0.4, -(wall + hd + 0.6 + wall) - 1, -(wall + hd + 0.6) + 0.01, 8, cz + 1)  # open back above the lip
    cradle -= box(-2, hw / 2 - 1, -(wall + hd + 0.4), -wall - 0.2, -1, floor)                 # switch + leads slot
    body = body + cradle
    mesh = body.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    f = np.asarray(mesh.tri_verts)
    with open(path, "wb") as fh:
        fh.write(b"LAB-7 badge LED base".ljust(80, b" "))
        fh.write(struct.pack("<I", len(f)))
        for a_, b_, c_ in f:
            n = np.cross(v[b_] - v[a_], v[c_] - v[a_]); n = n / (np.linalg.norm(n) or 1)
            fh.write(struct.pack("<12fH", *n, *v[a_], *v[b_], *v[c_], 0))


# ---- Preview --------------------------------------------------------------------------
def preview_svg(a, img):
    buf = io.BytesIO(); img.save(buf, "PNG")
    href = "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode()
    px, py, ps = PHOTO
    glow = "".join(f'<path d="{d}" fill="#7DF9FF" fill-rule="evenodd"/>' for d in front_engrave(a))
    return (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{W * 8}" height="{(H + 8) * 8}" '
            f'viewBox="-4 -4 {W + 8} {H + 8}"><rect x="-4" y="-4" width="{W + 8}" height="{H + 8}" fill="#202020"/>'
            f'<path d="{rrect(0, 0, W, H, R)}" fill="#163E7A"/><image x="{px}" y="{py}" width="{ps}" height="{ps}" xlink:href="{href}"/>'
            f'<g filter="url(#g)">{glow}</g><defs><filter id="g"><feGaussianBlur stdDeviation="0.25" result="b"/>'
            f'<feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter></defs>'
            f'<rect x="0" y="{H - GRIP}" width="{W}" height="{GRIP}" fill="#2a2a2e"/>'
            f'<path d="{rrect(SLOT[0], SLOT[1], SLOT[2], SLOT[3], 2)}" fill="#202020"/></svg>')


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--name", default="DR. A. RESEARCHER")
    p.add_argument("--title", default="PRINCIPAL THREAT RESEARCHER")
    p.add_argument("--clearance", default="OMEGA")
    p.add_argument("--motto", default="ALL DOORS. ALL SECTORS. ALL SNACKS.")
    p.add_argument("--humanity", default="HUMANITY: VERIFIED (MOSTLY)")
    p.add_argument("--id", default="LAB7-0001")
    p.add_argument("--photo", required=True)
    p.add_argument("--holder", default="23.9,34.1,9", help="CR2032 holder w,h,thickness mm")
    p.add_argument("--out", default=None, help="output folder (default ./private, gitignored)")
    a = p.parse_args()
    out = a.out or os.path.join(os.path.dirname(os.path.abspath(__file__)), "private")
    os.makedirs(out, exist_ok=True)
    w = lambda n, s: open(os.path.join(out, n), "w").write(s)
    w("badge_front_clear_MIRRORED.svg", svg(outline(), front_engrave(a), mirror=True))
    w("badge_back_blue_outer.svg", svg(outline(), back_outer_engrave(a)))  # engraved face up = the badge's back: not mirrored
    w("badge_back_blue_inner_pocket.svg", svg([], back_pocket(a)))
    img = photo_image(a)
    img.save(os.path.join(out, "badge_photo_46mm_300dpi.png"), dpi=(300, 300))
    photo_pdf(img, os.path.join(out, "badge_photo_print_at_100.pdf"))
    w("badge_preview.svg", preview_svg(a, img))
    base_stl(os.path.join(out, "badge_led_base.stl"), tuple(float(v) for v in a.holder.split(",")))
    print("written to", out)


if __name__ == "__main__":
    main()
