# LAB-7 researcher badge (laser-cut, edge-lit)

An ID badge for the LAB-7 researcher: 1/8" acrylic, 64 x 122 mm, on a lanyard,
with the engraving glowing cyan.

**How the glow works:** light shone into the edge of clear acrylic stays
trapped inside it; wherever the surface is engraved it scatters out. Two 3 mm
LEDs in a printed base shine up into the clear layer's bottom edge, so only the
engraved lines and lettering light up.

## Make your own

```bash
pip install fonttools pillow numpy manifold3d
python3 badge.py --name "DR. JANE DOE" --title "PRINCIPAL THREAT RESEARCHER" --photo face.jpg
```

Writes `private/` (gitignored -- it has a real name and photo). Options:
`--clearance`, `--motto`, `--humanity`, `--id`, `--holder w,h,thickness` (the
coin-cell holder, mm). Fonts are macOS's DIN and Menlo (`FONTS` in the script).

| File | What |
|---|---|
| `badge_front_clear_MIRRORED.svg` | 1/8" **clear** acrylic. Red = cut, black = engrave. **Mirrored**: engrave it on the face that will go against the photo, so the lettering reads correctly through the acrylic and the engraving is protected. |
| `badge_back_blue_outer.svg` | 1/8" **coloured** acrylic (blue). Engrave face up: that's the badge's back (the small print). |
| `badge_back_blue_inner_pocket.svg` | The same blue piece, flipped: a shallow engraved pocket (~0.3 mm, a couple of passes) for the photo, so the layers glue flat. |
| `badge_photo_print_at_100.pdf` | The photo, 46 mm square: print at 100 % (no scaling), cut on the marks. |
| `badge_led_base.stl` | 3D print: clips onto the bottom 6 mm; two 3 mm LEDs from below; a cradle for a 1 x CR2032 holder with switch. |
| `badge_preview.svg` | What it looks like lit. |

## Assemble

1. Laser: cut and engrave the clear piece; engrave the blue piece's back, flip it, engrave the pocket, cut it.
2. Photo into the pocket (a dab of glue stick), clear piece on top, engraved side down. Acrylic glue along the outer edges only (keep it off the photo window), clamp lightly.
3. LEDs (cyan, 3 mm) into the base's holes from below, rims against the underside; legs bent along the grooves; both LEDs in parallel to the holder's leads (long leg = red). One CR2032, no resistor needed for cyan/blue/white LEDs.
4. Holder into the cradle (switch down, through the slot), base onto the badge's bottom edge -- a drop of glue if it's loose.
