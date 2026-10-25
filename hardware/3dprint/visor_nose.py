#!/usr/bin/env python3
"""Nose rest for the laser-cut ROGUE.AI visor (size A): clips onto the nose
arch's edge and lines it with a soft-edged saddle, so the visor sits on the
nose (or on the bridge of your glasses) instead of the acrylic edge.

The arch is read from ../lasercut/rogue_ai_visor_glasses_A.svg. Seen from the
front it's a band following the arch: its outer half grips the acrylic in a
slot (front lip + back jaw), its inner half is the pad, which also runs
straight back toward the face (PAD_DEPTH) so the nose bears on a wide surface.
Push it on from below -- snug on 3 mm acrylic; a drop of clear glue or
silicone makes it permanent.

Print front face down (the lip on the bed), PETG or PLA, no supports: the back
jaw is a short overhang. Size B's arch differs -- this is size A only.

  python3 visor_nose.py      -> visor_nose.stl
"""
import os
import re
import struct

import numpy as np
from manifold3d import CrossSection, FillRule, JoinType, Manifold
from svgpathtools import parse_path

HERE = os.path.dirname(os.path.abspath(__file__))
SVG = os.path.join(HERE, "..", "lasercut", "rogue_ai_visor_glasses_A.svg")

# ---- Your settings ----------------------------------------------------------------
ACRYLIC = 3.1     # slot width: 3 mm acrylic (measured 2.97) + a snug margin
GRIP = 3.0        # how far the clip reaches onto the acrylic around the arch
PAD = 2.4         # pad thickness inside the arch (takes that much off the nose room)
LIP = 1.6         # front lip (in front of the acrylic)
JAW = 2.4         # back jaw (behind the acrylic)
PAD_DEPTH = 11.0  # the pad's reach behind the visor, toward the face
BOTTOM = 39.0     # the band stops here (mm from the visor's top edge): the arch's
                  # sides, clear of the visor's bottom corners

PT = 25.4 / 72


def arch():
    """The nose cutout as a polygon (mm, x centred, y down from the visor's top),
    closed off well below the visor's bottom edge."""
    s = open(SVG).read()
    g = s[s.index('<g id="cut"'):]
    g = g[:g.index("</g>")]
    paths = [parse_path(d) for d in re.findall(r'\sd="([^"]+)"', g)]
    outline = max(paths, key=lambda p: p.bbox()[1] - p.bbox()[0])
    x0, x1, y0, y1 = outline.bbox()
    cx = (x0 + x1) / 2 * PT
    pts = np.array([[q.real * PT - cx, q.imag * PT] for q in
                    (outline.point(t) for t in np.linspace(0, 1, 40000))])
    sel = (np.abs(pts[:, 0]) < 17) & (pts[:, 1] > 20) & (pts[:, 1] < y1 * PT - 0.6)
    a = pts[sel]
    a = a[np.argsort(a[:, 0])]      # left to right along the arch
    poly = [tuple(p) for p in a[::4]] + [(a[-1, 0] + 3, 70.0), (a[0, 0] - 3, 70.0)]
    return CrossSection([poly], FillRule.EvenOdd), a


def band(hole, inner, outer):
    """The ring between `inner` mm inside the hole and `outer` mm outside it,
    above BOTTOM."""
    ring = hole.offset(outer, JoinType.Round, circular_segments=48) - \
        hole.offset(-inner, JoinType.Round, circular_segments=48)
    keep = CrossSection.square((80, BOTTOM - 10)).translate((-40, 10))
    return ring ^ keep


def nose():
    hole, _ = arch()
    pad = band(hole, PAD, 0.0)            # inside the arch: the nose bears on it
    full = band(hole, PAD, GRIP)          # pad + grip
    z = 0.0
    parts = [full.extrude(LIP)]           # front lip
    z += LIP
    parts.append(pad.extrude(ACRYLIC).translate((0, 0, z)))   # through the slot
    z += ACRYLIC
    parts.append(full.extrude(JAW).translate((0, 0, z)))      # back jaw
    parts.append(pad.extrude(PAD_DEPTH).translate((0, 0, z)))  # the saddle, toward the face
    m = parts[0]
    for p in parts[1:]:
        m = m + p
    # Mirror y so +y is up when printed (svg y runs down).
    return m.scale((1, -1, 1))


def write_stl(man, path):
    mesh = man.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    f = np.asarray(mesh.tri_verts)
    with open(path, "wb") as fh:
        fh.write(b"ROGUE.AI visor nose rest".ljust(80, b" "))
        fh.write(struct.pack("<I", len(f)))
        for a, b, c in f:
            n = np.cross(v[b] - v[a], v[c] - v[a])
            n = n / (np.linalg.norm(n) or 1)
            fh.write(struct.pack("<12fH", *n, *v[a], *v[b], *v[c], 0))


if __name__ == "__main__":
    m = nose()
    write_stl(m, os.path.join(HERE, "visor_nose.stl"))
    b = m.bounding_box()
    print("visor_nose.stl: %.1f x %.1f x %.1f mm" % (b[3] - b[0], b[4] - b[1], b[5] - b[2]))
    _, a = arch()
    print("arch top at %.1f mm; nose room after the pad: %.1f mm wide at 5 mm below the top"
          % (a[:, 1].min(), (lambda y: np.ptp(a[a[:, 1] < y][:, 0]))(a[:, 1].min() + 5) - 2 * PAD))
