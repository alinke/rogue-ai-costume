#!/usr/bin/env python3
"""Cable port: an Ethernet cable "plugged into" the costume -- the costume's
data link. Same style as grommet.py: a hexagonal bezel with glue holes at the
corners and edge midpoints, and a collar with a lip and a groove ring. The
collar leans down, so the cable hangs instead of sticking straight out.

The cable doesn't go through the jacket. It goes in through the collar from
the front; its end comes out in a pocket in the port's back, and you fill that
pocket with hot glue from the back -- the glue sets round the cable's end and
locks it in. Then glue the port's back onto the jacket.

  1  push the cable in from the front until its end is just inside the back pocket
  2  turn it over, fill the back pocket with hot glue, let it set
  3  a bead of hot glue round the back's edge, press it onto the jacket so the
     glue comes up through the holes

Print back down (the collar up), no supports: the collar and its bore lean 35
degrees. PETG or PLA, black.

  python3 cable_port.py              -> cable_port.stl (6 mm cable)
  python3 cable_port.py --cable 5.5  -> for another cable (measure yours)
"""
import argparse
import math
import os

from manifold3d import Manifold

from grommet import GLUE_HOLE, glue_holes, hexagon, ring_groove, write_stl

HERE = os.path.dirname(os.path.abspath(__file__))

TILT = 35.0         # degrees from straight out, leaning down
BASE = 5.0          # the hex base: thick enough for the glue pocket
WALL = 3.5          # collar wall round the cable
COLLAR = 20.0       # collar length along its axis
POCKET_MARGIN = 2.5 # the back pocket: this much wider than where the bore comes out...
POCKET_DEPTH = 3.0  # ...and this deep
RIM = 8.0           # flange round the collar, for the glue holes


def port(cable):
    t = math.radians(TILT)
    bore = cable + 0.5
    collar_r = bore / 2 + WALL
    y0 = COLLAR * math.sin(t) * 0.35          # the collar's root, a little above centre
    ry = collar_r / math.cos(t)               # its footprint on the base: an oval
    hexsize = 2 * (y0 + ry) + 2 * RIM
    m = hexagon(hexsize, r=2.0).extrude(BASE)
    for x, y in glue_holes(hexsize, 3.0):
        if (x / (collar_r + 2.5)) ** 2 + ((y - y0) / (ry + 2.5)) ** 2 < 1:
            continue
        m = m - Manifold.cylinder(BASE + 2, GLUE_HOLE / 2, GLUE_HOLE / 2, 24).translate((x, y, -1))

    def along(man):  # z-axis part -> the leaning collar
        return man.rotate((TILT, 0, 0)).translate((0, y0, 0))

    L = COLLAR
    collar = Manifold.cylinder(L + 10, collar_r, collar_r, 96).translate((0, 0, -10))
    lip = Manifold.cylinder(1.5, collar_r + 1.0, collar_r + 1.0, 96).translate((0, 0, L - 1.5))
    m = m + along(collar + lip)
    m = m ^ Manifold.cube((400, 400, 200)).translate((-200, -200, 0))   # flat back
    # A groove round the collar below the lip, a chamfered mouth.
    m = m - along(Manifold.cylinder(0.8, collar_r + 1, collar_r + 1, 96).translate((0, 0, L - 4.5)) -
                  Manifold.cylinder(0.8, collar_r - 0.6, collar_r - 0.6, 96).translate((0, 0, L - 4.5)))
    m = m - along(Manifold.cylinder(1.0, bore / 2, bore / 2 + 1.0, 64).translate((0, 0, L - 1.0 + 0.001)))
    # The bore, all the way through along the collar's axis.
    m = m - along(Manifold.cylinder(L + 40, bore / 2, bore / 2, 64).translate((0, 0, -20)))
    # The back pocket, round where the bore comes out (an oval, centred on the root).
    pr = bore / 2 / math.cos(t) + POCKET_MARGIN
    m = m - Manifold.cylinder(POCKET_DEPTH, pr, pr, 64).translate((0, y0, -0.001))
    return m, hexsize


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--cable", type=float, default=6.0, help="cable diameter, mm")
    a = ap.parse_args()
    m, hexsize = port(a.cable)
    write_stl(m, os.path.join(HERE, "cable_port.stl"), "ROGUE.AI cable port")
    b = m.bounding_box()
    print("cable_port.stl: %.1f x %.1f x %.1f mm (hex %.0f mm), bore %.1f mm leaning %g deg" %
          (b[3] - b[0], b[4] - b[1], b[5] - b[2], hexsize, a.cable + 0.5, TILT))
