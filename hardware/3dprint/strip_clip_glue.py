#!/usr/bin/env python3
"""LED strip channel for gluing onto the jacket (no sewing): led_clips.scad's
channel profile -- the strip snaps in under two small lips -- with wider
flanges for the glue and round holes the hot glue pushes up through and sets
in, like rivets (a safety pin fits them too, for the sleeves).

  strip_clip_glue_60mm.stl   straight, stiff runs (the zipper edges)
  strip_clip_glue_30mm.stl   curves and places that flex (round the neck, sleeves)

Print in TPU 95A, flat, no supports. Glue: a bead under each flange, press the
clip onto the fabric and push the glue up through the holes; snap the strip in
once it's set. The strip's own thickness decides the fit: print one first and
check the strip snaps in and doesn't slide (--thick to adjust).

  python3 strip_clip_glue.py                 -> both
  python3 strip_clip_glue.py --thick 1.8     -> for a 1.8 mm strip
"""
import argparse
import os
import struct

import numpy as np
from manifold3d import CrossSection, Manifold

HERE = os.path.dirname(os.path.abspath(__file__))

STRIP_W = 5.0
FIT = 0.4
WALL = 1.2
BASE = 1.0
LIP_OVER = 0.5
LIP_T = 0.6
CLEAR = 0.2
FLANGE_W = 6.0     # was 4 (for stitching): more glue area
FLANGE_T = 1.0
HOLE = 2.5         # round, for the glue to push through
PITCH = 7.0


def channel(length, thick):
    a = (STRIP_W + FIT) / 2
    wo = a + WALL
    ll = BASE + thick + CLEAR
    top = ll + LIP_T
    parts = [CrossSection.square((2 * wo, BASE)).translate((-wo, 0))]
    for s in (-1, 1):
        parts.append(CrossSection.square((FLANGE_W, FLANGE_T)).translate((wo if s > 0 else -wo - FLANGE_W, 0)))
        parts.append(CrossSection.square((WALL, top)).translate((a if s > 0 else -wo, 0)))
        parts.append(CrossSection.square((LIP_OVER, LIP_T)).translate((a - LIP_OVER if s > 0 else -a, ll)))
    prof = parts[0]
    for p in parts[1:]:
        prof = prof + p
    # profile in (y, z) -> extrude along x
    m = prof.extrude(length).rotate((90, 0, 90))
    hc = wo + FLANGE_W / 2
    n = max(1, int((length - 2) // PITCH))
    x0 = (length - (n - 1) * PITCH) / 2
    for i in range(n):
        for s in (-1, 1):
            m = m - Manifold.cylinder(10, HOLE / 2, HOLE / 2, 24).translate((x0 + i * PITCH, s * hc, -1))
    return m


def write_stl(man, path, name):
    mesh = man.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    f = np.asarray(mesh.tri_verts)
    with open(path, "wb") as fh:
        fh.write(name.encode().ljust(80, b" "))
        fh.write(struct.pack("<I", len(f)))
        for a, b, c in f:
            nrm = np.cross(v[b] - v[a], v[c] - v[a])
            nrm = nrm / (np.linalg.norm(nrm) or 1)
            fh.write(struct.pack("<12fH", *nrm, *v[a], *v[b], *v[c], 0))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--thick", type=float, default=2.0, help="strip thickness, mm (measure yours)")
    a = ap.parse_args()
    for L in (60, 30):
        m = channel(L, a.thick)
        name = "strip_clip_glue_%dmm" % L
        write_stl(m, os.path.join(HERE, name + ".stl"), "ROGUE.AI " + name)
        b = m.bounding_box()
        print("%s.stl: %.1f x %.1f x %.1f mm" % (name, b[3] - b[0], b[4] - b[1], b[5] - b[2]))
