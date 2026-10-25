#!/usr/bin/env python3
"""Cable port: glued onto the jacket, an Ethernet cable comes out of it -- the
costume's "data link". A flat plate with rivet holes for the hot glue (like
strip_clip_glue.py), and a raised collar whose bore points down at an angle so
the cable hangs naturally instead of sticking straight out.

The cable comes through the jacket under the port (a small slit in the jacket,
hidden by the plate) or runs out from under its bottom edge in the groove
underneath. A slit along the bore lets the cable snap in from the side, so the
RJ45 plug never has to fit through.

Print plate-down, TPU (the slit flexes; PETG works if the slit is opened a
little with --slit), no supports: the bore leans 35 degrees.

  python3 cable_port.py                  -> cable_port.stl (6 mm cable)
  python3 cable_port.py --cable 5.5      -> for a thinner cable (measure yours)
"""
import argparse
import math
import os
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold

HERE = os.path.dirname(os.path.abspath(__file__))

PLATE_W, PLATE_H, PLATE_T, PLATE_R = 40.0, 48.0, 2.2, 6.0
HOLE, HOLE_PITCH, HOLE_INSET = 2.5, 7.0, 3.5
COLLAR_D, COLLAR_L = 17.0, 13.0   # the raised collar, along its (tilted) axis
TILT = 35.0                       # degrees from straight out, leaning down
GROOVE_D = 2.0                    # depth of the cable groove under the plate (a flat run of cable)


def rounded_rect(w, h, r):
    return CrossSection.square((w - 2 * r, h - 2 * r)).translate((-(w - 2 * r) / 2, -(h - 2 * r) / 2)).offset(
        r, JoinType.Round, circular_segments=48)


def port(cable, slit):
    bore = cable + 0.5
    plate = rounded_rect(PLATE_W, PLATE_H, PLATE_R)
    # Rivet holes round the edge (clear of the collar).
    for x in np.arange(-PLATE_W / 2 + HOLE_INSET + 2, PLATE_W / 2 - HOLE_INSET - 1, HOLE_PITCH):
        for y in (-PLATE_H / 2 + HOLE_INSET, PLATE_H / 2 - HOLE_INSET):
            plate = plate - CrossSection.circle(HOLE / 2, 20).translate((x, y))
    for y in np.arange(-PLATE_H / 2 + HOLE_INSET + HOLE_PITCH, PLATE_H / 2 - HOLE_INSET - 1, HOLE_PITCH):
        for x in (-PLATE_W / 2 + HOLE_INSET, PLATE_W / 2 - HOLE_INSET):
            plate = plate - CrossSection.circle(HOLE / 2, 20).translate((x, y))
    m = plate.extrude(PLATE_T)
    # The collar: a cylinder leaning TILT degrees toward -y (down), rooted in the
    # plate a little above centre so its mouth sits over the middle.
    tilt = math.radians(TILT)
    base_y = 5.0
    collar = Manifold.cylinder(COLLAR_L + 4, COLLAR_D / 2, COLLAR_D / 2 - 1.5, 64)
    ring = Manifold.cylinder(1.2, COLLAR_D / 2 + 0.8, COLLAR_D / 2 + 0.8, 64).translate((0, 0, COLLAR_L - 3))
    collar = collar + ring
    collar = collar.translate((0, 0, -4)).rotate((TILT, 0, 0)).translate((0, base_y, 0))
    m = m + collar
    # Flat bottom: anything below the plate goes.
    m = m ^ Manifold.cube((200, 200, 100)).translate((-100, -100, 0))
    # The bore: along the collar's axis, out through the plate's back.
    bore_m = Manifold.cylinder(COLLAR_L + 40, bore / 2, bore / 2, 48).translate((0, 0, -20))
    bore_m = bore_m.rotate((TILT, 0, 0)).translate((0, base_y, 0))
    # The slit: from the bore out through the collar's side (+x), full length.
    sl = Manifold.cube((COLLAR_D, slit, COLLAR_L + 40)).translate((0, -slit / 2, -20))
    sl = sl.rotate((TILT, 0, 0)).translate((0, base_y, 0))
    m = m - bore_m - sl
    # Groove underneath: from the bore down to the plate's bottom edge, for a
    # cable that runs along the jacket instead of through it.
    g = CrossSection.square((bore, PLATE_H)).translate((-bore / 2, -PLATE_H + base_y))
    m = m - g.extrude(GROOVE_D).translate((0, 0, -0.01))
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
    ap.add_argument("--cable", type=float, default=6.0, help="cable diameter, mm (Cat5e/6 round: 5.5-6.5)")
    ap.add_argument("--slit", type=float, default=4.8, help="side slit width, mm (snap-in; wider = easier)")
    a = ap.parse_args()
    m = port(a.cable, a.slit)
    write_stl(m, os.path.join(HERE, "cable_port.stl"), "ROGUE.AI cable port")
    b = m.bounding_box()
    print("cable_port.stl: %.1f x %.1f x %.1f mm, bore %.1f mm, slit %.1f mm" %
          (b[3] - b[0], b[4] - b[1], b[5] - b[2], a.cable + 0.5, a.slit))
