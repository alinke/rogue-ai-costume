#!/usr/bin/env python3
"""LAB-7 field scanner cases for the Elecrow CrowPanel Advance 3.5" and 4.3"
boards (scanner/ envs advance35 / advance43), held upright (portrait), each
with a 2000 mAh LiPo (59 x 37 x 7.1 mm) behind the board. Per board:

  scanner35_front.stl / scanner43_front.stl      the bezel: frames the screen,
                       the board's front rests on a ledge all round the panel
  scanner35_back.stl / scanner43_back.stl        the back shell: battery bay,
                       speaker grille (radar rings), power-switch slot,
                       lanyard tab, USB-C opening at the bottom
  scanner35_front_inlay.stl, scanner35_back_inlay.stl (and 43)
                       the LAB-7 markings, flush in the faces: print them in a
                       second colour (amber looks right), or leave them out and
                       the grooves stay as engraving
The power switch is the armband's (armband_switch_cap.stl fits it too).

Assembly: board screen-down into the front. Battery (+ an inline slide switch
on its red wire, the switch clicked into its cradle) and speaker into the back,
plug both into the board (battery: the 2-pin PH2.0 BAT socket; speaker: the
2-pin SPK socket). Back onto the board, 4x M3 x 16 screws from the back
through the shell's columns and the board's corner holes into the front.
On the 4.3": set the DIP switch on the back of the board to MIC & SPK first.

Coordinates are the board's own, from Elecrow's STEP models
(github.com/Elecrow-RD/CrowPanel-Advance-<size>-HMI-..., "3D file"): the PCB
from z -1.6 (front) to 0 (back), the panel in front of it (toward -z), parts
on the back up to z 8. Held upright, the USB-C end is the bottom (+x).
Needs Python 3 with manifold3d + numpy:

  python3 scanner_case.py
"""
import os
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold, OpType

BATTERY = (59.0, 37.0, 7.1)  # mm (2000 mAh)
SPEAKER = (30.0, 20.0, 7.5)  # mm, rectangular speaker: length, width, depth (glued into its frame)
SWITCH = dict(length=11.55, thick=4.0, height=6.0, lever_out=1.84, lever_w=1.95, travel=5.23 - 1.95)
SWITCH_FIT, LEVER_FIT = 0.45, 0.55
FIT = 0.4            # around the board and the panel
WALL = 2.4
FRONT = 1.8          # front plate
BACK = 2.0           # back plate
OVERLAP = 0.3        # how far the bezel reaches over the active area (hides the board's play in the case)
PILOT = 2.6          # M3 screws into the front
SCREW_LEN = 16.0
Z_SPLIT = 4.0        # front / back joint (above the USB-C sockets, z -1..3.2)

BOARDS = {
    "35": dict(  # CrowPanel Advance 3.5" (PCB V1.2-V1.4)
        pcb=(0.0, 96.8, 0.0, 58.7), holes=[(3.0, 3.0), (3.0, 55.7), (93.8, 3.0), (93.8, 55.7)],
        panel=(6.93, 89.87, 2.02, 56.68, -7.1),
        usb=(12.6, 36.9),        # both USB-C sockets (y 14.5-23.4, 26-35) on the +x edge: one opening
        button=24.4,             # a side push button on the -y edge (x), sticking 0.5 mm out
        battery=(30.5, 28.5, False), speaker=(78.0, 29.35, False), switch_x=14.5, under_battery=3.5,
        # The glass's black border round the active area, measured, upright
        # (USB at the bottom), screen toward you: top, bottom, left, right.
        border=(1.85, 6.2, 2.4, 2.7),
    ),
    "43": dict(  # CrowPanel Advance 4.3" (PCB V1.3)
        pcb=(0.43, 117.93, 0.17, 71.37), holes=[(3.43, 3.17), (3.43, 68.37), (114.93, 3.17), (114.93, 68.37)],
        panel=(6.2, 111.7, 2.3, 69.5, -7.2),
        usb=(29.5, 42.0),        # USB-C (y 31.3-40.2) on the +x edge
        button=44.4,
        battery=(78.5, 40.5, True), speaker=(37.0, 35.77, False), switch_x=36.0, under_battery=3.5,
        border=(4.91, 4.13, 4.16, 9.29),
    ),
}


# ---- 2D/3D helpers (as in armband_mount.py) --------------------------------------
def crect(x0, x1, y0, y1, c, r=0.8):
    """Rectangle with 45-degree corners (LAB-7's angular look), edges softened by r."""
    pts = [(x0 + c, y0), (x1 - c, y0), (x1, y0 + c), (x1, y1 - c), (x1 - c, y1), (x0 + c, y1), (x0, y1 - c), (x0, y0 + c)]
    return CrossSection([pts]).offset(-r, JoinType.Round).offset(r, JoinType.Round, circular_segments=24)


def rrect(x0, x1, y0, y1, r):
    return CrossSection.square((x1 - x0 - 2 * r, y1 - y0 - 2 * r)).translate((x0 + r, y0 + r)).offset(
        r, JoinType.Round, circular_segments=32)


def cslab(cs, z0, z1):
    return Manifold.extrude(cs, z1 - z0).translate((0, 0, z0))


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


def cyl(x, y, z0, z1, d, segs=48):
    return Manifold.cylinder(z1 - z0, d / 2, d / 2, segs).translate((x, y, z0))


def strokes(lines, width, pads=()):
    parts = []
    for line in lines:
        for (x0, y0), (x1, y1) in zip(line, line[1:]):
            parts.append(CrossSection.batch_hull([CrossSection.circle(width / 2, 16).translate((x0, y0)),
                                                  CrossSection.circle(width / 2, 16).translate((x1, y1))]))
    for x, y, d in pads:
        parts.append(CrossSection.circle(d / 2, 24).translate((x, y)))
    return CrossSection.batch_boolean(parts, OpType.Add)


# Block letters on a 4 x 6 grid.
GLYPHS = {
    "A": [[(0, 0), (0, 4.5), (1.5, 6), (2.5, 6), (4, 4.5), (4, 0)], [(0, 3), (4, 3)]],
    "B": [[(0, 0), (0, 6), (3, 6), (4, 5), (4, 4), (3, 3), (0, 3)], [(3, 3), (4, 2), (4, 1), (3, 0), (0, 0)]],
    "C": [[(4, 6), (1, 6), (0, 5), (0, 1), (1, 0), (4, 0)]],
    "D": [[(0, 0), (0, 6), (2.5, 6), (4, 4.5), (4, 1.5), (2.5, 0), (0, 0)]],
    "E": [[(4, 6), (0, 6), (0, 0), (4, 0)], [(0, 3), (3, 3)]],
    "F": [[(4, 6), (0, 6), (0, 0)], [(0, 3), (3, 3)]],
    "I": [[(1, 6), (3, 6)], [(2, 6), (2, 0)], [(1, 0), (3, 0)]],
    "L": [[(0, 6), (0, 0), (4, 0)]],
    "M": [[(0, 0), (0, 6), (2, 3.5), (4, 6), (4, 0)]],
    "N": [[(0, 0), (0, 6), (4, 0), (4, 6)]],
    "O": [[(1, 0), (0, 1), (0, 5), (1, 6), (3, 6), (4, 5), (4, 1), (3, 0), (1, 0)]],
    "R": [[(0, 0), (0, 6), (3, 6), (4, 5), (4, 4), (3, 3), (0, 3)], [(2, 3), (4, 0)]],
    "S": [[(4, 6), (1, 6), (0, 5), (0, 4), (1, 3), (3, 3), (4, 2), (4, 1), (3, 0), (0, 0)]],
    "T": [[(0, 6), (4, 6)], [(2, 6), (2, 0)]],
    "U": [[(0, 6), (0, 1), (1, 0), (3, 0), (4, 1), (4, 6)]],
    "7": [[(0, 6), (4, 6), (1.5, 0)]],
    "-": [[(0.8, 3), (3.2, 3)]],
    "/": [[(0.5, 0), (3.5, 6)]],
    " ": [],
}


def text(s, height, width):
    """-> 2D text centred on the origin, reading along +x."""
    k = height / 6
    lines = []
    for i, ch in enumerate(s):
        for line in GLYPHS[ch]:
            lines.append([((x + i * 5.6) * k, y * k) for x, y in line])
    length = (len(s) * 5.6 - 1.6) * k
    return strokes(lines, width).translate((-length / 2, -height / 2)), length


# Upright (portrait) the top is -x. Seen from the front (-z) "right" is -y,
# seen from the back (+z) it's +y. Text/art drawn with +x = right, +y = up:
def on_front(cs, x, y):
    return cs.rotate(90).mirror((0, 1)).translate((x, y))


def on_back(cs, x, y):
    return cs.rotate(90).translate((x, y))


INLAY = 0.6      # mm deep: 3 layers at 0.2
TRACE_W = 1.2


def hazard(x0, x1, y0, y1, pitch=4.0):
    """Diagonal hazard stripes filling a rectangle."""
    bars = []
    for k in np.arange(-(x1 - x0) - (y1 - y0), (x1 - x0) + (y1 - y0), pitch):
        bars.append(CrossSection([[(x0 + k, y0), (x0 + k + pitch / 2, y0), (x0 + k + pitch / 2 + (y1 - y0), y1),
                                   (x0 + k + (y1 - y0), y1)]]))
    return CrossSection.batch_boolean(bars, OpType.Add) ^ CrossSection.square((x1 - x0, y1 - y0)).translate((x0, y0))


def radar_grille(w, h):
    """Speaker holes as a radar scope: rings broken by a crosshair and a sweep
    wedge, and a centre dot, within a w x h rectangle."""
    d = max(w, h)
    r_out = d / 2
    rings = []
    for r in np.arange(3.6, r_out - 1.0, 3.0):
        rings.append(CrossSection.circle(r + 0.7, 64) - CrossSection.circle(r - 0.7, 64))
    cs = CrossSection.batch_boolean(rings, OpType.Add)
    cross = CrossSection.square((2 * r_out, 1.4), center=True) + CrossSection.square((1.4, 2 * r_out), center=True)
    sweep = CrossSection([[(0, 0), (r_out, 0.0), (r_out * np.cos(0.5), r_out * np.sin(0.5))]]).rotate(30)
    return ((cs - cross - sweep) ^ rrect(-w / 2, w / 2, -h / 2, h / 2, 2.0)) + CrossSection.circle(1.2, 24)


# ---- The parts --------------------------------------------------------------------
def geometry(b):
    px0, px1, py0, py1 = b["pcb"]
    g = dict(b)
    g["in"] = (px0 - FIT, px1 + FIT, py0 - FIT, py1 + FIT)
    g["out"] = (px0 - FIT - WALL, px1 + FIT + WALL, py0 - FIT - WALL, py1 + FIT + WALL)
    g["z_front"] = b["panel"][4] - 0.3            # front plate's inside, just clear of the glass
    g["z_top"] = g["z_front"] - FRONT             # the front face
    bat_z0 = b["under_battery"] + 1.0
    g["bat_z"] = (bat_z0, bat_z0 + BATTERY[2])
    g["z_back_in"] = bat_z0 + BATTERY[2] + 0.3
    g["z_back"] = g["z_back_in"] + BACK           # the back face
    # Screen window = the active area. Upright the top is -x; seen from the
    # front, left is +y and right is -y.
    px0, px1, py0, py1, _ = b["panel"]
    top, bottom, left, right = b["border"]
    g["win"] = (px0 + top - OVERLAP, px1 - bottom + OVERLAP, py0 + right - OVERLAP, py1 - left + OVERLAP)
    return g


def front_part(g):
    """-> (front, inlay)"""
    ox0, ox1, oy0, oy1 = g["out"]
    ix0, ix1, iy0, iy1 = g["in"]
    zt, zf = g["z_top"], g["z_front"]
    body = Manifold.batch_hull([cslab(crect(ox0, ox1, oy0, oy1, 7.0), zt + 1.2, Z_SPLIT),
                                cslab(crect(ox0 + 1.2, ox1 - 1.2, oy0 + 1.2, oy1 - 1.2, 6.4), zt, zt + 0.01)])
    # Inside: the board from its front face back; in front of it, a ledge all
    # round the panel (the board rests on it), with the panel's pocket.
    body = body - box(ix0, ix1, iy0, iy1, -1.6, Z_SPLIT + 1)
    px0, px1, py0, py1, _ = g["panel"]
    body = body - box(px0 - FIT, px1 + FIT, py0 - FIT, py1 + FIT, zf, -1.5)
    # The board's screws: pilot holes in the ledge.
    for x, y in g["holes"]:
        body = body - cyl(x, y, zf + 0.6, 0, PILOT)
    # Screen window, chamfered outward.
    w0, w1, v0, v1 = g["win"]
    win = Manifold.batch_hull([cslab(rrect(w0, w1, v0, v1, 1.0), zf - 0.01, zf + 0.5),
                               cslab(rrect(w0 - FRONT, w1 + FRONT, v0 - FRONT, v1 + FRONT, 2.0), zt - 0.5, zt + 0.01)])
    body = body - win
    # USB-C at the bottom (+x): open at the joint, the back closes it.
    u0, u1 = g["usb"]
    body = body - Manifold.batch_hull([box(ix1 - 1, ox1 + 1, u0 + 1.5, u1 - 1.5, -2.6, Z_SPLIT + 1),
                                       box(ix1 - 1, ox1 + 1, u0, u1, -1.1, Z_SPLIT + 1)])
    # The side push button (-y edge): room for it, and a pin hole to reach it.
    bx = g["button"]
    body = body - box(bx - 2.6, bx + 2.6, iy0 - 0.8, iy0 + 1, -1.7, 2.4)
    body = body - Manifold.cylinder(WALL + 3, 1.0, 1.0, 24).rotate((90, 0, 0)).translate((bx, iy0 + 0.5, 1.0))
    # Markings on the front face (inlay): the LAB-7 tag on the chin below the
    # screen, CONTAINMENT UNIT above it, circuit traces down both sides.
    zface = zt
    art = []
    chin = (w1 + FRONT + ox1) / 2
    lab, _ = text("LAB-7", 4.6, 0.9)
    art.append(on_front(lab, chin, (oy0 + oy1) / 2))
    top = (ox0 + w0 - FRONT) / 2
    unit, ulen = text("CONTAINMENT UNIT", 2.6, 0.6)
    if ulen < (oy1 - oy0) - 16:
        art.append(on_front(unit, top, (oy0 + oy1) / 2))
    side = (oy0 + 1.2 + (v0 - FRONT)) / 2      # down the middle of each side border
    side2 = (oy1 - 1.2 + (v1 + FRONT)) / 2
    xa, xb = px0 + 6, px1 - 6
    for y, s in ((side, 1), (side2, -1)):
        art.append(strokes([[(xa, y), (xb - 8, y), (xb - 4, y + s * 0.0)],
                            [(xa + 10, y), (xa + 14, y - s * 0)]], TRACE_W,
                           [(xa, y, 2.4), (xb - 4, y, 2.4)]))
    grooves = cslab(CrossSection.batch_boolean(art, OpType.Add), zface - 0.01, zface + INLAY)
    inlay = grooves ^ body
    return body - inlay, inlay


def switch_cradle(g, body):
    """The power switch in the -y side wall (armband_mount.py's cradle, turned
    to stand on the back plate): lever out through the wall, pins inward."""
    sw = SWITCH
    oy0 = g["out"][2]
    x = g["switch_x"]
    zc = (Z_SPLIT + g["z_back_in"]) / 2 + 0.6
    x0, x1 = x - sw["length"] / 2 - SWITCH_FIT, x + sw["length"] / 2 + SWITCH_FIT
    zb, zt = zc - sw["thick"] / 2 - SWITCH_FIT, zc + sw["thick"] / 2 + SWITCH_FIT
    face = oy0 + 0.6
    yd = face + sw["height"]
    body = body - box(x0, x1, face, yd + 0.5, zb - 2, zt)                   # its pocket (open toward the joint)
    slot = sw["lever_w"] + sw["travel"] + 2 * LEVER_FIT
    body = body - box(x - slot / 2, x + slot / 2, oy0 - 1, face + 0.01,
                      zc - sw["lever_w"] / 2 - LEVER_FIT, zc + sw["lever_w"] / 2 + LEVER_FIT)
    yi = yd - 0.6
    zbk = g["z_back_in"]
    body = body + box(x0, x1, face, yi, zt, zbk + 0.1)                      # ledge (the back plate side)
    body = body + box(x0 - 1.6, x0, face, yi, zb - 0.8, zbk + 0.1)          # stops at both ends
    body = body + box(x1, x1 + 1.6, face, yi, zb - 0.8, zbk + 0.1)
    for xa, xb in ((x0 - 0.01, x0 + 0.3), (x1 - 0.3, x1 + 0.01)):          # catches under the body
        body = body + Manifold.batch_hull([box(xa, xb, face, yi, zb - 0.4, zb - 0.1),
                                           box(xa if xa < x else xb - 0.01, xb if xa < x else xb, face, yi,
                                               zb - 0.8, zb - 0.79)])
    return body


def back_part(g):
    """-> (back, inlay)"""
    ox0, ox1, oy0, oy1 = g["out"]
    ix0, ix1, iy0, iy1 = g["in"]
    zb, zbi = g["z_back"], g["z_back_in"]
    body = Manifold.batch_hull([cslab(crect(ox0, ox1, oy0, oy1, 7.0), Z_SPLIT, zb - 1.2),
                                cslab(crect(ox0 + 1.2, ox1 - 1.2, oy0 + 1.2, oy1 - 1.2, 6.4), zb - 0.01, zb)])
    body = body - box(ix0, ix1, iy0, iy1, Z_SPLIT - 1, zbi)
    # Columns down onto the board's corner holes: the M3 screws go through
    # them and the board into the front's pilot holes, clamping all three.
    seat = (-1.6 - 5.2) + SCREW_LEN          # screw head seat, so the tip ends 5.2 mm into the front
    for x, y in g["holes"]:
        body = body + (cyl(x, y, 0.05, zbi + 0.1, 7.0) ^ box(ix0, ix1, iy0, iy1, 0, zbi + 0.2))
        body = body - cyl(x, y, -1, zb + 1, 3.4)
        body = body - cyl(x, y, seat, zb + 1, 6.2)
    # Battery bay: a low fence on the back plate around it (the battery rests on
    # the board's low parts side, 1 mm clear of them, between fence and plate).
    bx, by, turned = g["battery"]
    bl, bw = (BATTERY[1], BATTERY[0]) if turned else (BATTERY[0], BATTERY[1])
    f0, f1, f2, f3 = bx - bl / 2 - 0.6, bx + bl / 2 + 0.6, by - bw / 2 - 0.6, by + bw / 2 + 0.6
    fence = box(f0 - 1.2, f1 + 1.2, f2 - 1.2, f3 + 1.2, zbi - 2.5, zbi + 0.1) - box(f0, f1, f2, f3, zbi - 3, zbi + 1)
    # gaps in the fence for the leads
    fence = fence - box(f1 - 1, f1 + 2, by - 5, by + 5, zbi - 3, zbi + 1) - box(bx - 5, bx + 5, f2 - 2, f2 + 1, zbi - 3, zbi + 1)
    body = body + fence
    # Speaker: a frame to glue it in (it hangs 7.5 mm down from the back
    # plate, clear of the board's parts under it), and the grille.
    sx, sy, along_x = g["speaker"]
    sl, sw_ = (SPEAKER[0], SPEAKER[1]) if along_x else (SPEAKER[1], SPEAKER[0])
    a0, a1, b0, b1 = sx - sl / 2 - 0.3, sx + sl / 2 + 0.3, sy - sw_ / 2 - 0.3, sy + sw_ / 2 + 0.3
    body = body + (box(a0 - 1.2, a1 + 1.2, b0 - 1.2, b1 + 1.2, zbi - 2.5, zbi + 0.1) - box(a0, a1, b0, b1, zbi - 3, zbi + 1)
                   - box(sx - 3, sx + 3, b0 - 2, b0 + 1, zbi - 3, zbi + 1))   # a gap for its leads
    body = body - cslab(radar_grille(sl - 4.0, sw_ - 4.0).translate((sx, sy)), zbi - 1, zb + 1)
    # USB-C at the bottom: the rest of the opening.
    u0, u1 = g["usb"]
    body = body - Manifold.batch_hull([box(ix1 - 1, ox1 + 1, u0 + 1.5, u1 - 1.5, Z_SPLIT - 1, 5.0),
                                       box(ix1 - 1, ox1 + 1, u0, u1, Z_SPLIT - 1, 4.6)])
    body = switch_cradle(g, body)
    # Lanyard tab at the top (-x end).
    tab = Manifold.batch_hull([box(ox0 + 2, ox0 + 3, -6 + (oy0 + oy1) / 2, 6 + (oy0 + oy1) / 2, zb - 5, zb),
                               cyl(ox0 - 5, (oy0 + oy1) / 2, zb - 5, zb, 9.0)])
    body = body + (tab - cyl(ox0 - 5, (oy0 + oy1) / 2, zb - 6, zb + 1, 4.2))
    # Markings on the back (inlay): LAB-7 and FIELD SCANNER at the top,
    # hazard stripes across the bottom, a trace from the tag to the grille.
    art = []
    cyy = (oy0 + oy1) / 2
    lab, llen = text("LAB-7", 8.0, 1.4)
    art.append(on_back(lab, ox0 + 14, cyy))
    fs, _ = text("FIELD SCANNER", 3.2, 0.7)
    art.append(on_back(fs, ox0 + 22, cyy))
    hz0, hz1 = ox1 - 9.0, ox1 - 3.5
    art.append(hazard(hz0, hz1, oy0 + 6, oy1 - 6, 4.0))
    grooves = cslab(CrossSection.batch_boolean(art, OpType.Add), zb - INLAY, zb + 0.01)
    inlay = grooves ^ body
    return body - inlay, inlay


def write_stl(man, path, title):
    mesh = man.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    f = np.asarray(mesh.tri_verts)
    with open(path, "wb") as fh:
        fh.write(title.encode().ljust(80, b" "))
        fh.write(struct.pack("<I", len(f)))
        for a, b, c in f:
            n = np.cross(v[b] - v[a], v[c] - v[a])
            n = n / (np.linalg.norm(n) or 1)
            fh.write(struct.pack("<12fH", *n, *v[a], *v[b], *v[c], 0))


def to_bed(m, flip, ref=None):
    """Print orientation: the front face down for the front, the back face down
    for the back. An inlay gets its part's shift (ref), not its own, so the two
    STLs line up when loaded together."""
    if flip:
        m = m.rotate((0, 180, 0))
        ref = ref.rotate((0, 180, 0)) if ref is not None else None
    lo = (ref if ref is not None else m).bounding_box()[:3]
    return m.translate((-lo[0], -lo[1], -lo[2]))


def build(name):
    g = geometry(BOARDS[name])
    f, fi = front_part(g)
    b, bi = back_part(g)
    return g, {"front": (f, False, None), "front_inlay": (fi, False, f), "back": (b, True, None),
               "back_inlay": (bi, True, b)}


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    for name in BOARDS:
        g, parts = build(name)
        for part, (m, flip, ref) in parts.items():
            m = to_bed(m, flip, ref)
            write_stl(m, os.path.join(here, f"scanner{name}_{part}.stl"), f"LAB-7 scanner {name} {part}")
            lo, hi = m.bounding_box()[:3], m.bounding_box()[3:]
            print(f"scanner{name}_{part}: {hi[0]-lo[0]:.1f} x {hi[1]-lo[1]:.1f} x {hi[2]-lo[2]:.1f} mm, "
                  f"{m.volume()/1000:.2f} cm3")
        ox0, ox1, oy0, oy1 = g["out"]
        print(f"  {name}: {ox1-ox0:.1f} tall x {oy1-oy0:.1f} wide x {g['z_back']-g['z_top']:.1f} thick; "
              f"M3 x {SCREW_LEN:.0f} screws")
