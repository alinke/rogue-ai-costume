# LAB-7 Containment Scanner

The researcher's handheld for the ROGUE.AI costume
(`costume/` in this repo). It hunts the rogue AI by its Bluetooth
signal and fires CONTAIN at it. The AI always escapes.

Boards (same firmware, same behaviour):
- **Waveshare ESP32-S3-Touch-LCD-3.49 V2** (ESP32-S3, 640x172 bar LCD, touch,
  speaker, battery) -- env `scanner`.
- **M5Stack Tab5** (ESP32-P4, 5" 1280x720 touch, speaker, battery) -- env
  `tab5`. Bluetooth goes through its ESP32-C6 co-processor (ESP-Hosted).
  Frames are drawn into a canvas and rotated onto the portrait panel by the
  P4's PPA, as on the costume. No BOOT-button CONTAIN (on-screen only), and
  the CPU stays at 360 MHz (the P4's only speed).

Display, touch and audio bring-up are copied unmodified from the Pixelcade
Sidekick firmware (`board_configs/`).

## What it does

- **Signal meter** (left): 8 bars and the signal strength in dBm, from the
  costume's Bluetooth beacon. With no signal it sweeps "SEARCHING...".
- **Sounds**: a beep-beep on TARGET LOCKED (at most every 20 s), the CONTAIN zap, the CONTAINMENT
  FAILED buzzer. **Tap the signal meter** for quiet mode (top right shows
  SOUND: ON / SOUND: QUIET; remembered across power-off): then the only sound
  is the CONTAIN button (the zap, or a short beep while it's recharging).
  Geiger clicks are built in but off (`GEIGER_CLICKS` in `config.h`).
- **Status**: NO SIGNAL -> ROGUE SIGNAL -> SIGNAL STRONG (~2 m) -> TARGET
  LOCKED (arm's length, blinking, beep-beep).
- **Intel** from the costume's beacon: its THREAT level, what the AI is doing
  (scanning a human, playing a game, midnight takeover...), humans scanned,
  times contained.
- **CONTAIN** (the red button on screen, or the BOOT button): "BEAM FIRED",
  and the costume plays its re-containment attack -- the sandbox gets to 97%,
  then the AI breaks out. The scanner follows along: CONTAINING 97% ->
  CONTAINMENT FAILED, THE AI ESCAPED. 12 s recharge between shots.

## Build and flash

Plug into the board's USB-C (native USB; shows up as `/dev/cu.usbmodem*`).

```bash
cd scanner
pio run -e scanner -t upload   # Waveshare
pio run -e tab5 -t upload      # Tab5
pio device monitor          # log, 115200
```

## Settings (`src/config.h`)

| Define | Default | |
|---|---|---|
| `NEAR_DBM` / `CLOSE_DBM` | -70 / -58 | Signal for SIGNAL STRONG / TARGET LOCKED. Keep in step with the costume's `SCANNER_NEAR_DBM` / `SCANNER_CLOSE_DBM`. |
| `HYSTERESIS_DB` | 4 | How far the signal must drop to leave a zone. |
| `LOST_MS` | 5000 | No beacon this long = NO SIGNAL. |
| `CONTAIN_COOLDOWN_S` | 12 | Recharge between shots (matches the costume). |
| `BACKLIGHT_PERCENT` / `VOLUME_PERCENT` | 50 / 60 | |
| `IDLE_DIM_PERCENT` / `IDLE_DIM_AFTER_S` | 15 / 60 | Backlight dims after a minute with no AI signal; any signal, touch or button brings it back. |
| `FRAME_MS`, `CPU_MHZ`, `SCAN_WINDOW_PERCENT` | 100, 160, 20 | Power savers: 10 redraws/s, CPU clock, Bluetooth listening duty cycle. |
| `GEIGER_CLICKS` | 0 | 1 = geiger clicks that speed up as she gets closer. |

**Tune the distances together, in costume.** Bodies absorb 2.4 GHz, so the
numbers depend on how the two of you stand and where the boards sit. Stand
at the distance you want each zone to start, read the dBm on the scanner,
and set the thresholds on both devices.

## Tab5 notes

- Held upright (portrait), camera at the top. `PORTRAIT_ROT` in config.h = 2
  if it ever needs turning round.
- **Powering off without a battery in it:** the Tab5 can't really switch off
  on USB power, and the power-off pulse it gets then leaves the speaker dead
  (no sound from any firmware, Sidekick included) until a full power cycle:
  unplug USB, power button off, wait 10 s, on. With a battery in, power-off is
  a real off and this doesn't happen.
- Speaker check: serial `b` plays the CONTAIN zap; `pio run -e tab5-audiotest`
  is a bare tone test (display bring-up + Tab5Audio only).
- `Tab5Audio.hpp` (from the Sidekick firmware) switches the Wi-Fi antenna to the
  empty external socket when it turns the amp on (it sets every bit of
  expander 0x43); `sound_impl.h` switches it back. Without that, Wi-Fi was at
  -91 dBm instead of -61.

## Humanity test (Tab5)

The **HUMANITY TEST** button (under CONTAIN) opens the camera as a live
viewfinder (mirrored, like a selfie). Tap to scan the guest: white flash, then a
**LAB-7 ID card** -- photo, SUBJECT #, AI EXPOSURE %, two test findings, barcode,
"AUTHORISED: <RESEARCHER_NAME>" and a stamp: **VERIFIED HUMAN**, or
(`COMPROMISED_PERCENT`, default 20%) **COMPROMISED** with a glitched photo. The X
at top right, or 30 s untouched (`VIEWFINDER_TIMEOUT_S`), closes it; the camera
only runs while it's open.

With the photo service set up (optional -- see the top-level README and
`../cloud/README.md`; settings in `src/secrets.h`), the cards go to the landing
page's **LAB-7 survivor registry** (verified vs compromised, and the scoreboard):
- queued on the **SD card** (`/rogue-ai/queue`), with a keepsake copy of each
  one in `/rogue-ai/photos`; no card = queued in the board's flash instead;
- sent over Wi-Fi (via the C6, next to Bluetooth) whenever a network in
  `src/secrets.h` is reachable;
- photos where Cloud Vision finds no face stay off the page (admin page: SHOW).

Camera: the Tab5's SC2356 (the core's SC202CS driver), 1280x720, auto
exposure/white balance from the ISP. Its control bus is the Tab5's internal
I2C, which LovyanGFX drives directly, so `tab5_camera_impl.h` hands the camera
driver an SCCB adapter over LovyanGFX's I2C (under the shared bus lock)
instead of a second I2C driver. 24 MHz sensor clock on GPIO36 (LEDC timer 0).
Serial (testing): `v` opens the viewfinder, `c` takes a photo, `b` plays the CONTAIN zap, `f` fires CONTAIN, `s` uploads a screenshot.

## The link

Both devices broadcast Bluetooth LE beacons (manufacturer data, company ID
0xFFFF) -- no pairing, no connection, nothing to drop out:

- Scanner: `RSC1` + trigger counter (+1 per CONTAIN) + reserved byte
- Costume: `RAI1` + threat level, activity, humans scanned, times contained

The format lives in `src/radio.cpp` here and `src/researcher.cpp` in the
costume project.

## Battery

Estimated (not measured) draw with the power settings above: roughly
100-130 mA, so a 500 mAh LiPo should give about 3.5-4 hours; a 1000-1200 mAh
cell about 7-8. To get a real number, charge fully, unplug, run it until it
dies and note the time -- or use a USB power meter once it's fully charged.
