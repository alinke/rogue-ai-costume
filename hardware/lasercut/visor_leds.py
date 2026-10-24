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
notch is a ~3 mm strip -- the LED lies flat inside the 3 mm sheet -- with a
rounded inner end for the LED's dome.

  python3 visor_leds.py      -> rogue_ai_visor_glasses_A_leds.svg
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "rogue_ai_visor_glasses_A.svg")
OUT = os.path.join(HERE, "rogue_ai_visor_glasses_A_leds.svg")

PT = 25.4 / 72              # the file is in points
NOTCH_W = 2.8 / PT          # drawn 2.8: after the laser kerf a snug push fit for a 3 mm LED (2.9 was barely loose)
RIM_H = 4.2 / PT            # the LED's rim (3.8 mm) plus a little: the mouth is cut square this tall
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
q = RIM_H / 2
paths = []
for y, xl, xr in NOTCHES:
    # One shape per notch: a mouth as tall as the LED's rim, cut square to the LED at
    # the edge's outermost point (the slanted edge otherwise leaves a sharp point the
    # rim hits), then the channel, its inner end a half circle for the LED's dome.
    xe = xl + NOTCH_D
    paths.append(f"M{xl - 6:.2f},{y - q:.2f} H{xl:.2f} V{y - r:.2f} H{xe - r:.2f} "
                 f"A{r:.2f},{r:.2f} 0 0 1 {xe - r:.2f},{y + r:.2f} H{xl:.2f} V{y + q:.2f} H{xl - 6:.2f} Z")
    xs = xr - NOTCH_D                      # right end: mirror image
    paths.append(f"M{xr + 6:.2f},{y - q:.2f} H{xr:.2f} V{y - r:.2f} H{xs + r:.2f} "
                 f"A{r:.2f},{r:.2f} 0 0 0 {xs + r:.2f},{y + r:.2f} H{xr:.2f} V{y + q:.2f} H{xr + 6:.2f} Z")
els = "".join(f'\n    <path class="st1" d="{d}"/>' for d in paths)
s = re.sub(r'(<g id="cut"[^>]*>)', lambda m: m.group(1) + els, s, count=1)
open(OUT, "w").write(s)
print("wrote", os.path.basename(OUT), "with", len(paths), "LED notches")
