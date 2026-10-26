#!/usr/bin/env python3
"""Cable port: an Ethernet cable "plugged into" the costume -- the costume's
data link. Same style as grommet.py: a hexagonal bezel with glue holes at the
corners and edge midpoints, and a raised collar with a groove ring and a lip.

The cable doesn't go through the jacket. It goes in through the collar from
the front, its end stops in a wider pocket in the port's back, and you fill
that pocket with hot glue from the back -- the glue sets round the cable's end
and locks it in. Then glue the port's back onto the jacket (a bead round the
edge, pressed so it comes up through the glue holes).

  1  push the cable in from the front until its end is ~2 mm inside the back
  2  turn it over, fill the back pocket with hot glue, let it set
  3  glue the back onto the jacket

Print back down (the collar up), no supports. PETG or PLA, black.

  python3 cable_port.py              -> cable_port.stl (23 mm cable)
  python3 cable_port.py --cable 6    -> for another cable (measure yours)
"""
import argparse
import os

from manifold3d import Manifold

from grommet import GLUE_HOLE, glue_holes, hexagon, ring_groove, write_stl

HERE = os.path.dirname(os.path.abspath(__file__))

FLANGE = 3.0      # the hex plate (glue holes)
BODY = 14.0       # the collar's height above the plate: the cable's grip
WALL = 4.5        # collar wall
POCKET_EXTRA = 5.0  # the back pocket: this much wider than the cable...
POCKET_DEPTH = 5.0  # ...and this deep, for the glue round the cable's end
RIM = 9.0         # flange round the collar, for the glue holes


def port(cable):
    bore = cable + 0.6
    collar_r = bore / 2 + WALL
    hexsize = 2 * collar_r + 2 * RIM
    m = hexagon(hexsize, r=2.0).extrude(FLANGE)
    for x, y in glue_holes(hexsize, 3.2):
        m = m - Manifold.cylinder(FLANGE + 2, GLUE_HOLE / 2, GLUE_HOLE / 2, 24).translate((x, y, -1))
    top = FLANGE + BODY
    m = m + Manifold.cylinder(top, collar_r, collar_r, 128)
    m = m + Manifold.cylinder(1.8, collar_r + 1.2, collar_r + 1.2, 128).translate((0, 0, top - 1.8))   # lip
    # A groove round the collar's side, below the lip.
    m = m - (Manifold.cylinder(1.0, collar_r + 1, collar_r + 1, 128) -
             Manifold.cylinder(1.0, collar_r - 0.7, collar_r - 0.7, 128)).translate((0, 0, top - 5.5))
    # The bore, a chamfered mouth, the groove ring on the top face.
    m = m - Manifold.cylinder(top + 2, bore / 2, bore / 2, 128).translate((0, 0, -1))
    m = m - Manifold.cylinder(1.5, bore / 2, bore / 2 + 1.5, 128).translate((0, 0, top - 1.5 + 0.001))
    m = m - ring_groove(bore / 2 + 2.2, depth=0.5, w=0.6).translate((0, 0, top - 0.5 + 0.001))
    # The back pocket the cable's end is glued into (z = 0 is the back).
    m = m - Manifold.cylinder(POCKET_DEPTH, (bore + POCKET_EXTRA) / 2, (bore + POCKET_EXTRA) / 2, 128).translate(
        (0, 0, -0.001))
    return m, hexsize


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--cable", type=float, default=23.0, help="cable diameter, mm")
    a = ap.parse_args()
    m, hexsize = port(a.cable)
    write_stl(m, os.path.join(HERE, "cable_port.stl"), "ROGUE.AI cable port")
    b = m.bounding_box()
    print("cable_port.stl: %.1f x %.1f x %.1f mm (hex %.0f mm); bore %.1f mm, back pocket %.1f mm x %.0f mm" %
          (b[3] - b[0], b[4] - b[1], b[5] - b[2], hexsize, a.cable + 0.6, a.cable + 0.6 + POCKET_EXTRA, POCKET_DEPTH))
