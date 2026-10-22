#!/usr/bin/env python3
"""ARM-7 armband mount for the Sunton ESP32-2432S024C (2.4" CYD) + a 2000 mAh
LiPo, worn on the forearm. Two printed parts:

  armband_bezel.stl  the front: frames the screen, holds the board on its four
                     M3 holes, windows for the RGB LED and the light sensor,
                     side cut-outs for USB-C and the SD card
  armband_pod.stl    the back: battery compartment, power-switch slot, a
                     saddle curved to the forearm, strap slots at both ends

Assembly: board screen-down into the bezel, 4x M3x6 screws through the board
into the bezel's posts. Battery (+ an inline slide switch on its red wire) in
the pod, plugged into the board's BAT connector. Pod onto the bezel, 4x M3x10
screws from underneath. Strap through the end slots.

Coordinates are the board's own (from esp32-2432s024c.stl): PCB 74.4 x 42.9,
its back at z=0, front at z=1.6, screen glass top at z=5.94; the arm runs
along x. Needs Python 3 with manifold3d + numpy (pip install manifold3d
numpy); writes the STLs next to this file.

  python3 armband_mount.py
"""
import os
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold, OpType

# ---- Your settings ----------------------------------------------------------------
ARM_CIRCUMFERENCE = 330.0   # mm, where it sits, over the jacket (13")
STRAP_WIDTH = 25.0          # mm (hook-and-loop or elastic); 38 also fits
BATTERY = (58.4, 38.1, 8.9)  # mm, L x W x thickness (2000 mAh, 2.3" x 1.5" x 0.35")
# The power switch (3-pin mini slide switch): body length x thickness x height
# (without pins), how far the lever sticks out, lever width, lever travel.
SWITCH = dict(length=11.55, thick=4.0, height=6.0, lever_out=1.84, lever_w=2.2, travel=3.2)
FIT = 0.4                   # mm clearance around the board
SCREW_PILOT = 2.6           # mm, M3 self-tapping into PETG/PLA (4.0-4.2 for heat-set inserts)

# ---- The board (measured from esp32-2432s024c.stl) --------------------------------
PCB_X, PCB_Y = 37.2, 21.45           # half sizes
HOLES = [(sx * 34.0, sy * 18.25) for sx in (-1, 1) for sy in (-1, 1)]
SCREEN = (-30.25, 30.91, 21.5, 5.94)  # x from/to, half height, glass top z
VIEW = (0.33, 0.0, 52.0, 39.5)        # window: centre x/y, size (active area is 48.96 x 36.72)
LED = (-33.45, -12.45, 5.6)           # RGB LED1 (5x5 mm): x, y, window diameter
LDR = (-34.25, 12.55, 3.0)            # light sensor (R21)
USB = (-2.53, 13.0, 1.0)              # USB-C on the +x edge: centre y, opening width, top z
SD = (2.08, 13.0, -0.5)               # SD slot through the -y edge: centre x, width, top z
BACK_PARTS = 4.75                     # deepest part on the back, below z=0

# ---- Derived -----------------------------------------------------------------------
WALL = 2.4
FRONT = 1.8                           # front plate
IN_X, IN_Y = PCB_X + FIT, SCREEN[2] + FIT
OUT_X, OUT_Y = IN_X + WALL, IN_Y + WALL
Z_FRONT = SCREEN[3] + 0.2             # plate underside, just clear of the glass
Z_TOP = Z_FRONT + FRONT
Z_SPLIT = -(BACK_PARTS + 0.85)        # bezel / pod joint
EAR_X0, EAR_X1, EAR_Y, EAR_W = OUT_X, OUT_X + 8.0, 15.0, 9.0
END_X = EAR_X1 + 10.0                 # pod ends (strap bars)
SLOT_X0 = EAR_X1 + 2.5                # strap slot
SLOT_T = 3.2
ARM_R = ARM_CIRCUMFERENCE / (2 * np.pi)
Z_FLOOR = Z_SPLIT - (BATTERY[2] + 1.3)  # cavity floor
Z_SADDLE = Z_FLOOR - 2.0                # underside at the centre line
ARM_CZ = Z_SADDLE - ARM_R               # arm's axis


def rrect(w, h, r, segs=48):
    return CrossSection.square((w - 2 * r, h - 2 * r), center=True).offset(r, JoinType.Round, circular_segments=segs)


def crect(w, h, c, r=0.8):
    """Rectangle with 45-degree corners (the visor's angular look), edges softened by r."""
    x, y = w / 2, h / 2
    pts = [(-x + c, -y), (x - c, -y), (x, -y + c), (x, y - c), (x - c, y), (-x + c, y), (-x, y - c), (-x, -y + c)]
    return CrossSection([pts]).offset(-r, JoinType.Round).offset(r, JoinType.Round, circular_segments=24)


def slab(w, h, r, z0, z1, x=0.0, y=0.0):
    return Manifold.extrude(rrect(w, h, r), z1 - z0).translate((x, y, z0))


def cslab(cs, z0, z1):
    return Manifold.extrude(cs, z1 - z0).translate((0, 0, z0))


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


def cyl(x, y, z0, z1, d, segs=48):
    return Manifold.cylinder(z1 - z0, d / 2, d / 2, segs).translate((x, y, z0))


def strokes(lines, width, pads=()):
    """2D: polylines drawn with a round pen of this width, plus round pads (x, y, d)."""
    parts = []
    for line in lines:
        for (x0, y0), (x1, y1) in zip(line, line[1:]):
            parts.append(CrossSection.batch_hull([CrossSection.circle(width / 2, 16).translate((x0, y0)),
                                                  CrossSection.circle(width / 2, 16).translate((x1, y1))]))
    for x, y, d in pads:
        parts.append(CrossSection.circle(d / 2, 24).translate((x, y)))
    return CrossSection.batch_boolean(parts, OpType.Add)


# Block letters for the label, on a 4 x 6 grid.
GLYPHS = {
    "A": [[(0, 0), (0, 4.5), (1.5, 6), (2.5, 6), (4, 4.5), (4, 0)], [(0, 3), (4, 3)]],
    "R": [[(0, 0), (0, 6), (3, 6), (4, 5), (4, 4), (3, 3), (0, 3)], [(2, 3), (4, 0)]],
    "M": [[(0, 0), (0, 6), (2, 3.5), (4, 6), (4, 0)]],
    "-": [[(0.8, 3), (3.2, 3)]],
    "7": [[(0, 6), (4, 6), (1.5, 0)]],
}


def text(s, height, width):
    k = height / 6
    lines = []
    for i, ch in enumerate(s):
        for line in GLYPHS[ch]:
            lines.append([((x + i * 5.6) * k, y * k) for x, y in line])
    return strokes(lines, width), (len(s) * 5.6 - 1.6) * k


def on_side(cs, sign, depth):
    """A 2D shape drawn as (x, z) -> a groove/inlay `depth` deep into the pod's
    long side at y = sign * OUT_Y."""
    m = Manifold.extrude(cs, depth + 0.02).rotate((90, 0, 0))  # (x, y) -> (x, 0, y), depth toward -y
    return m.translate((0, OUT_Y + 0.01, 0)) if sign > 0 else m.rotate((0, 0, 180)).translate((0, -OUT_Y - 0.01, 0)).mirror((1, 0, 0))


# ---- Accents: green circuit traces like the costume's light channels -----------------
TRACE_W = 1.6
FRONT_TRACES = (  # on the bezel's front, at the ends of the screen window (x, y)
    [[(33.6, -17.0), (33.6, 6.0), (36.0, 8.4), (36.0, 15.5)],
     [(30.4, -19.5), (32.0, -17.9)], [(30.4, -16.3), (32.0, -14.7)], [(30.4, -13.1), (32.0, -11.5)],
     [(-33.45, -7.6), (-33.45, 3.0), (-36.0, 5.5), (-36.0, 8.0)]],
    [(33.6, -17.0, 3.0), (36.0, 15.5, 3.0), (-36.0, 8.0, 2.6)],
)
INLAY_DEPTH = 0.6  # 3 layers at 0.2 mm
SIDE_TRACE = ([[(-15, -9.5), (-12, -9.5), (-8, -13.5), (14, -13.5), (18, -9.5), (34, -9.5)]],
              [(-15, -9.5, 3.0), (34, -9.5, 3.0)])
ENGRAVE = 0.8


def bezel_parts():
    """-> (bezel, green inlay)"""
    body = Manifold.batch_hull([cslab(crect(2 * OUT_X, 2 * OUT_Y, 6.0), Z_SPLIT, Z_TOP - 1.2),
                                cslab(crect(2 * OUT_X - 2.4, 2 * OUT_Y - 2.4, 5.4), Z_TOP - 0.01, Z_TOP)])
    # Flanges for the pod's screws, sloping down from the body to the ends.
    fy = EAR_Y + EAR_W / 2
    flange = Manifold.batch_hull([cslab(crect(2 * EAR_X1, 2 * fy, 5.0), Z_SPLIT, Z_SPLIT + 3.5),
                                  # full width where it meets the body: no crevice at its angled corners
                                  cslab(crect(2 * OUT_X + 2.0, 2 * OUT_Y, 7.0), Z_SPLIT, Z_SPLIT + 8.5)])
    body = body + flange
    for sx in (-1, 1):
        for sy in (-1, 1):
            body = body - cyl(sx * (EAR_X0 + 4.0), sy * EAR_Y, Z_SPLIT - 1, Z_SPLIT + 5.0, SCREW_PILOT)
    # Inside: room for the board and the screen.
    body = body - box(-IN_X, IN_X, -IN_Y, IN_Y, Z_SPLIT - 1, Z_FRONT)
    # Screen window, chamfered outward.
    cx, cy, w, h = VIEW
    win = Manifold.batch_hull([slab(w, h, 1.0, Z_FRONT - 0.5, Z_FRONT + 0.3, cx, cy),
                               slab(w + 2 * FRONT, h + 2 * FRONT, 2.0, Z_TOP, Z_TOP + 0.5, cx, cy)])
    body = body - win
    body = body - cyl(LED[0], LED[1], Z_FRONT - 1, Z_TOP + 1, LED[2])
    body = body - cyl(LDR[0], LDR[1], Z_FRONT - 1, Z_TOP + 1, LDR[2])
    # Posts on the board's holes.
    for x, y in HOLES:
        body = body + cyl(x, y, 1.6, Z_FRONT + 0.1, 4.6)
        body = body - cyl(x, y, 1.6 - 0.1, 1.6 + 4.2, SCREW_PILOT)
    # USB-C (+x edge) and SD card (-y edge): open at the bottom, the pod closes them.
    uy, uw, uz = USB
    body = body - box(IN_X - 1, OUT_X + 3, uy - uw / 2, uy + uw / 2, Z_SPLIT - 1, uz)
    sx_, sw, sz = SD
    body = body - box(sx_ - sw / 2, sx_ + sw / 2, -OUT_Y - 1, -IN_Y + 1, Z_SPLIT - 1, sz)
    # Green inlay, flush with the front.
    inlay = cslab(strokes(FRONT_TRACES[0], TRACE_W, FRONT_TRACES[1]), Z_TOP - INLAY_DEPTH, Z_TOP + 0.01) ^ body
    return body - inlay, inlay


def jewel():
    """Clear 'status jewel' over the RGB LED: a light pipe down to the LED, a faceted hex cap."""
    x, y, d = LED
    pipe = cyl(x, y, 3.7, Z_TOP + 0.1, d - 0.25)  # LED top is at z 3.37
    cap = Manifold.batch_hull([Manifold.cylinder(0.8, 4.85, 4.85, 6).translate((x, y, Z_TOP)),
                               Manifold.cylinder(0.01, 2.9, 2.9, 6).translate((x, y, Z_TOP + 1.8))])
    return pipe + cap


def pod():
    zlow = ARM_CZ + np.sqrt(ARM_R ** 2 - OUT_Y ** 2) - 0.5
    # Outline: full width under the bezel, tapering to the strap ends.
    ew = STRAP_WIDTH / 2 + 4.5
    pts = [(-END_X, -ew), (-EAR_X1, -OUT_Y), (EAR_X1, -OUT_Y), (END_X, -ew),
           (END_X, ew), (EAR_X1, OUT_Y), (-EAR_X1, OUT_Y), (-END_X, ew)]
    outline = CrossSection([pts]).offset(-2, JoinType.Round).offset(2, JoinType.Round, circular_segments=24)
    body = cslab(outline, zlow, Z_SPLIT)
    # The saddle: the forearm's curve taken out of the underside.
    arm = Manifold.cylinder(4 * END_X, ARM_R, ARM_R, 256).rotate((0, 90, 0)).translate((-2 * END_X, 0, ARM_CZ))
    body = body - arm
    # Battery cavity, open to the board.
    body = body - box(-IN_X, IN_X, -IN_Y, IN_Y, Z_FLOOR, Z_SPLIT + 1)
    for sx in (-1, 1):
        # The ends are thinner (strap bars).
        x0, x1 = sorted((sx * EAR_X1, sx * (END_X + 1)))
        body = body - box(x0, x1, -OUT_Y - 1, OUT_Y + 1, Z_SPLIT - 3.0, Z_SPLIT + 1)
        # Strap slot.
        x0, x1 = sorted((sx * SLOT_X0, sx * (SLOT_X0 + SLOT_T)))
        body = body - slab(x1 - x0, STRAP_WIDTH + 1.5, 1.2, zlow - 1, Z_SPLIT + 1, (x0 + x1) / 2, 0)
        # Screws up into the bezel's flanges: through hole + counterbore from below.
        for sy in (-1, 1):
            x, y = sx * (EAR_X0 + 4.0), sy * EAR_Y
            body = body - cyl(x, y, zlow - 1, Z_SPLIT + 1, 3.4)
            body = body - cyl(x, y, zlow - 1, Z_SPLIT - 5.0, 6.2)
    # Power switch in the -y side wall near the -x end (the battery goes toward
    # the +x end; a low rib keeps it there). The body sits in a shallow pocket
    # on the inside of the wall, the lever pokes out through a slot into a
    # finger recess: its tip ends level with the outside, nothing to snag.
    sw = SWITCH
    zc = (Z_FLOOR + Z_SPLIT) / 2
    x = -IN_X + 0.4 + sw["length"] / 2 + 0.15
    pocket, recess = 0.6, 1.0
    wall_left = WALL - pocket - recess                   # wall between pocket and recess
    body = body - box(x - sw["length"] / 2 - 0.15, x + sw["length"] / 2 + 0.15, -IN_Y - pocket, -IN_Y + 1,
                      zc - sw["thick"] / 2 - 0.15, zc + sw["thick"] / 2 + 0.15)
    slot = sw["lever_w"] + sw["travel"] + 0.6
    body = body - box(x - slot / 2, x + slot / 2, -OUT_Y - 1, -IN_Y, zc - 1.3, zc + 1.3)
    nook = Manifold.extrude(rrect(slot + 8.0, 8.0, 2.5), recess + 0.01).rotate((90, 0, 0)).translate((x, -OUT_Y + recess, zc))
    body = body - nook
    rib_x = x + sw["length"] / 2 + 1.0
    body = body + box(rib_x, rib_x + 1.2, -IN_Y, IN_Y, Z_FLOOR - 0.1, Z_FLOOR + 3.0)
    # Engraved circuit trace down both long sides, and the ARM-7 label on +y.
    trace = strokes(SIDE_TRACE[0], TRACE_W, SIDE_TRACE[1])
    body = body - on_side(trace, 1, ENGRAVE) - on_side(trace, -1, ENGRAVE)
    label, length = text("ARM-7", 5.5, 1.1)
    # Seen from +y, x runs right-to-left: mirror the text so it reads from outside.
    body = body - on_side(label.mirror((1, 0)).translate((length / 2 - 4, -21.5)), 1, ENGRAVE)
    return body


def write_stl(man, path):
    mesh = man.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    f = np.asarray(mesh.tri_verts)
    with open(path, "wb") as fh:
        fh.write(b"ARM-7 armband mount".ljust(80, b" "))
        fh.write(struct.pack("<I", len(f)))
        for a, b, c in f:
            n = np.cross(v[b] - v[a], v[c] - v[a])
            n = n / (np.linalg.norm(n) or 1)
            fh.write(struct.pack("<12fH", *n, *v[a], *v[b], *v[c], 0))


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    b, inlay = bezel_parts()
    parts = {"armband_bezel": b, "armband_bezel_inlay": inlay, "armband_jewel": jewel(), "armband_pod": pod()}
    for name, m in parts.items():
        write_stl(m, os.path.join(here, name + ".stl"))
        lo, hi = m.bounding_box()[:3], m.bounding_box()[3:]
        print(f"{name}: {hi[0]-lo[0]:.1f} x {hi[1]-lo[1]:.1f} x {hi[2]-lo[2]:.1f} mm, {m.volume()/1000:.2f} cm3")
    print(f"arm radius {ARM_R:.1f} mm, total height {Z_TOP - (ARM_CZ + np.sqrt(ARM_R**2 - OUT_Y**2)):.1f} mm at the edges, "
          f"{Z_TOP - Z_SADDLE:.1f} mm at the centre line")
