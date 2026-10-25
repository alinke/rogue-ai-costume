#!/usr/bin/env python3
"""Strap clip: a slim tri-glide (three-bar slider) that closes and adjusts a
strap loop with no sewing and no snap -- for the armband's two straps and the
visor's strap.

    end A   wrap it once round the CENTRE bar (in through one slot, out the other)
    end B   after going round the arm / through the visor's slot: up through
            slot 1, over the centre bar (on top of A's wrap), down through slot 2

  ====[ bar |slot 1| CENTRE |slot 2| bar ]====      B pinches A against the centre
                                                   bar, so both ends hold by friction
Tighten: pull B's tail. Loosen: tilt the clip up (lifting B off the centre bar)
and slide. The centre bar has fine ridges on both faces so slippery elastic
grips too.

Print flat, PETG (flexes instead of snapping), no supports.

  python3 strap_clip.py                         -> strap_clip_25mm.stl (armband),
                                                   strap_clip_20mm.stl (visor)
  python3 strap_clip.py --strap 24 --thick 1    -> one clip for that strap
  python3 strap_clip.py --strap 24 --thick 1 --layers 4
                                                -> strap_clip_24mm_4layer.stl: slots
                                                   that also take the loose end
                                                   tucked back through (4 layers)
"""
import argparse
import os
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold

HERE = os.path.dirname(os.path.abspath(__file__))

SIDE = 2.4        # side rails
BAR = 2.6         # outer bars, along the strap
CENTRE = 3.6      # centre bar
H = 2.8           # thickness (the bars); the centre bar is a little thinner
CENTRE_H = 2.2
RIDGE = 0.35      # grip ridges on the centre bar
ROUND = 1.0       # rounded outline


def clip(strap, thick, layers=2):
    slot = layers * thick + 0.8            # that many layers of strap through each slot + room
    inner = strap + 1.0                    # slot length, across the strap
    L = 2 * BAR + 2 * slot + CENTRE
    Wd = inner + 2 * SIDE
    outline = CrossSection.square((L - 2 * ROUND, Wd - 2 * ROUND)).translate(
        (-(L - 2 * ROUND) / 2, -(Wd - 2 * ROUND) / 2)).offset(ROUND, JoinType.Round, circular_segments=32)
    m = outline.extrude(H)
    # The two slots.
    for sx in (-1, 1):
        cx = sx * (CENTRE / 2 + slot / 2)
        s = CrossSection.square((slot, inner)).translate((cx - slot / 2, -inner / 2))
        s = s.offset(0.0)
        m = m - s.extrude(H + 2).translate((0, 0, -1))
    # Centre bar: thinner and centred in height, so the strap wraps round it
    # easily and the clip sits flat; ridges across it on both faces.
    cut = (H - CENTRE_H) / 2
    bar = CrossSection.square((CENTRE, inner)).translate((-CENTRE / 2, -inner / 2))
    m = m - bar.extrude(cut).translate((0, 0, -0.001)) - bar.extrude(cut + 0.01).translate((0, 0, H - cut))
    n = 3
    for i in range(n):
        x = -CENTRE / 2 + CENTRE * (i + 0.5) / n
        r = CrossSection.square((0.6, inner)).translate((x - 0.3, -inner / 2))
        m = m + r.extrude(RIDGE).translate((0, 0, H - cut))
        m = m + r.extrude(RIDGE).translate((0, 0, cut - RIDGE))
    return m, (L, Wd, slot)


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
    ap.add_argument("--strap", type=float, help="strap width, mm")
    ap.add_argument("--thick", type=float, default=1.5, help="strap thickness, mm (elastic ~1, webbing ~1.5)")
    ap.add_argument("--layers", type=int, default=2, help="strap layers each slot takes (4 = room to tuck the end back through)")
    a = ap.parse_args()
    sizes = [a.strap] if a.strap else [25.0, 20.0]
    for w in sizes:
        m, (L, Wd, slot) = clip(w, a.thick, a.layers)
        name = "strap_clip_%gmm" % w + ("_%dlayer" % a.layers if a.layers != 2 else "")
        write_stl(m, os.path.join(HERE, name + ".stl"), "ROGUE.AI " + name)
        print("%s.stl: %.1f x %.1f x %.1f mm, slots %.1f mm (strap %g x %g)" % (name, L, Wd, H, slot, w, a.thick))
