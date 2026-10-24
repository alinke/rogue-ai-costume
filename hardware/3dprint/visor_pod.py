#!/usr/bin/env python3
"""Battery pod for the edge-lit visor: worn on the back of the head, on the
visor's own strap. The 1 x CR2032 holder with switch (the badge's: 23.9 x 34.1
x 9 mm) clicks into an open-top tray -- switch and battery cover facing out --
and the strap runs through a tunnel along the pod's underside, so the strap
holds it. A notch in one end lets the LED wires out (they run along the strap
to the visor's end notches).

Print tray-up (tunnel floor on the bed), PETG or PLA, no supports: the
tunnel's roof is a short bridge.

  python3 visor_pod.py      -> visor_pod.stl
"""
import os
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold

# ---- Your settings ----------------------------------------------------------------
HOLDER = (23.9, 34.1, 9.0)  # coin-cell holder: width, length, height (measure yours)
STRAP_W = 20.0              # the visor strap (its slots take up to ~21 mm)
STRAP_T = 2.0               # its thickness (elastic ~1 mm, webbing ~2 mm)
FIT = 0.4                   # clearance around the holder
WALL = 1.8
FLOOR = 1.6                 # between the holder and the strap tunnel
SKIN = 1.2                  # under the strap (against the head)
LIP = 0.6                   # retaining lips at the tray's rim: the holder clicks under them
WIRE = (5.0, 3.5)           # wire notch in one end wall: width, depth from the rim

TUNNEL_W = STRAP_W + 2.0
TUNNEL_H = STRAP_T + 1.0


def rounded_box(w, l, h, r):
    cs = CrossSection.square((w - 2 * r, l - 2 * r)).translate((-(w - 2 * r) / 2, -(l - 2 * r) / 2))
    return cs.offset(r, JoinType.Round, circular_segments=48).extrude(h)


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


def pod():
    hw, hl, hh = HOLDER
    cw, cl = hw + 2 * FIT, hl + 2 * FIT              # the tray
    ow = max(cw, TUNNEL_W) + 2 * WALL
    ol = cl + 2 * WALL
    z_tray = SKIN + TUNNEL_H + FLOOR                  # tray floor
    oh = z_tray + hh                                  # the holder sits flush with the rim
    body = rounded_box(ow, ol, oh, 3.0)
    # The tray, open on top.
    body = body - box(-cw / 2, cw / 2, -cl / 2, cl / 2, z_tray, oh + 1)
    # Lips along the long sides' rim: a small overhang the holder clicks under.
    for sx in (-1, 1):
        x_in = sx * cw / 2
        lip = box(min(x_in, x_in - sx * LIP), max(x_in, x_in - sx * LIP), -cl / 2 + 4, cl / 2 - 4, oh - 1.0, oh)
        body = body + lip
    # The strap tunnel, along the length (the strap runs round the head).
    body = body - box(-TUNNEL_W / 2, TUNNEL_W / 2, -ol / 2 - 1, ol / 2 + 1, SKIN, SKIN + TUNNEL_H)
    # Wire notch in one end wall.
    body = body - box(-WIRE[0] / 2, WIRE[0] / 2, cl / 2 - 1, ol / 2 + 1, oh - WIRE[1], oh + 1)
    return body


def write_stl(man, path):
    mesh = man.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    f = np.asarray(mesh.tri_verts)
    with open(path, "wb") as fh:
        fh.write(b"ROGUE.AI visor battery pod".ljust(80, b" "))
        fh.write(struct.pack("<I", len(f)))
        for a, b, c in f:
            n = np.cross(v[b] - v[a], v[c] - v[a])
            n = n / (np.linalg.norm(n) or 1)
            fh.write(struct.pack("<12fH", *n, *v[a], *v[b], *v[c], 0))


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    m = pod()
    write_stl(m, os.path.join(here, "visor_pod.stl"))
    b = m.bounding_box()
    print("visor_pod.stl: %.1f x %.1f x %.1f mm" % (b[3] - b[0], b[4] - b[1], b[5] - b[2]))
