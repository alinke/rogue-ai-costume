#!/usr/bin/env python3
"""Harness hardware for wearing the 10" costume screen in landscape on the chest
(two straps: top slot -> over the shoulder -> cross on the back -> round the
side -> buckle -> bottom slot). Three printed parts, for 25 mm (1") webbing or
elastic:

  harness_buckle_socket.stl  side-release buckle, female half (print 2)
  harness_buckle_plug.stl    male half, with a ladder-lock adjuster tail to
                             set the strap length (print 2)
  harness_cross_clip.stl     keeps the two straps crossed between the shoulder
                             blades (print 1)

Buckle: squeeze the two hooks showing in the socket's side windows to open it.
The hooks are springs: print in PETG (PLA works but can snap after a while).
All three parts print flat, as they come, no supports, 0.2 mm, 3+ walls.

Needs Python 3 with manifold3d + numpy (pip install manifold3d numpy); writes
the STLs next to this file.

  python3 harness_parts.py
"""
import math
import os
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold

# ---- Your settings ----------------------------------------------------------------
STRAP_WIDTH = 24.0     # mm, webbing / elastic width (the mount's slots are 27 x 5)
STRAP_THICK = 1.0      # mm, its thickness (elastic 1 mm; webbing up to ~4 fits if you raise this)
CROSS_ANGLE = 60.0     # degrees between the two straps where they cross on the back

SLOT_W = STRAP_WIDTH + 1.5          # strap slots: length
SLOT_T = STRAP_THICK + 1.5          # strap slots: width
PLUG_H = 6.4                        # plug (and adjuster) thickness
WALL = 3.0                          # socket side walls (the windows are in these)
SKIN = 1.6                          # socket top / bottom
FIT = 0.6                           # side clearance between plug and socket


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


def rrect(x0, x1, y0, y1, r):
    """Rounded rectangle (2D)."""
    return CrossSection.square((x1 - x0 - 2 * r, y1 - y0 - 2 * r)).translate((x0 + r, y0 + r)).offset(r, JoinType.Round, circular_segments=32)


def slot(cx, cy, length, width, angle=0.0):
    """A strap slot (2D): length across the strap, rounded ends."""
    s = CrossSection.square((length - width, 0.01)).translate((-(length - width) / 2, -0.005))
    s = s.offset(width / 2, JoinType.Round, circular_segments=24)
    return s.rotate(angle).translate((cx, cy))


# Plug: inserted along +y. Its shoulder (y 0..2) stops at the socket's mouth.
ARM_Y0, LATCH_Y, ARM_END = 2.0, 26.0, 33.0     # arms: root, latch face, tip
CAV_HALF = 14.2                                 # socket cavity half-width
ARM_OUT = CAV_HALF - FIT                        # arm outer edge
ARM_W = 2.6                                     # arm width (the spring)
HOOK_OUT = CAV_HALF + WALL - 0.3                # hook reaches nearly flush with the socket's outside
TONGUE_END = 36.0


def plug():
    tail_y0 = -(3 + 2 * SLOT_T + 2 * 4.0)       # ladder-lock: two slots, a bar between
    half = CAV_HALF + WALL
    body = rrect(-half, half, tail_y0, 2.0, 3.0)
    s1 = -3 - SLOT_T / 2
    s2 = s1 - SLOT_T - 4.0
    body = body - slot(0, s1, SLOT_W, SLOT_T) - slot(0, s2, SLOT_W, SLOT_T)
    tongue = rrect(-4.0, 4.0, 1.0, TONGUE_END, 3.5)
    parts = [body, tongue]
    for sx in (-1, 1):
        x_in, x_out = ARM_OUT - ARM_W, ARM_OUT
        arm = CrossSection.square((ARM_W, ARM_END - ARM_Y0 + 1)).translate((x_in, ARM_Y0 - 1))
        hook = CrossSection([[(x_in + 0.5, LATCH_Y), (HOOK_OUT, LATCH_Y), (HOOK_OUT, LATCH_Y + 1.5),
                              (x_out, ARM_END), (x_in + 0.5, ARM_END)]])
        a = arm + hook
        if sx < 0:
            a = a.mirror((1, 0))
        parts.append(a)
    out = parts[0]
    for p in parts[1:]:
        out = out + p
    return out.extrude(PLUG_H)


def socket():
    half = CAV_HALF + WALL
    y0, cav_end = 2.0, TONGUE_END + 1.0
    y_back = cav_end + 2.0
    slot_c = y_back + SLOT_T / 2
    y1 = slot_c + SLOT_T / 2 + 4.0
    h = PLUG_H + 0.6 + 2 * SKIN
    outer = rrect(-half, half, y0, y1, 3.0).extrude(h)
    cav = box(-CAV_HALF, CAV_HALF, y0 - 1, cav_end, SKIN, h - SKIN)
    win = box(-half - 1, half + 1, LATCH_Y - 0.4, ARM_END - 0.4, SKIN, h - SKIN)   # both side windows
    win = win - box(-CAV_HALF + 0.01, CAV_HALF - 0.01, LATCH_Y - 1, ARM_END, 0, h)
    strap = slot(0, slot_c, SLOT_W, SLOT_T).extrude(h + 2).translate((0, 0, -1))
    s = outer - cav - win - strap
    return s


def cross_clip():
    """A plate with two slots per strap, the straps at CROSS_ANGLE: each strap goes
    down through one of its slots and back up through the other."""
    t = 3.0
    a = math.radians(CROSS_ANGLE / 2)
    dirs = [(math.sin(a), math.cos(a)), (-math.sin(a), math.cos(a))]
    # move the slots apart along their strap until the two straps' slots clear by 3 mm
    sep = SLOT_T
    while True:
        cut = []
        for dx, dy in dirs:
            ang = math.degrees(math.atan2(dy, dx)) + 90       # slot long axis across the strap
            for k in (-1, 1):
                cut.append(slot(k * sep * dx, k * sep * dy, SLOT_W, SLOT_T + 6, ang))
        clash = any((cut[i] ^ cut[j]).area() > 0.01 for i in (0, 1) for j in (2, 3))
        if not clash:
            break
        sep += 0.5
    r = math.hypot(sep + SLOT_T / 2, SLOT_W / 2) + 5.0
    plate = CrossSection.circle(r, 96)
    for dx, dy in dirs:
        ang = math.degrees(math.atan2(dy, dx)) + 90
        for k in (-1, 1):
            plate = plate - slot(k * sep * dx, k * sep * dy, SLOT_W, SLOT_T, ang)
    return plate.extrude(t)


def write_stl(man, path):
    mesh = man.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    f = np.asarray(mesh.tri_verts)
    with open(path, "wb") as fh:
        fh.write(b"ROGUE.AI harness".ljust(80, b" "))
        fh.write(struct.pack("<I", len(f)))
        for a, b, c in f:
            n = np.cross(v[b] - v[a], v[c] - v[a])
            n = n / (np.linalg.norm(n) or 1)
            fh.write(struct.pack("<12fH", *n, *v[a], *v[b], *v[c], 0))


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    parts = {"harness_buckle_socket": socket(), "harness_buckle_plug": plug(), "harness_cross_clip": cross_clip()}
    for name, m in parts.items():
        write_stl(m, os.path.join(here, name + ".stl"))
        lo, hi = m.bounding_box()[:3], m.bounding_box()[3:]
        print("%-24s %5.1f x %5.1f x %4.1f mm" % (name, hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]))
