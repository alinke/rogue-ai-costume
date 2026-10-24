#!/usr/bin/env python3
"""Edge-lit ROGUE.AI visor: adds four LED notches to rogue_ai_visor_glasses_A.svg.

A 3 mm LED is as wide as the 3 mm acrylic is thick, so it sits INSIDE a
3.1 x 5 mm notch cut into the visor's end edge, pointing along the sheet: its
light stays in the acrylic and comes out wherever it's scored or engraved (the
lines and ROGUE.AI glow, the rest stays see-through). Four notches, at each end
above and below the strap slot (clear of the strap that wraps round the end):
the top pair shine along the top band's lines, the bottom pair along the bottom
lines and ROGUE.AI. Use two or all four.

The notches are closed shapes in the cut layer, overlapping the outline: the
laser cuts both, and the sliver between them drops out. Seen from the front a
notch is a 3.1 mm strip -- the LED lies flat inside the 3 mm sheet -- with a
rounded inner end for the LED's dome.

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
r = NOTCH_W / 2
paths = []
for y, xl, xr in NOTCHES:
    y0, y1 = y - r, y + r
    # Each notch starts outside the sheet and runs NOTCH_D inside the edge, its inner
    # end a half circle that matches the LED's dome tip (no air gap, more light in).
    xe = xl + NOTCH_D                      # left end: inner end of the notch
    paths.append(f"M{xl - 6:.2f},{y0:.2f} H{xe - r:.2f} A{r:.2f},{r:.2f} 0 0 1 {xe - r:.2f},{y1:.2f} H{xl - 6:.2f} Z")
    xs = xr - NOTCH_D                      # right end: mirror image
    paths.append(f"M{xr + 6:.2f},{y0:.2f} H{xs + r:.2f} A{r:.2f},{r:.2f} 0 0 0 {xs + r:.2f},{y1:.2f} H{xr + 6:.2f} Z")
els = "".join(f'\n    <path class="st1" d="{d}"/>' for d in paths)
s = re.sub(r'(<g id="cut"[^>]*>)', lambda m: m.group(1) + els, s, count=1)
open(OUT, "w").write(s)
print("wrote", os.path.basename(OUT), "with", len(paths), "LED notches")
