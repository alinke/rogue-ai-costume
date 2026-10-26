#!/usr/bin/env python3
"""Cable grommet for the holes in the jacket (the LED leads pass through): a
hexagonal "data port" bezel like the LAB-7 logo, in two parts that clamp the
fabric round a 17.5 mm hole.

  grommet_front.stl   outside: a hex plate (engraved rivets, chamfered
                      opening) with a tube that goes through the hole
  grommet_back.stl    inside: a round ring that presses onto the tube, the
                      fabric sandwiched between the two

Cut the hole 17.5 mm across, push the front's tube through from outside, press
the back ring on from inside. A drop of super glue on the tube before pressing
makes it permanent (keep it off the fabric's visible face). The 15 mm opening
takes the strips' JST-SM plugs.

Print both flat, no supports: the front FACE DOWN (the tube up), so the
engraved face comes out smooth. PETG or PLA, black.

  python3 grommet.py            -> grommet_front.stl, grommet_back.stl
  python3 grommet.py --fabric 3 -> for thicker fabric (shell + lining), mm
"""
import argparse
import math
import os
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold

HERE = os.path.dirname(os.path.abspath(__file__))

OPEN = 15.0          # the opening the plugs go through
HOLE = OPEN + 2.5    # the hole in the jacket (the tube's 1.2 mm wall round the opening)
HEX = 31.0           # front plate, across the flats
PLATE = 2.0
TUBE_OD = HOLE - 0.4 # goes through the hole
FIT = 0.15           # the back ring's press fit on the tube (interference)
RING_OD, RING_T = 27.0, 1.6
RIVET_D, RIVET_DEPTH = 1.8, 0.5
CHAMFER = 1.2


def hexagon(across_flats, r=1.2):
    R = across_flats / 2 / math.cos(math.pi / 6)
    pts = [(R * math.cos(math.pi / 6 + k * math.pi / 3), R * math.sin(math.pi / 6 + k * math.pi / 3)) for k in range(6)]
    return CrossSection([pts]).offset(-r, JoinType.Square).offset(r, JoinType.Round, circular_segments=16)


def front(fabric):
    tube_len = fabric + RING_T - 0.3     # ends just inside the back ring
    plate = hexagon(HEX).extrude(PLATE)
    tube = Manifold.cylinder(tube_len, TUBE_OD / 2, TUBE_OD / 2, 72).translate((0, 0, PLATE))
    m = plate + tube
    # Opening, with a chamfer on the face (z = 0 is the face: printed face down).
    m = m - Manifold.cylinder(PLATE + tube_len + 2, OPEN / 2, OPEN / 2, 72).translate((0, 0, -1))
    m = m - Manifold.cylinder(CHAMFER, OPEN / 2 + CHAMFER, OPEN / 2, 72).translate((0, 0, -0.001))
    # Engraved details on the face: a groove ring, and a rivet at each corner.
    g0 = OPEN / 2 + CHAMFER + 1.3
    groove = Manifold.cylinder(0.4, g0 + 0.6, g0 + 0.6, 96) - Manifold.cylinder(0.4, g0, g0, 96)
    m = m - groove.translate((0, 0, -0.001))
    Rr = HEX / 2 / math.cos(math.pi / 6) - 3.2
    for k in range(6):
        a = math.pi / 6 + k * math.pi / 3
        m = m - Manifold.cylinder(RIVET_DEPTH, RIVET_D / 2, RIVET_D / 2, 24).translate(
            (Rr * math.cos(a), Rr * math.sin(a), -0.001))
    return m


def back():
    ring = Manifold.cylinder(RING_T, RING_OD / 2, RING_OD / 2, 96)
    return ring - Manifold.cylinder(RING_T + 2, (TUBE_OD - FIT) / 2, (TUBE_OD - FIT) / 2, 96).translate((0, 0, -1))


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
    ap.add_argument("--fabric", type=float, default=1.5, help="fabric thickness at the hole, mm (shell, or shell + lining)")
    a = ap.parse_args()
    for name, m in (("grommet_front", front(a.fabric)), ("grommet_back", back())):
        write_stl(m, os.path.join(HERE, name + ".stl"), "ROGUE.AI " + name)
        b = m.bounding_box()
        print("%s.stl: %.1f x %.1f x %.1f mm" % (name, b[3] - b[0], b[4] - b[1], b[5] - b[2]))
