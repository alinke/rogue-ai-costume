#!/usr/bin/env python3
"""Edge-lit ROGUE.AI visor: adds four LED notches to rogue_ai_visor_glasses_A.svg.

A 3 mm LED is as wide as the 3 mm acrylic is thick, so it sits INSIDE a
3.1 x 5 mm notch cut into the visor's end edge, pointing along the sheet: its
light stays in the acrylic and comes out wherever it's scored or engraved (the
lines and ROGUE.AI glow, the rest stays see-through). Four notches, at each end
above and below the strap slot (clear of the strap that wraps round the end):
the top pair shine along the top band's lines, the bottom pair along the bottom
lines and ROGUE.AI. Use two or all four.

The notches are plain rectangles in the cut layer, overlapping the outline:
the laser cuts both, and the sliver between them drops out.

  python3 visor_leds.py      -> rogue_ai_visor_glasses_A_leds.svg
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "rogue_ai_visor_glasses_A.svg")
OUT = os.path.join(HERE, "rogue_ai_visor_glasses_A_leds.svg")

PT = 25.4 / 72              # the file is in points
NOTCH_W = 3.1 / PT          # the LED body (3 mm) plus a little
NOTCH_D = 5.0 / PT          # how deep the LED goes in (its body; the rim stays outside)
# Notch centre lines (points): top near the scored lines at y 22 and 29, bottom
# between 104.2 and 111.3. The end edge slants there, and the LED's rim stops on
# the edge's outermost point across the notch's width, so the depth counts from
# that point (measured from the outline): (y, left-end x, right-end x).
NOTCHES = [
    (24.0, 15.83, 512.79),    # high enough to leave ~1.7 mm above the strap slot
    (107.75, 10.97, 517.60),
]

s = open(SRC).read()
rects = []
for y, xl, xr in NOTCHES:
    y0, h = y - NOTCH_W / 2, NOTCH_W
    # Each rectangle starts outside the sheet and ends NOTCH_D inside the edge.
    rects.append((xl - 6, y0, 6 + NOTCH_D, h))
    rects.append((xr - NOTCH_D, y0, NOTCH_D + 6, h))
paths = "".join(
    f'\n    <rect class="st1" x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}"/>' for x, y, w, h in rects)
s = re.sub(r'(<g id="cut"[^>]*>)', r"\1" + paths.replace("\\", "\\\\"), s, count=1)
open(OUT, "w").write(s)
print("wrote", os.path.basename(OUT), "with", len(rects), "LED notches")
