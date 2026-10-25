#!/usr/bin/env python3
"""Housing for the 8x8 WS2812B matrix on the jacket's back (the watching eye):
a sew-on base, a light grid, and a laser-cut smoke acrylic front.

  front (smoke acrylic, laser-cut, 4 holes)       back_matrix_window.svg
  grid  (printed: 8x8 cells, frame, 4 holes)     back_matrix_grid.stl
  panel (flexible 8x8, 79.67 mm, 1.72 mm)
  base  (printed: locating pocket, wire window,  back_matrix_base.stl
         wire grooves underneath, sewing flange)

Four M2.5 x 14 pan-head self-tapping screws go through the acrylic and the grid
into the base (or M2.5 x 14 machine screws and nuts on the back). The grid's
walls sit on the panel between the LEDs, press the flexible panel flat and keep
each LED's light in its own cell, so the pixels read as squares.

The panel's three leads (data in, 5V/GND, data out) come off its back in a row
24 mm from one edge (its mounting template, LEADS below). They go through the
base's three holes and out under the base in three grooves to that edge -- the
WIRE edge, marked by a notch in the flange's corner. Sew it on with the WIRE
edge toward your left: the back strip ends at your left shoulder blade and the
left sleeve starts there. (The firmware's MATRIX_ROTATION turns the picture to
match.)

Print both parts in BLACK (PETG or PLA), flat, no supports: black walls stop
light bleeding between cells and round the edge.

  python3 back_matrix.py   -> back_matrix_base.stl, back_matrix_grid.stl,
                              ../lasercut/back_matrix_window.svg
"""
import os
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold

HERE = os.path.dirname(os.path.abspath(__file__))

# ---- Your settings ----------------------------------------------------------------
PANEL = 79.67      # the matrix, square
PANEL_T = 1.72     # its thickness with the LEDs
N = 8              # LEDs per side
FIT = 0.3          # clearance round the panel
BODY = 96.0        # the housing's outer size (square)
CORNER_R = 4.0
SCREW_AT = 44.0    # screw centres at (+-SCREW_AT, +-SCREW_AT)
SCREW_CLEAR = 2.8  # through the grid and the acrylic (M2.5)
SCREW_PILOT = 2.1  # into the base (self-tapping M2.5)
BASE_T = 3.0
POCKET = 0.2       # how deep the panel sits in the base: about its PCB, so the grid presses it flat
# The panel's three leads, from its mounting template (8x8-only.svg, seen from the
# BACK; the template's square is the panel): data in (left), 5V/GND (middle),
# data out (right), all 24 mm below the panel's top edge. (x, y) mm from the
# panel's top-left corner, seen from the back.
LEADS = [(23.13, 24.01), (38.81, 24.01), (54.86, 24.01)]
LEAD_HOLE = 12.4   # the template's cut-outs: big enough for the wires and plugs
GROOVE_W, GROOVE_D = 6.0, 2.2   # wire grooves under the base, lead holes -> the WIRE edge
FLANGE = 8.0       # sewing flange round the base
SEW_HOLE, SEW_PITCH = 2.2, 8.0
GRID_H = 8.0       # grid height above the panel's PCB: LED (1.6) + ~6 mm to the acrylic
WALL = 0.8         # cell walls


def rounded_square(size, r):
    s = size - 2 * r
    return CrossSection.square((s, s)).translate((-s / 2, -s / 2)).offset(r, JoinType.Round, circular_segments=48)


def square(size):
    return CrossSection.square((size, size)).translate((-size / 2, -size / 2))


def screw_holes(d):
    return [CrossSection.circle(d / 2, 32).translate((sx * SCREW_AT, sy * SCREW_AT))
            for sx in (-1, 1) for sy in (-1, 1)]


def base():
    outer = BODY + 2 * FLANGE
    plate = rounded_square(outer, CORNER_R + FLANGE)
    for h in screw_holes(SCREW_PILOT):
        plate = plate - h
    for x, y in lead_xy():
        plate = plate - CrossSection.circle(LEAD_HOLE / 2, 48).translate((x, y))
    # Sewing holes along the flange, SEW_HOLE from... centred in the flange.
    c = BODY / 2 + FLANGE / 2
    k = int((BODY - 2 * CORNER_R) // SEW_PITCH)
    for i in range(k + 1):
        t = -((k) * SEW_PITCH) / 2 + i * SEW_PITCH
        for x, y in ((t, c), (t, -c), (c, t), (-c, t)):
            plate = plate - CrossSection.circle(SEW_HOLE / 2, 20).translate((x, y))
    m = plate.extrude(BASE_T)
    # Locating pocket for the panel.
    p = PANEL + 2 * FIT
    m = m - square(p).extrude(POCKET + 1).translate((0, 0, BASE_T - POCKET))
    # Wire grooves underneath: from each lead hole straight out through the
    # nearest (WIRE) edge's flange, so the leads lie flat between base and jacket.
    for x, y in lead_xy():
        g = CrossSection.square((GROOVE_W, outer)).translate((x - GROOVE_W / 2, y))
        m = m - g.extrude(GROOVE_D)
    # A notch in the flange marks the WIRE edge (faces your left when sewn on).
    m = m - CrossSection.circle(2.5, 24).translate((-outer / 2 + 3, outer / 2 - 3)).extrude(BASE_T)
    return m


def lead_xy():
    """Lead positions in the model (x right, y up, seen from the FRONT): the
    template is seen from the back, so x is mirrored."""
    return [(-(bx - PANEL / 2), PANEL / 2 - by) for bx, by in LEADS]


def grid():
    frame = rounded_square(BODY, CORNER_R)
    inner = PANEL + 2 * FIT
    pitch = PANEL / N
    cells = square(inner)
    walls = CrossSection()
    for i in range(1, N):
        o = -PANEL / 2 + i * pitch
        walls = walls + CrossSection.square((WALL, inner)).translate((o - WALL / 2, -inner / 2))
        walls = walls + CrossSection.square((inner, WALL)).translate((-inner / 2, o - WALL / 2))
    section = (frame - cells) + walls
    for h in screw_holes(SCREW_CLEAR):
        section = section - h
    # The grid stands on the panel's PCB; its outer frame comes down round the
    # panel onto the base (the panel sits POCKET deep, so the frame drops by
    # PANEL_T-ish -- PCB is ~0.1 mm, so the frame and the walls start together).
    return section.extrude(GRID_H)


def window_svg(path):
    """Smoke acrylic front: BODY square, rounded corners, 4 screw holes (mm)."""
    s, r = BODY, CORNER_R
    h = SCREW_CLEAR / 2
    o = s / 2
    d = (f"M{r},0 H{s - r} A{r},{r} 0 0 1 {s},{r} V{s - r} A{r},{r} 0 0 1 {s - r},{s} "
         f"H{r} A{r},{r} 0 0 1 0,{s - r} V{r} A{r},{r} 0 0 1 {r},0 Z")
    holes = "".join(f'<circle cx="{o + sx * SCREW_AT:.2f}" cy="{o + sy * SCREW_AT:.2f}" r="{h:.2f}"/>'
                    for sx in (-1, 1) for sy in (-1, 1))
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{s}mm" height="{s}mm" viewBox="0 0 {s} {s}">\n'
           f'  <!-- ROGUE.AI back matrix window: smoke acrylic, cut lines red 0.01 mm -->\n'
           f'  <g fill="none" stroke="#ff0000" stroke-width="0.01">\n'
           f'    <path d="{d}"/>\n    {holes}\n  </g>\n</svg>\n')
    open(path, "w").write(svg)


def write_stl(man, path, name):
    mesh = man.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    f = np.asarray(mesh.tri_verts)
    with open(path, "wb") as fh:
        fh.write(name.encode().ljust(80, b" "))
        fh.write(struct.pack("<I", len(f)))
        for a, b, c in f:
            n = np.cross(v[b] - v[a], v[c] - v[a])
            n = n / (np.linalg.norm(n) or 1)
            fh.write(struct.pack("<12fH", *n, *v[a], *v[b], *v[c], 0))


if __name__ == "__main__":
    for name, m in (("back_matrix_base", base()), ("back_matrix_grid", grid())):
        write_stl(m, os.path.join(HERE, name + ".stl"), "ROGUE.AI " + name)
        b = m.bounding_box()
        print("%s.stl: %.1f x %.1f x %.1f mm" % (name, b[3] - b[0], b[4] - b[1], b[5] - b[2]))
    svg = os.path.join(HERE, "..", "lasercut", "back_matrix_window.svg")
    window_svg(svg)
    print("back_matrix_window.svg: %.0f x %.0f mm" % (BODY, BODY))
    print("cell pitch %.2f mm, opening %.2f mm" % (PANEL / N, PANEL / N - WALL))
