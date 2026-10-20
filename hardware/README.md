# Hardware

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
  uses its NP-F battery.
