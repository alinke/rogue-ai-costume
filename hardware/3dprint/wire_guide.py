#!/usr/bin/env python3
"""Wire guides for the inside of the jacket: short glue-on channels the LED
extension leads (flat 3-wire, 5.4 x 1.75 mm) snap into, so the cabling inside
lies flat and tidy instead of hanging loose. Same idea as strip_clip_glue.py:
the cable presses in past two small lips; wide flanges with round holes the hot
glue pushes up through and sets in.

  wire_guide_single.stl   one lead
  wire_guide_double.stl   two side by side (a data lead and a power wire)

Glue them on about every 10 cm along each lead's route, and either side of
anything that bends (the shoulder, the armpit). Print TPU (it flexes with the
fabric), flat, no supports.

  python3 wire_guide.py                       -> both
  python3 wire_guide.py --width 5.4 --thick 1.75 --length 15
"""
import argparse
import os
import struct

import numpy as np
from manifold3d import CrossSection, Manifold

HERE = os.path.dirname(os.path.abspath(__file__))

FIT = 0.4          # room round the cable (width)
CLEAR = 0.2        # room above it, under the lips
WALL = 1.0
BASE = 0.8
LIP_OVER = 0.5     # the lips the cable snaps past
LIP_T = 0.6
FLANGE_W = 5.0
FLANGE_T = 0.9
HOLE = 2.2


def guide(width, thick, length, n):
    a = width + FIT                       # channel width
    ll = BASE + thick + CLEAR             # underside of the lips
    top = ll + LIP_T
    inner_w = n * a + (n - 1) * WALL      # channels side by side, one wall between
    wo = inner_w / 2 + WALL
    parts = [CrossSection.square((2 * wo, BASE)).translate((-wo, 0))]
    for s in (-1, 1):
        parts.append(CrossSection.square((FLANGE_W, FLANGE_T)).translate((wo if s > 0 else -wo - FLANGE_W, 0)))
    # walls: outer two plus the ones between channels
    xs = [-wo + WALL / 2 + k * (a + WALL) for k in range(n + 1)]
    for x in xs:
        parts.append(CrossSection.square((WALL, top)).translate((x - WALL / 2, 0)))
    # lips on both sides of every channel
    for k in range(n):
        c0 = -wo + WALL + k * (a + WALL)
        parts.append(CrossSection.square((LIP_OVER, LIP_T)).translate((c0, ll)))
        parts.append(CrossSection.square((LIP_OVER, LIP_T)).translate((c0 + a - LIP_OVER, ll)))
    prof = parts[0]
    for p in parts[1:]:
        prof = prof + p
    m = prof.extrude(length).rotate((90, 0, 90))
    hc = wo + FLANGE_W / 2
    for x in (length * 0.27, length * 0.73):
        for s in (-1, 1):
            m = m - Manifold.cylinder(10, HOLE / 2, HOLE / 2, 24).translate((x, s * hc, -1))
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
    ap.add_argument("--width", type=float, default=5.4, help="cable width, mm")
    ap.add_argument("--thick", type=float, default=1.75, help="cable thickness, mm")
    ap.add_argument("--length", type=float, default=15.0, help="guide length along the cable, mm")
    a = ap.parse_args()
    for n, name in ((1, "wire_guide_single"), (2, "wire_guide_double")):
        m = guide(a.width, a.thick, a.length, n)
        write_stl(m, os.path.join(HERE, name + ".stl"), "ROGUE.AI " + name)
        b = m.bounding_box()
        print("%s.stl: %.1f x %.1f x %.1f mm" % (name, b[3] - b[0], b[4] - b[1], b[5] - b[2]))
