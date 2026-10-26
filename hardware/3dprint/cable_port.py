#!/usr/bin/env python3
"""Cable port: an Ethernet cable "plugged into" the costume -- the costume's
data link. The cable doesn't go through the jacket: its end is hot-glued into
a blind socket. Same style as grommet.py: a hexagonal bezel with glue holes at
the corners and edge midpoints (the hot glue pushes up through them and sets
like rivets), a chamfered collar with a groove ring, leaning down so the cable
hangs naturally.

Glue the flat back onto the jacket (glue round the edge, up through the holes),
then fill the socket with hot glue and push the cable end in.

Print plate down, no supports (the collar leans 35 degrees; the socket opens
upward). PETG or PLA, black.

  python3 cable_port.py                -> cable_port.stl (23 mm cable)
  python3 cable_port.py --cable 6      -> for a thinner cable (measure yours)
"""
import argparse
import math
import os
import struct

import numpy as np
from manifold3d import Manifold

from grommet import GLUE_HOLE, glue_holes, hexagon, ring_groove, write_stl

HERE = os.path.dirname(os.path.abspath(__file__))

TILT = 35.0       # degrees from straight out, leaning down
PLATE = 2.6
WALL = 2.6        # the collar's wall
SOCKET_DEPTH = 20.0
BOTTOM = 1.6      # solid floor between the socket and the plate's back


def port(cable):
    bore = cable + 0.6
    collar_r = bore / 2 + WALL
    t = math.radians(TILT)
    # The socket's floor must stay above the plate's back: its lowest point is
    # t0*cos - r*sin above the axis root.
    t0 = (BOTTOM + (bore / 2) * math.sin(t)) / math.cos(t)
    length = t0 + SOCKET_DEPTH
    y0 = length * math.sin(t) * 0.3          # root a little above centre: the collar leans over the middle
    # Where the leaning collar meets the plate: an oval, stretched along y.
    ry = collar_r / math.cos(t)
    hexsize = 2 * (abs(y0) + ry) + 10        # the oval plus a rim for the glue holes
    m = hexagon(hexsize, r=2.0).extrude(PLATE)
    for x, y in glue_holes(hexsize, 3.0):
        if (x / (collar_r + 3)) ** 2 + ((y - y0) / (ry + 3)) ** 2 < 1:
            continue                         # under the collar: no glue could come up there
        m = m - Manifold.cylinder(PLATE + 2, GLUE_HOLE / 2, GLUE_HOLE / 2, 24).translate((x, y, -1))
    def along(man):                          # z-axis part -> tilted collar frame
        return man.rotate((TILT, 0, 0)).translate((0, y0, 0))
    collar = Manifold.cylinder(length + 6, collar_r, collar_r, 96).translate((0, 0, -6))
    lip = Manifold.cylinder(1.6, collar_r + 1.0, collar_r + 1.0, 96).translate((0, 0, length - 1.6))
    m = m + along(collar + lip)
    m = m ^ Manifold.cube((400, 400, 200)).translate((-200, -200, 0))   # flat back
    socket = Manifold.cylinder(SOCKET_DEPTH + 5, bore / 2, bore / 2, 96).translate((0, 0, t0))
    chamfer = Manifold.cylinder(1.5, bore / 2, bore / 2 + 1.5, 96).translate((0, 0, length - 1.5 + 0.001))
    groove = ring_groove(collar_r - 0.9, depth=0.5, w=0.5).translate((0, 0, length - 0.5 + 0.001))
    m = m - along(socket) - along(chamfer)
    m = m - along(Manifold.cylinder(0.8, collar_r + 1.2, collar_r + 1.2, 96).translate((0, 0, length - 4.5))
                  - Manifold.cylinder(0.8, collar_r - 0.4, collar_r - 0.4, 96).translate((0, 0, length - 4.5)))
    m = m - along(groove)
    return m, hexsize


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--cable", type=float, default=23.0, help="cable diameter, mm")
    a = ap.parse_args()
    m, hexsize = port(a.cable)
    write_stl(m, os.path.join(HERE, "cable_port.stl"), "ROGUE.AI cable port")
    b = m.bounding_box()
    print("cable_port.stl: %.1f x %.1f x %.1f mm (hex %.0f mm), socket %.1f mm x %.0f mm deep" %
          (b[3] - b[0], b[4] - b[1], b[5] - b[2], hexsize, a.cable + 0.6, SOCKET_DEPTH))
