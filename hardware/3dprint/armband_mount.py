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
# (without pins), how far the lever sticks out, lever size (square), lever
# travel (the switch's own lever opening, 5.23 mm, minus the lever), centred.
SWITCH = dict(length=11.55, thick=4.0, height=6.0, lever_out=1.84, lever_w=1.95, travel=5.23 - 1.95)
SWITCH_FIT = 0.45   # mm a side around the switch body (0.15 printed ~0.55 mm too tight in PETG)
LEVER_FIT = 0.55    # mm a side around the lever's slot
FIT = 0.4                   # mm clearance around the board
SCREW_PILOT = 2.6           # mm, the pod's M3 screws into the bezel's flanges
BOARD_SCREW_PILOT = 2.8     # mm, the board's M3 x 6 screws into the bezel's posts (prints ~2.6-2.7: easy to drive)

# ---- The board (measured from esp32-2432s024c.stl) --------------------------------
PCB_X, PCB_Y = 37.2, 21.45           # half sizes
HOLES = [(sx * 34.0, sy * 18.25) for sx in (-1, 1) for sy in (-1, 1)]
SCREEN = (-30.25, 30.91, 21.5, 5.94)  # x from/to, half height, glass top z
# Window = the screen's viewable area (measured on the real glass, 59.45 x 43.05,
# taken as centred on the board like in the STL: 2.58 mm border on the LED end,
# top and bottom, 6.31 mm on the other end), plus 0.3 mm a side.
GLASS = (59.45, 43.05)
VIEW = ((-GLASS[0] / 2 + 2.58 + GLASS[0] / 2 - 6.31) / 2, 0.0, GLASS[0] - 2.58 - 6.31 + 0.6, GLASS[1] - 2 * 2.58 + 0.6)
LED = (-33.45, -12.45, 5.6)           # RGB LED1 (5x5 mm): x, y, window diameter
LDR = (-34.25, 12.55, 3.0)            # light sensor (R21)
USB = (-2.53, 13.0, 2.5)              # USB-C on the +x edge: centre y, opening width, top z (plug overmold room)
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
SIDE_TRACE = ([[(-15, -8.8), (-12, -8.8), (-8, -12.4), (14, -12.4), (18, -8.8), (34, -8.8)]],
              [(-15, -8.8, 3.0), (34, -8.8, 3.0)])
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
    # Posts on the board's holes: 5.6 mm, flared where they meet the front (the
    # flare stays above the screen's edge layers and the LED).
    for x, y in HOLES:
        body = body + cyl(x, y, 1.6, Z_FRONT + 0.1, 5.6)
        body = body + Manifold.cylinder(2.0, 2.8, 3.5, 48).translate((x, y, Z_FRONT - 2.0 + 0.05))
        body = body - cyl(x, y, 1.6 - 0.1, 1.6 + 4.2, BOARD_SCREW_PILOT)
    # Screen window, chamfered outward.
    cx, cy, w, h = VIEW
    win = Manifold.batch_hull([slab(w, h, 1.0, Z_FRONT - 0.5, Z_FRONT + 0.3, cx, cy),
                               slab(w + 2 * FRONT, h + 2 * FRONT, 2.0, Z_TOP, Z_TOP + 0.5, cx, cy)])
    body = body - win
    body = body - cyl(LED[0], LED[1], Z_FRONT - 3, Z_TOP + 1, LED[2])
    body = body - cyl(LDR[0], LDR[1], Z_FRONT - 3, Z_TOP + 1, LDR[2])
    # USB-C (+x edge) and SD card (-y edge): open at the bottom, the pod closes them.
    uy, uw, uz = USB
    # ...right out through the end flange, so a cable plugs straight in.
    body = body - box(IN_X - 1, EAR_X1 + 2, uy - uw / 2, uy + uw / 2, Z_SPLIT - 1, uz)
    sx_, sw, sz = SD
    body = body - box(sx_ - sw / 2, sx_ + sw / 2, -OUT_Y - 1, -IN_Y + 1, Z_SPLIT - 1, sz)
    # Green inlay, flush with the front.
    inlay = cslab(strokes(FRONT_TRACES[0], TRACE_W, FRONT_TRACES[1]), Z_TOP - INLAY_DEPTH, Z_TOP + 0.01) ^ body
    return body - inlay, inlay


def jewel():
    """Clear 'status jewel' over the RGB LED: a light pipe down to the LED, a faceted hex cap."""
    x, y, d = LED
    pipe = cyl(x, y, 3.7, Z_TOP + 0.1, d - 0.25)  # LED top is at z 3.37
    cap = Manifold.batch_hull([Manifold.cylinder(0.8, 4.85, 4.85, 6).rotate((0, 0, 30)).translate((x, y, Z_TOP)),
                               Manifold.cylinder(0.01, 2.9, 2.9, 6).rotate((0, 0, 30)).translate((x, y, Z_TOP + 1.8))])
    return pipe + cap


def saddle_z(y):
    """Height of the forearm's surface under the pod at y."""
    return ARM_CZ + np.sqrt(ARM_R ** 2 - y ** 2)


# Strap tunnels: two straps go round the forearm, each under the pod through a
# pair of tunnels -- in through a long side's face, out through the underside
# near that edge. Centred at these x positions; the tunnel's line (y, z) runs
# from the underside (A) to the side face (B).
STRAP_X = (-25.0, 25.0)
TUNNEL = 3.6                           # mm, the strap's thickness + room
TUNNEL_A = (14.0, None)                # on the underside (z follows the arm)
TUNNEL_B_Z = -18.0                     # where it comes out of the side face
BATTERY_ZONE_Z = -12.0                 # below this the cavity is only as wide as the battery
SWITCH_LIP = 0.6                       # mm of wall in front of the switch's face


def tunnel(sign, xc):
    ay, by = TUNNEL_A[0], OUT_Y
    az, bz = saddle_z(ay), TUNNEL_B_Z
    d = np.array([by - ay, bz - az]); d /= np.linalg.norm(d)
    n = np.array([-d[1], d[0]]) * TUNNEL / 2
    p0 = np.array([ay, az]) - d * 6       # run on past both surfaces
    p1 = np.array([by, bz]) + d * 4
    pts = [tuple(p0 - n), tuple(p1 - n), tuple(p1 + n), tuple(p0 + n)]
    cs = CrossSection([[(y, z) for y, z in pts]])
    # (y, z) section, extruded along x
    m = Manifold.extrude(cs, STRAP_WIDTH + 1.5).rotate((90, 0, 90)).translate((xc - (STRAP_WIDTH + 1.5) / 2, 0, 0))
    return m if sign > 0 else m.mirror((0, 1, 0))


def pod_parts():
    """-> (pod, green inlay)"""
    zlow = saddle_z(OUT_Y) - 0.5
    outline = crect(2 * (EAR_X1 + 2), 2 * OUT_Y, 8.0)
    body = cslab(outline, zlow, Z_SPLIT)
    # The saddle: the forearm's curve taken out of the underside.
    span = 2 * (EAR_X1 + 10)
    arm = Manifold.cylinder(2 * span, ARM_R, ARM_R, 256).rotate((0, 90, 0)).translate((-span, 0, ARM_CZ))
    body = body - arm
    # Cavity: full width up top (wires, the board's back), battery-wide below.
    bw = BATTERY[1] / 2 + 1.25
    body = body - box(-IN_X, IN_X, -IN_Y, IN_Y, BATTERY_ZONE_Z, Z_SPLIT + 1)
    body = body - box(-IN_X, IN_X, -bw, bw, Z_FLOOR, BATTERY_ZONE_Z + 0.01)
    for sx in (-1, 1):
        # Screws up into the bezel's flanges: through hole + counterbore from below.
        for sy in (-1, 1):
            x, y = sx * (EAR_X0 + 4.0), sy * EAR_Y
            body = body - cyl(x, y, zlow - 1, Z_SPLIT + 1, 3.4)
            body = body - cyl(x, y, zlow - 1, Z_SPLIT - 5.0, 6.2)
    # Strap tunnels.
    for xc in STRAP_X:
        for sy in (-1, 1):
            body = body - tunnel(sy, xc)
    # A notch under the USB-C opening (in the bezel's +x flange) for big plugs.
    uy, uw, _ = USB
    body = body - box(IN_X - 1, EAR_X1 + 2, uy - uw / 2, uy + uw / 2, Z_SPLIT - 1.5, Z_SPLIT + 1)
    # Power switch in the -y side wall near the -x end (the battery goes toward
    # the +x end; a low rib keeps it there). Its face rests on a thin lip at the
    # outside, so the lever sticks out ~1.2 mm (the switch cap adds ~3 mm).
    sw = SWITCH
    zc = (Z_FLOOR + Z_SPLIT) / 2
    x = -IN_X + 0.4 + sw["length"] / 2 + SWITCH_FIT
    x0, x1 = x - sw["length"] / 2 - SWITCH_FIT, x + sw["length"] / 2 + SWITCH_FIT
    zb, zt = zc - sw["thick"] / 2 - SWITCH_FIT, zc + sw["thick"] / 2 + SWITCH_FIT
    face = -OUT_Y + SWITCH_LIP                            # the switch's face
    yd = face + sw["height"]                              # its back (pins beyond)
    body = body - box(x0, x1, face, yd + 0.5, zb, zt + 2)  # its pocket
    slot = sw["lever_w"] + sw["travel"] + 2 * LEVER_FIT
    body = body - box(x - slot / 2, x + slot / 2, -OUT_Y - 1, face + 0.01,
                      zc - sw["lever_w"] / 2 - LEVER_FIT, zc + sw["lever_w"] / 2 + LEVER_FIT)
    rib_x = x1 + 1.0
    body = body + box(rib_x, rib_x + 1.2, -bw, bw, Z_FLOOR - 0.1, Z_FLOOR + 3.0)
    # Cradle: the switch drops in from the open side (pins toward the middle),
    # onto a ledge, between the end wall and a stop, and clicks under two small
    # catches. Flipping the lever pushes it sideways into the stops; a dab of
    # glue is extra.
    yi = yd - 0.6
    body = body + box(x0, x1, face, yi, Z_FLOOR - 0.1, zb)                       # ledge
    body = body + box(-IN_X - 0.1, x0, face, yi, Z_FLOOR - 0.1, zt + 0.8)          # end stop
    body = body + box(x1, x1 + 1.6, face, yi, Z_FLOOR - 0.1, zt + 0.8)             # stop
    for xa, xb in ((x0 - 0.01, x0 + 0.3), (x1 - 0.3, x1 + 0.01)):                 # catches over the body
        catch = Manifold.batch_hull([box(xa, xb, face, yi, zt + 0.1, zt + 0.4),
                                     box(xa if xa < x else xb - 0.01, xb if xa < x else xb, face, yi,
                                         zt + 0.79, zt + 0.8)])
        body = body + catch
    # Circuit trace down both long sides and the ARM-7 label on +y (between the
    # strap tunnels): grooves, filled flush by a green inlay (printed on its
    # side, the colour change is only in the first and last few layers). Seen
    # from +y, x runs right-to-left: the text is mirrored so it reads from outside.
    label, length = text("ARM-7", 4.5, 1.0)
    trace = strokes(SIDE_TRACE[0], TRACE_W, SIDE_TRACE[1])
    grooves = Manifold.batch_boolean([on_side(trace, 1, ENGRAVE), on_side(trace, -1, ENGRAVE),
                                      on_side(label.mirror((1, 0)).translate((length / 2, -21.2)), 1, ENGRAVE)],
                                     OpType.Add)
    inlay = grooves ^ body
    return body - inlay, inlay


def switch_cap():
    """A cap glued onto the switch's lever: something to push with."""
    sw = SWITCH
    w = sw["lever_w"] + 0.2                               # socket
    cap = Manifold.batch_hull([box(-2.2, 2.2, -1.9, 1.9, 0, 2.0), box(-1.8, 1.8, -1.5, 1.5, 2.0, 3.0)])
    cap = cap - box(-w / 2, w / 2, -w / 2, w / 2, -0.1, 1.0)
    for gx in (-1.2, 0.0, 1.2):                           # grip ridges across the push direction
        cap = cap - box(gx - 0.25, gx + 0.25, -2, 2, 2.6, 3.1)
    return cap


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
    p, p_inlay = pod_parts()
    parts = {"armband_bezel": b, "armband_bezel_inlay": inlay, "armband_jewel": jewel(), "armband_pod": p,
             "armband_pod_inlay": p_inlay, "armband_switch_cap": switch_cap()}
    for name, m in parts.items():
        write_stl(m, os.path.join(here, name + ".stl"))
        lo, hi = m.bounding_box()[:3], m.bounding_box()[3:]
        print(f"{name}: {hi[0]-lo[0]:.1f} x {hi[1]-lo[1]:.1f} x {hi[2]-lo[2]:.1f} mm, {m.volume()/1000:.2f} cm3")
    print(f"arm radius {ARM_R:.1f} mm, total height {Z_TOP - saddle_z(OUT_Y):.1f} mm at the edges, "
          f"{Z_TOP - Z_SADDLE:.1f} mm at the centre line")
