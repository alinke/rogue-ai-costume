#!/usr/bin/env python3
"""Cable grommet for the holes in the jacket (the LED leads pass through): a
hexagonal "data port" bezel like the LAB-7 logo, glued on from the outside.

One piece: a hex plate with a 15 mm opening (takes the strips' JST-SM plugs),
a chamfer and a groove ring round it, and 12 glue holes -- at the corners and
the middle of each edge -- that the hot glue pushes up through and sets in,
like the strip clips (they read as the bezel's rivets). A short tube on the
back sits in the hole in the jacket and keeps its cut edge from fraying.

Cut the hole 17.5 mm across, push the tube in, glue round the plate's edge so
the glue comes up through the holes.

Print FACE DOWN (the tube up), so the face comes out smooth, no supports.
PETG or PLA, black.

  python3 grommet.py              -> grommet.stl
  python3 grommet.py --fabric 3   -> a longer tube for thicker fabric, mm
"""
import argparse
import math
import os
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold

HERE = os.path.dirname(os.path.abspath(__file__))

OPEN = 15.0          # the opening the plugs go through
HOLE = OPEN + 2.5    # the hole in the jacket (the tube's wall round the opening)
HEX = 31.0           # across the flats
PLATE = 2.2
TUBE_OD = HOLE - 0.4
CHAMFER = 1.2
GLUE_HOLE = 2.5


def hexagon(across_flats, r=1.2):
    R = across_flats / 2 / math.cos(math.pi / 6)
    pts = [(R * math.cos(math.pi / 6 + k * math.pi / 3), R * math.sin(math.pi / 6 + k * math.pi / 3)) for k in range(6)]
    return CrossSection([pts]).offset(-r, JoinType.Square).offset(r, JoinType.Round, circular_segments=16)


def glue_holes(across_flats, inset):
    """Holes at the six corners and the six edge midpoints, `inset` in from the edge."""
    R = across_flats / 2 / math.cos(math.pi / 6) - inset * 1.15
    a_ = across_flats / 2 - inset
    out = []
    for k in range(6):
        c = math.pi / 6 + k * math.pi / 3
        out.append((R * math.cos(c), R * math.sin(c)))
        m = k * math.pi / 3
        out.append((a_ * math.cos(m), a_ * math.sin(m)))
    return out


def ring_groove(r0, depth=0.4, w=0.6):
    return Manifold.cylinder(depth, r0 + w, r0 + w, 96) - Manifold.cylinder(depth, r0, r0, 96)


def grommet(fabric):
    m = hexagon(HEX).extrude(PLATE)
    m = m + Manifold.cylinder(fabric, TUBE_OD / 2, TUBE_OD / 2, 72).translate((0, 0, PLATE))
    m = m - Manifold.cylinder(PLATE + fabric + 2, OPEN / 2, OPEN / 2, 72).translate((0, 0, -1))
    # z = 0 is the face (printed face down): chamfered opening, a groove ring.
    m = m - Manifold.cylinder(CHAMFER, OPEN / 2 + CHAMFER, OPEN / 2, 72).translate((0, 0, -0.001))
    m = m - ring_groove(OPEN / 2 + CHAMFER + 1.0).translate((0, 0, -0.001))
    for x, y in glue_holes(HEX, 2.6):
        m = m - Manifold.cylinder(PLATE + 2, GLUE_HOLE / 2, GLUE_HOLE / 2, 24).translate((x, y, -1))
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
    ap.add_argument("--fabric", type=float, default=1.5, help="fabric thickness at the hole, mm (the tube's length)")
    a = ap.parse_args()
    m = grommet(a.fabric)
    write_stl(m, os.path.join(HERE, "grommet.stl"), "ROGUE.AI grommet")
    b = m.bounding_box()
    print("grommet.stl: %.1f x %.1f x %.1f mm, opening %.1f mm, hole to cut %.1f mm" %
          (b[3] - b[0], b[4] - b[1], b[5] - b[2], OPEN, HOLE))
