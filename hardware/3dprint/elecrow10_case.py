#!/usr/bin/env python3
"""Desk case for the Elecrow CrowPanel Advanced 10.1" (ESP32-P4, costume/ env
elecrow-p4-10), landscape, standing on two kickstand legs, with a LiPo
(59 x 58 x 10.5 mm) and the two stereo speakers inside. Parts:

  elecrow10_front.stl        the bezel: frames the screen, corner standoffs the
                             board screws onto, USB-C and microSD openings
  elecrow10_back.stl         the back shell: battery bay, two speaker frames
                             under radar grilles, a slot for the power-switch
                             slider, pin holes to BOOT/RESET, light pipes for
                             the PWR/CHG LEDs, bosses for the legs
  elecrow10_front_inlay.stl, elecrow10_back_inlay.stl
                             the LAB-7 markings, flush (second colour, or
                             leave them out and the grooves stay as engraving)
  elecrow10_leg.stl          kickstand leg -- print two, flange down
  elecrow10_slider.stl       rides in the back's slot and moves the board's
                             power switch (it goes in from the inside)

Coordinates are the board's own, from Elecrow's STEP model
(github.com/Elecrow-RD/CrowPanel-Advanced-10.1inch-ESP32-P4-HMI-AI-Display-
1024x600-IPS-Touch-Screen, "3D file", 20251230): x along the long side, y up
(the panel's flex side is the top), z through the board: PCB from z -1.7
(front) to 0 (back), glass front at z -8.5, parts on the back up to z 8.1.
Seen from the front, +x is LEFT (the USB-C sockets are on the right).
The visible area is the window of Elecrow's own case for this board.
Needs Python 3 with manifold3d + numpy (helpers shared with scanner_case.py):

  python3 elecrow10_case.py
"""
import os

import numpy as np
from manifold3d import CrossSection, FillRule, Manifold, OpType

from scanner_case import (GLYPHS, INLAY, TRACE_W, box, crect, cslab, cyl, hazard, radar_grille, rrect, strokes,
                          text, to_bed, write_stl)

BATTERY = (59.0, 58.0, 10.5)   # mm: along x, along y, thick
SPEAKER = (30.0, 20.0, 7.5)    # mm: rectangular speakers (x, y, depth), glued into their frames
TILT = 20.0                    # degrees the screen leans back from vertical
FIT = 0.4                      # around the board
WALL = 2.4
FRONT = 2.0                    # front plate
BACK = 2.0                     # back plate
OVERLAP = 0.5                  # bezel over the active area (hides the board's play)
PILOT = 2.6                    # M3 self-tapping into plastic
SCREW_LEN = 16.0               # board screws: M3 x 16 from the back
Z_SPLIT = 4.0                  # front / back joint (above the USB-C and microSD sockets)
UNDER_BATTERY = 2.3            # battery's clearance over the board's back

PCB = (-123.5, 123.5, -73.5, 73.5)
HOLES = [(sx * 120.5, sy * 70.5) for sx in (-1, 1) for sy in (-1, 1)]  # 3.2 mm, M3
GLASS = (-117.5, 117.5, -71.5, 71.5, -8.5)       # x0, x1, y0, y1, front face z
ACTIVE = (-112.0, 110.72, -65.64, 59.64)         # 222.72 x 125.28
USB_Y = (-10.47, 10.47)        # two USB-C sockets on the -x edge, z -1.0..3.16
TF = (96.05, 112.2)            # microSD socket, its mouth on the +y edge, z -0.6..2.05
LCD_FLEX = (-7.75, 7.75)       # the panel's flex wraps round the board's top edge here
SWITCH = dict(x=-118.73, y=(-28.63, -26.25), lever=1.5, top=5.15, body_top=3.0)  # lever centre travel
BUTTONS = [(119.25, 56.25), (119.25, 41.75)]     # BOOT / RESET, tops at z 4.5
LEDS = [(-121.1, 0.0), (-116.3, 0.0)]            # PWR / CHG, on the back
BATTERY_AT = (-42.0, 0.0)
SPEAKERS_AT = [(-90.0, 45.0), (90.0, 45.0)]
LEG_X = (-105.0, 105.0)
LEG = dict(width=20.0, flange=6.0, fin=5.0, y0=-72.0, y1=-18.0, reach=78.0, foot=16.0)
LEG_SCREWS = [(6.25, -49.0), (-6.25, -26.0)]     # x relative to the leg, y absolute; M3 x 14, heads on the flange

GLYPHS.update({
    "G": [[(4, 5), (3, 6), (1, 6), (0, 5), (0, 1), (1, 0), (3, 0), (4, 1), (4, 3), (2, 3)]],
    ".": [[(1.8, 0), (2.2, 0)]],
})


def geometry():
    g = {}
    px0, px1, py0, py1 = PCB
    g["in"] = (px0 - FIT, px1 + FIT, py0 - FIT, py1 + FIT)
    g["out"] = (px0 - FIT - WALL, px1 + FIT + WALL, py0 - FIT - WALL, py1 + FIT + WALL)
    g["z_front"] = GLASS[4] - 0.2           # front plate's inside, just clear of the glass
    g["z_top"] = g["z_front"] - FRONT       # the front face
    g["z_back_in"] = UNDER_BATTERY + BATTERY[2]
    g["z_back"] = g["z_back_in"] + BACK     # the back face
    a0, a1, b0, b1 = ACTIVE
    g["win"] = (a0 - OVERLAP, a1 + OVERLAP, b0 - OVERLAP, b1 + OVERLAP)
    return g


# Seen from the front (-z) "right" is -x; from the back (+z) it's +x.
def on_front(cs, x, y):
    return cs.mirror((1, 0)).translate((x, y))


def on_back(cs, x, y):
    return cs.translate((x, y))


def usb_openings(g, z0, z1):
    ix0, ox0 = g["in"][0], g["out"][0]
    cut = []
    for y in USB_Y:   # the plugs' overmold: 12.5 x 6.5
        cut.append(Manifold.batch_hull([box(ox0 - 1, ix0 + 1, y - 5.0, y + 5.0, -2.1, 4.3),
                                        box(ox0 - 1, ix0 + 1, y - 6.5, y + 6.5, -0.6, 2.8)]))
    return Manifold.batch_boolean(cut, OpType.Add) ^ box(ox0 - 2, ix0 + 2, -50, 50, z0, z1)


def tf_opening(g, z0, z1):
    iy1, oy1 = g["in"][3], g["out"][3]
    c = (TF[0] + TF[1]) / 2
    slot = box(c - 6.5, c + 6.5, iy1 - 1, oy1 + 1, -0.4, 2.8)
    notch = Manifold.batch_hull([box(c - 9, c + 9, oy1 - 0.01, oy1 + 1, -2.5, 4.5),   # fingernail scoop
                                 box(c - 6.5, c + 6.5, oy1 - 1.6, oy1 - 1.59, -0.4, 2.8)])
    return (slot + notch) ^ box(c - 20, c + 20, iy1 - 2, oy1 + 2, z0, z1)


def front_part(g):
    """-> (front, inlay)"""
    ox0, ox1, oy0, oy1 = g["out"]
    ix0, ix1, iy0, iy1 = g["in"]
    zt, zf = g["z_top"], g["z_front"]
    body = Manifold.batch_hull([cslab(crect(ox0, ox1, oy0, oy1, 9.0), zt + 1.2, Z_SPLIT),
                                cslab(crect(ox0 + 1.2, ox1 - 1.2, oy0 + 1.2, oy1 - 1.2, 8.4), zt, zt + 0.01)])
    body = body - box(ix0, ix1, iy0, iy1, zf, Z_SPLIT + 1)
    # The panel's flex bends round the board's top edge: room for it.
    body = body - box(LCD_FLEX[0] - 1.5, LCD_FLEX[1] + 1.5, iy1 - 0.5, iy1 + 1.6, zf, Z_SPLIT + 1)
    # Corner standoffs: the board's front rests on them (the glass stays just
    # clear of the plate), M3 pilot holes for the screws from the back.
    gx0, gx1, gy0, gy1, _ = GLASS
    for x, y in HOLES:
        sx, sy = np.sign(x), np.sign(y)
        post = Manifold.batch_hull([cyl(x, y, zf - 0.1, -1.7, 5.2),
                                    box(min(x, ix1 * sx), max(x, ix1 * sx), min(y, iy1 * sy), max(y, iy1 * sy),
                                        zf - 0.1, -1.7)])
        post = post - box(gx0 - 0.25, gx1 + 0.25, gy0 - 0.25, gy1 + 0.25, zf - 1, 0)
        body = body + post - cyl(x, y, zf + 0.6, 0, PILOT)
    # Screen window, chamfered outward.
    w0, w1, v0, v1 = g["win"]
    win = Manifold.batch_hull([cslab(rrect(w0, w1, v0, v1, 1.0), zf - 0.01, zf + 0.5),
                               cslab(rrect(w0 - FRONT, w1 + FRONT, v0 - FRONT, v1 + FRONT, 2.0), zt - 0.5, zt + 0.01)])
    body = body - win
    body = body - usb_openings(g, -10, Z_SPLIT + 1) - tf_opening(g, -10, Z_SPLIT + 1)
    # Markings (inlay): LAB-7 over the screen between circuit traces,
    # CONTAINMENT UNIT on the chin.
    art = []
    top = (v1 + FRONT + oy1 - 1.2) / 2
    lab, llen = text("LAB-7", 7.0, 1.3)
    art.append(on_front(lab, 0, top))
    for s in (-1, 1):
        xa, xb = s * (llen / 2 + 8), s * (w1 - 20)
        art.append(strokes([[(xa, top), (xb - s * 10, top), (xb - s * 6, top + 3), (xb, top + 3)],
                            [(xa + s * 22, top), (xa + s * 26, top - 3), (xa + s * 40, top - 3)]], TRACE_W,
                           [(xa, top, 2.6), (xb, top + 3, 2.6), (xa + s * 40, top - 3, 2.2)]))
    chin = (v0 - FRONT + oy0 + 1.2) / 2
    unit, _ = text("CONTAINMENT UNIT", 3.6, 0.8)
    art.append(on_front(unit, 0, chin))
    grooves = cslab(CrossSection.batch_boolean(art, OpType.Add), zt - 0.01, zt + INLAY)
    inlay = grooves ^ body
    return body - inlay, inlay


SL = dict(stem=3.0, flange=(7.0, 8.0, 1.2), col=(4.0, 4.4), gap=2.0, prong=1.2, prong_w=2.4, knob=1.2)


def slider_part(g):
    """Moves the board's slide switch: a fork over its lever, a column up to a
    flange under the back plate, a stem out through the slot."""
    x, (ya, yb) = SWITCH["x"], SWITCH["y"]
    y = yb                                    # modelled in the lever's +y position
    zbi, zb = g["z_back_in"], g["z_back"]
    fx, fy, ft = SL["flange"]
    z_pr0, z_pr1 = SWITCH["body_top"] + 0.4, SWITCH["top"] + 0.25
    s = box(x - SL["stem"] / 2, x + SL["stem"] / 2, y - SL["stem"] / 2, y + SL["stem"] / 2, zbi - 0.3, zb + SL["knob"])
    s = s + box(x - fx / 2, x + fx / 2, y - fy / 2, y + fy / 2, zbi - 0.2 - ft, zbi - 0.2)
    cx, cy = SL["col"]
    s = s + box(x - cx / 2, x + cx / 2, y - cy / 2, y + cy / 2, z_pr1, zbi - 0.2 - ft + 0.01)
    half = SL["gap"] / 2
    for k in (-1, 1):
        y0 = y + k * half
        s = s + box(x - SL["prong_w"] / 2, x + SL["prong_w"] / 2, min(y0, y0 + k * SL["prong"]),
                    max(y0, y0 + k * SL["prong"]), z_pr0, z_pr1 + 0.01)
    return s


def back_part(g):
    """-> (back, inlay)"""
    ox0, ox1, oy0, oy1 = g["out"]
    ix0, ix1, iy0, iy1 = g["in"]
    zb, zbi = g["z_back"], g["z_back_in"]
    body = Manifold.batch_hull([cslab(crect(ox0, ox1, oy0, oy1, 9.0), Z_SPLIT, zb - 1.2),
                                cslab(crect(ox0 + 1.2, ox1 - 1.2, oy0 + 1.2, oy1 - 1.2, 8.4), zb - 0.01, zb)])
    body = body - box(ix0, ix1, iy0, iy1, Z_SPLIT - 1, zbi)
    inside = box(ix0, ix1, iy0, iy1, 0, zbi + 0.2)
    add, cut = [], []
    # Columns down onto the board's corner holes: M3 screws through them and
    # the board into the front's standoffs.
    seat = -1.7 - 5.0 + SCREW_LEN           # screw head seat: the tip ends 5 mm into the standoff
    for x, y in HOLES:
        add.append((cyl(x, y, 0.05, 2.5, 6.0) + cyl(x, y, 2.5, zbi + 0.1, 9.5)) ^ inside)
        cut += [cyl(x, y, -1, zb + 1, 3.4), cyl(x, y, seat, zb + 1, 6.4)]
    # Battery bay: a fence on the back plate (the battery is held against the
    # plate, 2.3 mm clear of the board), gaps for its lead.
    bx, by = BATTERY_AT
    f0, f1 = bx - BATTERY[0] / 2 - 0.6, bx + BATTERY[0] / 2 + 0.6
    f2, f3 = by - BATTERY[1] / 2 - 0.6, by + BATTERY[1] / 2 + 0.6
    fence = box(f0 - 1.6, f1 + 1.6, f2 - 1.6, f3 + 1.6, zbi - 5.0, zbi + 0.1) - box(f0, f1, f2, f3, zbi - 6, zbi + 1)
    fence = fence - box(f1 - 1, f1 + 3, by - 6, by + 6, zbi - 6, zbi + 1) - box(bx - 6, bx + 6, f3 - 1, f3 + 3, zbi - 6, zbi + 1)
    add.append(fence)
    # Speakers: frames to glue them in, grilles over them.
    for sx, sy in SPEAKERS_AT:
        a0, a1 = sx - SPEAKER[0] / 2 - 0.3, sx + SPEAKER[0] / 2 + 0.3
        b0, b1 = sy - SPEAKER[1] / 2 - 0.3, sy + SPEAKER[1] / 2 + 0.3
        add.append(box(a0 - 1.2, a1 + 1.2, b0 - 1.2, b1 + 1.2, zbi - 2.5, zbi + 0.1) - box(a0, a1, b0, b1, zbi - 3, zbi + 1)
                   - box(a0 - 2, a0 + 1, sy - 3, sy + 3, zbi - 3, zbi + 1)
                   - box(a1 - 1, a1 + 2, sy - 3, sy + 3, zbi - 3, zbi + 1))
        cut.append(cslab(radar_grille(SPEAKER[0] - 4.0, SPEAKER[1] - 4.0).translate((sx, sy)), zbi - 1, zb + 1))
    # Power switch: slot for the slider's stem, rails either side of its flange.
    x, (ya, yb) = SWITCH["x"], SWITCH["y"]
    st, (fx, fy, ft) = SL["stem"], SL["flange"]
    cut.append(box(x - st / 2 - 0.3, x + st / 2 + 0.3, ya - st / 2 - 0.3, yb + st / 2 + 0.3, zbi - 1, zb + 1))
    for k in (-1, 1):
        r0 = x + k * (fx / 2 + 0.3)
        add.append(box(min(r0, r0 + k * 1.4), max(r0, r0 + k * 1.4), ya - fy / 2 - 1.5, yb + fy / 2 + 1.5,
                       zbi - ft - 0.6, zbi + 0.1) ^ inside)
    # BOOT / RESET: pin holes with guide tubes down to the buttons.
    for bxx, byy in BUTTONS:
        add.append(cyl(bxx, byy, 5.6, zbi + 0.1, 4.6))
        cut.append(cyl(bxx, byy, 5.0, zb + 1, 2.3))
    # PWR / CHG LEDs: light pipes (open tubes) down to just above them.
    for lx, ly in LEDS:
        add.append(cyl(lx, ly, 1.4, zbi + 0.1, 4.0) ^ inside)
        cut.append(cyl(lx, ly, 1.0, zb + 1, 2.2))
    # Leg bosses: M3 self-tapping, from the legs outside.
    for lx in LEG_X:
        for dx, ly in LEG_SCREWS:
            add.append(cyl(lx + dx, ly, zbi - 7.0, zbi + 0.1, 7.0))
            cut.append(cyl(lx + dx, ly, zbi - 6.6, zb + 1, PILOT))
    body = Manifold.batch_boolean([body] + add, OpType.Add)
    body = Manifold.batch_boolean([body] + cut, OpType.Subtract)
    body = body - usb_openings(g, Z_SPLIT - 1, 20) - tf_opening(g, Z_SPLIT - 1, 20)
    # Markings (inlay): LAB-7 and ROGUE.AI CONTAINED in the middle, hazard
    # stripes along the bottom (clear of the legs).
    art = []
    lab, _ = text("LAB-7", 16.0, 2.4)
    art.append(on_back(lab, 0.0, 10.0))
    sub, _ = text("ROGUE.AI // CONTAINED", 4.0, 0.9)
    art.append(on_back(sub, 0.0, -6.0))
    art.append(hazard(-80, 80, oy0 + 4.0, oy0 + 10.0, 5.0))
    grooves = cslab(CrossSection.batch_boolean(art, OpType.Add), zb - INLAY, zb + 0.01)
    inlay = grooves ^ body
    return body - inlay, inlay


def leg_part(g, lx=0.0):
    """Kickstand leg at x = lx: a flange screwed to the back, a fin out to a
    foot on the table. The case stands on its bottom edge, leaning back TILT."""
    zb = g["z_back"]
    oy0 = g["out"][2]
    w, fl, fin = LEG["width"], LEG["flange"], LEG["fin"]
    t = np.radians(TILT)
    d = np.array([np.sin(t), np.cos(t)])         # (y, z): the table, from the bottom-back edge
    corner = np.array([oy0, zb])
    tip = corner + d * LEG["reach"]
    heel = corner + d * (LEG["reach"] - LEG["foot"])
    nrm = np.array([d[1], -d[0]])               # off the table, into the leg
    flange = box(lx - w / 2, lx + w / 2, LEG["y0"], LEG["y1"], zb, zb + fl)
    pts = [(LEG["y0"], zb + 0.01), (LEG["y1"], zb + 0.01), tuple(tip + nrm * 5.0), tuple(tip), tuple(heel)]
    prof = CrossSection([[(p[1], p[0]) for p in pts]], FillRule.EvenOdd)   # drawn in (z, y)
    fin_m = Manifold.extrude(prof, fin).rotate((0, -90, 0)).translate((lx + fin / 2, 0, 0))
    leg = flange + fin_m
    for dx, ly in LEG_SCREWS:
        leg = leg - cyl(lx + dx, ly, zb - 1, zb + fl + 1, 3.4)
    return leg


def check(g, parts):
    """Volumes where a part runs into the board (from the STEP's parts)."""
    board = [box(*PCB, -1.7, 0), box(-117.5, 117.5, -71.5, 71.5, -8.5, -1.7)]
    comps = [(-124.5, -116.92, 6.0, 14.94, -1.0, 3.16), (-124.5, -116.92, -14.94, -6.0, -1.0, 3.16),
             (-121.5, -115.8, 25.94, 38.44, 0, 7.0), (-109.7, -104.8, -71.5, -53.22, 0, 6.0),
             (-94.2, -89.3, -71.5, -53.22, 0, 6.0), (99.5, 111.5, -71.51, -65.3, 0, 8.1),
             (77.5, 89.5, -71.51, -65.3, 0, 8.1), (96.05, 112.2, 55.8, 71.0, 0, 2.05),
             (106.9, 123.5, -53.5, -40.3, 0, 2.41), (-111.5, -103.49, 62.91, 71.41, 0, 5.48),
             (83.75, 91.76, 62.91, 71.41, 0, 5.48), (-95.5, -87.49, 62.91, 71.41, 0, 5.48),
             (-120.5, -116.95, -31.94, -22.94, 0, 3.0), (117.0, 121.5, 52.75, 59.75, 0, 4.5),
             (117.0, 121.5, 38.25, 45.25, 0, 4.5), (-25.6, 25.6, 65.7, 72.3, 0, 3.5),
             (-9.3, 9.3, 18.6, 24.0, 0, 3.1), (29.7, 36.3, -57.9, -52.5, 0, 3.0), (-7.8, 7.8, -67.86, -62.06, 0, 1.96),
             (117.78, 122.5, -63.5, -59.74, 0, 1.26), (-122.5, -117.78, -63.5, -59.74, 0, 1.26)]
    solid = Manifold.batch_boolean(board + [box(*c) for c in comps], OpType.Add)
    for name, m in parts.items():
        v = (m ^ solid).volume()
        print(f"  {name}: {v:.2f} mm3 into the board")


def build():
    g = geometry()
    f, fi = front_part(g)
    b, bi = back_part(g)
    s = slider_part(g)
    return g, f, fi, b, bi, s


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    g, f, fi, b, bi, s = build()
    leg = leg_part(g)
    check(g, {"front": f, "back": b, "slider (lever excluded)": s})
    parts = {"front": (f, False, None), "front_inlay": (fi, False, f), "back": (b, True, None),
             "back_inlay": (bi, True, b), "slider": (s, False, None),
             "leg": (leg, False, None)}   # flange down
    for part, (m, flip, ref) in parts.items():
        m = to_bed(m, flip, ref)
        write_stl(m, os.path.join(here, f"elecrow10_{part}.stl"), f"LAB-7 Elecrow 10.1 {part}")
        lo, hi = m.bounding_box()[:3], m.bounding_box()[3:]
        print(f"elecrow10_{part}: {hi[0]-lo[0]:.1f} x {hi[1]-lo[1]:.1f} x {hi[2]-lo[2]:.1f} mm, "
              f"{m.volume()/1000:.2f} cm3")
    ox0, ox1, oy0, oy1 = g["out"]
    print(f"case {ox1-ox0:.1f} x {oy1-oy0:.1f} x {g['z_back']-g['z_top']:.1f} mm; board screws M3 x {SCREW_LEN:.0f}")
