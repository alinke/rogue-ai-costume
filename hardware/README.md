# Hardware

![Costume concept with every part called out: laser-cut visor, LED channels, the printed mount and chest terminal, eye pod, pauldron, kill switch, tether, asset tag](../docs/images/costume-concept.png)

## 3D print (`3dprint/`)

The display mount, printed, with the Guition 10.1" (V2, `JC8012P4A1C_I_W_Y1`) in it.
The four slots at the corners take straps; the screen faces out and the board's
back stays reachable (USB ports, SD card, the UART/LED connector).

<table>
<tr>
<td><img src="../docs/images/mount-front.jpg" alt="The 3D-printed mount from the front, display running"></td>
<td><img src="../docs/images/mount-back.jpg" alt="The mount from the back: the Guition board's back panel and label, strap slots, LED wiring"></td>
<td><img src="../docs/images/mount-back-angle.jpg" alt="The mount from the back at an angle"></td>
</tr>
<tr>
<td align="center">Front</td>
<td align="center">Back</td>
<td align="center">Back, angled</td>
</tr>
</table>

| File | What |
|---|---|
| `guition10_mount.stl` | Mount for the Guition 10.1" JC8012P4A1C display on the costume's chest. `guition10_mount_drawing.ai`: the source drawing. |
| `led_clips.scad` | Sewable clips for WS2812 LED strips (OpenSCAD, parametric — measure your strip and set `strip_w` / `strip_t`; pick a `part`, render, export). |
| `channel_60mm.stl`, `channel_30mm.stl` | Straight strip channels, 60 / 30 mm (from `led_clips.scad`). |
| `channel_60mm_endcap.stl` | Channel with a closed end. |
| `splice_25mm.stl` | Joins two strip ends. |
| `saddle_clip.stl` | Saddle clip for crossing a seam. |

### ARM-7 armband mount (2.4" CYD + 2000 mAh battery, forearm)

![Armband mount renders: matte black bezel and battery pod, green circuit-trace inlay, clear hex status jewel over the LED, ARM-7 engraved on the side](../docs/images/armband-mount.png)

*Renders, not yet printed.* Angular like the visor, black like the techwear,
with green circuit traces like the costume's light channels.

| File | Print in | What |
|---|---|---|
| `armband_bezel.stl` | matte black PETG | Front: frames the screen, holds the board on its four M3 holes, LED and light-sensor windows, cut-outs for USB-C and the SD card, sloped flanges for the pod's screws. |
| `armband_bezel_inlay.stl` | translucent neon-green PETG | The circuit traces, flush in the bezel's front (3 layers). |
| `armband_jewel.stl` | clear / natural PETG | The "status jewel": a faceted hex cap with a light pipe down to the RGB LED. |
| `armband_pod.stl` | matte black PETG | Back: battery compartment, power-switch slot, a saddle curved to the forearm (330 mm round, over a jacket), strap slots for a 25 mm strap at both ends, engraved traces and ARM-7 on the sides. |
| `armband_mount.py` | | The source (Python + `manifold3d`): arm size, strap width, battery size, fit and the trace layout at the top; `python3 armband_mount.py` rewrites the STLs. |

Fitted to the board's own 3D model and checked for collisions with it; about
116 x 49 mm, 26 mm tall at the centre. The arm runs along the long side.

**Colours.** With a multi-colour printer (AMS): load `armband_bezel.stl` and
`armband_bezel_inlay.stl` together as one object (Bambu Studio / Orca: add the
inlay as a part), black + green; the colour change is only in the first 3
layers, so very little purge. Single colour: print the bezel black and fill the
trace grooves with a green paint pen, or print the bezel in the green and keep
the pod black. The jewel is its own small print in clear.

**You also need:** 4x M3x6 screws (board into bezel), 4x M3x10 (pod onto
bezel, from underneath), a 3.7 V LiPo (2000 mAh, 58 x 38 x 9 mm) with a
**JST 1.25 mm 2-pin plug** (check polarity against the board's silkscreen
before plugging in -- these plugs come both ways round), a 3-pin mini slide
switch (11.55 x 4 x 6 mm body, lever 1.84 mm; other sizes: `SWITCH` in the script), a 25 mm strap (hook-and-loop or elastic), foam tape.

**Print:** PETG (or PLA), 0.2 mm, 3 walls. Bezel front-face down, no supports.
Pod upside down (open side on the bed) with supports for the floor. Jewel cap
down, no supports; press it into the LED window (a drop of clear glue).

**Assemble:** board screen-down into the bezel, four M3x6 screws through the
board into the posts. Cut the battery's red wire and solder the slide switch in
(middle pin + one outer pin; insulate the joints). Push it down into its
cradle on the inside of the side wall (pins toward the middle, lever into the
slot) until it clicks under the two small catches; a dab of hot glue or epoxy
on top makes it permanent. The lever ends level with the outside, in a finger
recess. Battery into the
pod on the other side of the low rib, held with foam tape; plug it into the
board's BAT connector (the one next to the speaker connector on the top edge).
Pod onto the bezel, four M3x10 screws from underneath. Strap through the slots.

**Battery gauge (optional):** two 100 kΩ resistors from the switched battery +
to GND, the middle to GPIO35 on the board's P3 connector -- see
[armband/README.md](../armband/README.md). They fit in the pod next to the
switch; heat-shrink them.

**Charging:** the board charges the battery itself through its USB-C port (it
has a power-bank chip, U8 in the schematic), with the switch **on**. The switch
off = fully off, no standby drain.

The LED parts are made to print in **TPU 95A**, flat, no supports, and get
sewn on through their flange holes.

## Laser cut (`lasercut/`)

The ROGUE.AI visor goggles, in two sizes for different glasses:
`rogue_ai_visor_glasses_A.svg` (+ `.ai` source) and `rogue_ai_visor_glasses_B.ai`.

## Wiring

- **LED strips:** one chained run of WS2812 strips on the Guition's UART
  connector **TX pin (GPIO37)** + 5 V + GND. Enable and lay out the chain in
  `costume/src/config.h` (`LED_STRIP_ENABLED`, `LED_CHAIN`). See
  `costume/README.md`, "LED strip".
- **Battery:** the Guition has a battery connector (IP5306 charger); the
  Waveshare scanner takes a 3.7 V LiPo on its MX1.25 2-pin connector; the Tab5
  uses its NP-F battery. The ARM-7 armband's 2.4" CYD charges a 3.7 V LiPo on
  its JST 1.25 mm 2-pin BAT connector (power-bank chip on board).
