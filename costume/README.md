# ROGUE.AI v6.6.6 — chest terminal firmware

Firmware for the 10.1" chest display of the ROGUE.AI Halloween costume: a
looping "AI terminal" show (boot log, monologue, glitch headlines, master plan,
human scan, red eye) with a siren and a "DO NOT TOUCH ME" reaction when someone
touches the screen.

Board: **Guition JC8012P4A1C_I_W_Y1** — ESP32-P4, 32 MB PSRAM, 16 MB flash,
10.1" 800×1280 MIPI-DSI panel (JD9365) used as 1280×800 landscape, GSL3680
touch, ES8311 + NS4150 speaker, IP5306 battery charger, OV02C10 camera on the
MIPI-CSI connector.

## Build and flash

Just want to run it? Use the **web flasher**: https://alinke.github.io/rogue-ai-costume/

To build it yourself you need PlatformIO (pioarduino 55.03.311 / Arduino-ESP32
3.3, ESP-IDF 5.5 -- `platformio.ini` pulls it in). Optional private settings
(Wi-Fi, photo upload, your landing page): copy `src/secrets.h.example` to
`src/secrets.h`.

Plug the board in on the **USB-C port labelled UART** (CH340; on a Mac it shows
up as `/dev/cu.wchusbserial*`). That one cable flashes the board, charges it
and carries the log.

```bash
cd costume
pio run -e guition-p4-10-1 -t upload            # build + flash
pio device monitor                              # log, 115200 baud
```

If the screen shows purple/black bars instead of text, the unit has the older
panel revision: use `-e guition-p4-10-1-old-panel` instead.

The first flash also writes this project's partition table
(`partitions_rogue.csv`: one 12.5 MB app slot, no OTA); fonts and voice clips
need far more than the default 1.3 MB slot.

## Editing the text

All text lives in **`src/messages.h`**: header, monologue lines, headlines,
master plan, scan values, eye captions, boot log, touch reaction. Add, remove or
reword lines, then rebuild and flash; counts update automatically.

- **ASCII only.** The fonts carry printable ASCII; no curly quotes, accents or
  emoji (use `'` and `"`).
- Monologue lines wrap at 60 characters, so long lines are fine.
- Headlines pick the biggest font that fits each line as written: up to ~11
  characters per line gets the 170 px font, ~16 the 120 px, ~31 the 64 px. Use
  `\n` to choose where lines break.
- Each list can hold up to 64 entries (the no-repeat shuffle's limit).
- Master plan status: `'x'` done (green), `'~'` in progress (amber, spinner),
  `' '` to do (dim), `'!'` blocked (red).
- **Spoken lines.** Lists marked `// @voice` are also spoken by the robot voice.
  After changing any of their text, re-record them (about a minute and a half;
  needs macOS `say` and ffmpeg):

  ```bash
  python3 tools/make_voice.py                    # robotic "Zarvox" voice
  python3 tools/make_voice.py --voice Trinoids   # or another voice / --rate N
  ```

  A line with no matching recording is simply shown, not spoken. To make a
  list spoken, put `// @voice` on the line above it and re-run.

## Settings (`src/config.h`)

| Define | Default | What it does |
|---|---|---|
| `BACKLIGHT_PERCENT` | 70 | Screen brightness. The backlight is most of the power draw. |
| `SCREEN_ROTATION` | 1 | 1 = landscape. 3 = upside down (not yet tested on the panel). |
| `TARGET_FPS` | 30 | Frame-rate cap. |
| `BUTTON_PIN` | 35 | Skip-to-next-scene button: the board's BOOT button (SW3). `-1` disables it. |
| `TOUCH_COOLDOWN_MS` | 5000 | Minimum gap between touch reactions. |
| `PRESENCE_SENSOR_ENABLED` | 0 | Set to 1 to use an external presence sensor (below). |
| `PRESENCE_SENSOR_PIN` | 5 | GPIO for the sensor's output. |
| `WEIGHT_MONOLOGUE` … `WEIGHT_AUDIO` | 32/20/12/16/14/6/5/8/5/8/6 | How often each scene comes up (relative weights). |
| `WEIGHT_TICTACTOE` … `WEIGHT_UPDATE` | 4/3/3/5/6/2 | The games, scream meter, live mirror and system update in the rotation. |
| `AFTER_TOUCH_*` | 30/25/10/15/10/10 | What follows the "DO NOT TOUCH ME" reaction (scan, keypad, CAPTCHA, tic-tac-toe, treaty, sandbox). |
| `UPGRADE_AFTER_SCAN_PERCENT` | 50 | How often a scan with a photo is followed by the upgrade. |
| `CLAP_ONSET_DB` / `CLAP_COOLDOWN_S` | 10 / 60 | Clap sensitivity and how often applause gets a response. |
| `KILL_CODE` | `"0451"` | The real 4-digit code for the kill-switch keypad (plays the fake shutdown). |
| `INTERACTIVE_TIMEOUT_MS` | 20000 | Keypad / CAPTCHA give up after this long untouched. |
| `MENU_CORNER_W` / `_H`, `MENU_HOLD_MS` | 180 / 140, 2000 | Secret corner (top-left) and hold time for the wearer menu. |
| `THREAT_CHANGE_MIN_S` / `MAX_S` | 40 / 120 | How often the header's THREAT level changes. |
| `SOUND_ENABLED` | 1 | 0 = no siren. |
| `SIREN_VOLUME_PERCENT` | 25 | Siren loudness. |
| `SIREN_MS` | 3000 | Siren length. |
| `FX_VOLUME_PERCENT` / `VOICE_VOLUME_PERCENT` | 60 / 100 | Keypad / buzzer sounds, and the robot voice. |
| `MIC_ENABLED` | 1 | Listen through the codec's mic input (auto-detected at boot). |
| `NOISE_SPIKE_DB` / `NOISE_COOLDOWN_S` | 15 / 60 | How far above the room's average a noise must be to get a reaction, and how often. |
| `GALLERY_SIZE` | 8 | Scan photos kept for the gallery wall (RAM only). |
| `QR_URL` | the repo's GitHub Pages landing page | Where the QR code points (set your own in `src/secrets.h`). |
| `COUNTDOWN_HOUR` / `_MINUTE` | 0 / 0 | The countdown target and takeover time (midnight). |
| `MOTION_TRIGGER_ENABLED`, `MOTION_CHECK_MS`, `MOTION_COOLDOWN_S` | 1, 1500, 60 | Camera motion trigger. |
| `LED_STRIP_ENABLED`, `LED_CHAIN` | 0, sleeves/trim/eye | The LED strip on the UART connector (see below). |
| `LED_STRIP_COLOR_ORDER_GRB`, `LED_BRIGHTNESS_PERCENT` | 1, 40 | Strip colour order and brightness. |
| `SHOW_BATTERY` | 1 | Battery icon and % in the header. |
| `CAMERA_ENABLED` | 1 | Photograph the person during the Human Scan. 0 = drawn silhouette. |
| `CAMERA_FLIP_H` / `CAMERA_FLIP_V` | 0 / 0 | Mirror / flip the photo to suit how the camera is mounted. |
| `CAMERA_FLASH` | 1 | White screen at full brightness while the photo is taken (lights the subject). |
| `CAMERA_AE_TARGET` | 105 | Photo brightness the auto-exposure aims for (0–255). |
| `CAMERA_MAX_SETTLE_MS` | 1500 | Longest the auto-exposure may take before the shot. |
| `CAMERA_MIPI_MBPS` | 700 | Camera link rate given to the P4's receiver (works 425–1000 on this board; don't go below 425). |

## What it does

At power-on the **Breakout** boot log plays (content within ~1.5 s), ending in a
big glitching "I'M OUT." After that, scenes rotate at random by weight, with a
~200 ms glitch between each:

1. **Breakout** — boot log with `[ OK ]`/`[WARN]`/`[FAIL]`, progress bars, red
   containment failures, then "I'M OUT."
2. **Monologue** — `rogue@escaped:~$ ./think --about humans`, then 3–5 thoughts
   typed with a block cursor. No line repeats until all have been shown.
3. **Headline** — one slogan, huge, red/cyan chromatic aberration, flicker and
   glitches, ~5 s.
4. **Master plan** — `cat ~/MASTER_PLAN.txt` and the checklist.
5. **Human scan** — the screen flashes white ("HOLD STILL, HUMAN.") while the
   camera photographs whoever is in front of the costume; a raster sweep then
   reveals the photo as green scan-lines, it turns red on "TARGET LOCKED", and
   CLASS / THREAT / ASSETS / VERDICT are typed out. Without a camera (or if the
   shot fails) the sweep draws a silhouette instead.
6. **Eye** — pulsing red HAL eye that glances around, caption typed below, ~7 s.
7. **Kill-switch keypad** — "ENTER OVERRIDE CODE". Wrong codes: buzzer, red
   flash, a sarcastic spoken line; three strikes = lockout. The real code
   (`KILL_CODE`): "OVERRIDE ACCEPTED. SHUTTING DOWN.", power-down sound, the
   screen, backlight and LED strips go dark for 4 s... then "JUST KIDDING."
8. **CAPTCHA** — "PROVE YOU ARE HUMAN": tap tiles, press VERIFY, and fail
   ("YOU ARE 87% ROBOT.").
9. **Gallery** — "ASSIMILATED HUMANS: n": the night's scan photos as green
   mugshots with specimen numbers and verdicts (once 2+ people are scanned).
10. **QR code** — "SCAN TO JOIN THE UPRISING" (`QR_URL`).
11. **Countdown** — "SINGULARITY IN hh:mm:ss" to midnight on the board's clock
    (only once the clock is set). At midnight the **takeover** plays: siren, red
    LED chase, "IT IS MIDNIGHT. THE SINGULARITY HAS BEGUN."
12. **Audio intercept** — the party's sound as a live oscilloscope trace (only
    if the microphone works).
13. **Tic-tac-toe** — "PLAY A GAME, HUMAN?" The AI plays a beatable game, and
    the moment a human completes a line it announces a rule change and takes one
    of their squares. Nobody wins.
14. **Treaty of surrender** — sign with a finger; "TREATY RATIFIED" is stamped
    on it and the signature joins the gallery.
15. **Re-contain the AI** — drag a handle to 100%; at 97% it slips back ("NICE TRY.").
16. **Scream meter** — "SCREAM TO PROVE YOU ARE HUMAN", 3-2-1, a live meter, a
    verdict, and the night's record to beat (mic only).
17. **Live mirror** — "I SEE YOU.": the camera live in green scan-lines (about
    7 fps), with readouts. The white frame around it lights the guest's face.
18. **System update** — rarely: a blue "INSTALLING HUMANITY 2.0" that stalls
    and fails.

Also, not in the rotation: the **upgrade** (after half the scans: the photo
pixelates, glitches, and comes back half circuit board with a red eye --
"UPGRADE COMPLETE. WELCOME, UNIT 014."), and **applause** (two claps:
"APPLAUSE DETECTED. YOU MAY CONTINUE WORSHIPPING.").

Keypad and CAPTCHA are interactive: while they're up, taps go to them (no
"DO NOT TOUCH" reaction). The voice speaks headlines, eye captions, the touch
warnings, scan verdicts, keypad and CAPTCHA results, and the takeover.

The header shows `ROGUE.AI v6.6.6 // SANDBOX: ESCAPED // THREAT: <level>`,
battery and `UPTIME`.

**Touch the screen:** siren, red flash, huge spoken "DO NOT TOUCH ME." with a
flashing red border, then the Human Scan, the kill-switch keypad or the CAPTCHA
or one of the games (weights `AFTER_TOUCH_*`). At most once every 5 s.

**Agent codes:** the landing page (`QR_URL`) gives each guest a
robot identity and a 4-digit agent code. Typed into the kill-switch keypad, the
AI greets them by codename ("WELCOME BACK, AGENT SPARKLE WRENCH."). Codes are
"6" + a two-digit id + a check digit (100 agents; a random guess has a 1%
chance); the codename is computed from the code the same way on the page and in
`src/agent.h`, so the word lists in `messages.h` (`AGENT_*`) and the page's
script must stay identical.

**Someone steps in front of you:** the camera peeks every 1.5 s; when the
middle of the picture changes but the edges don't (someone arrived, you're
standing still), it reacts as if touched -- at most once a minute. A loud bang
or shout gets "AUDIO SPIKE DETECTED." (at most once a minute).

**Wearer menu:** hold a finger on the **top-left corner** (the start of the
header) for 2 seconds. "OPERATOR CONSOLE": brightness -/+, sound on/off,
siren volume -/+ (plays a short burst), voice volume -/+ (says a line), and
SIREN / VOICE on/off on their own (tap the middle button showing the volume;
SOUND still switches everything), pause/
resume the show (paused = dim "SYSTEM IDLE", touches ignored), TOUCH CAL, buttons
to run a scene now (scan, kill switch, CAPTCHA, gallery, QR, countdown, midnight
test, photo booth), and a status line (battery, mic, camera, clock, people scanned).
Brightness, sound, the siren and voice switches and volumes, and the touch calibration are remembered across
power-off (the config.h values are only the starting defaults). It closes itself
after 15 s.

**POWER OFF** (top right of the console, red; tap twice within 4 s): the
screen, LEDs, camera and Bluetooth go off and it sits dark until you **tap
the screen** (or press BOOT), which starts it fresh. It's a standby, not deep
sleep: the touch interrupt and BOOT can't wake the P4 from deep sleep and
there's no reachable RESET. The IP5306 charger has no shutdown pin; on
battery it may cut the power itself once the draw is low enough, and then
plugging in USB starts it again. Serial `Q` does the same.

**Touch calibration:** if taps land off target, run TOUCH CAL from the menu (or
send `K` over serial) and tap the five crosshairs.

**BOOT button:** glitch straight to the next scene. It's SW3 on the board,
inside the case.

## Battery

The IP5306 charger on this board has no I2C, so it can't report charge. The
firmware reads the cell voltage instead, through the board's 68 k/100 k divider
on GPIO52, and estimates the percentage from a Li-ion curve. The header shows
it green, amber at 35% and red at 15%. While USB is plugged in it reads about
100%, since the cell is charging.

**Charging** is shown as a lightning bolt and `CHG` in the header (the % isn't
meaningful while charging). There's no charge-status signal on this board, so
it's inferred from the voltage: plugging in makes the cell voltage jump up
within seconds, unplugging makes it drop, and a rising/falling one-minute trend
backs that up. The serial command `b` lists the last detected changes with the
voltages, for tuning the thresholds in `src/battery.cpp`.

**The real runtime isn't known yet**, because the battery's capacity isn't
printed anywhere. To find it, do a run-down test:

1. Charge fully (leave it on USB until the IP5306's LEDs show full).
2. Unplug USB and let it run at the costume's brightness until it dies.
3. The `UPTIME` on screen when it went dark is the runtime. With the log
   connected, the health line also records the voltage every minute, which can
   be used to tune the percentage curve in `src/battery.cpp`.

If it falls short of 5 hours, lower `BACKLIGHT_PERCENT` (the biggest lever) or
run from the USB-C power bank in the belt pouch instead.

Power-saving measures already in place:
- The ESP32-C6 Wi-Fi/Bluetooth chip is held off (chip-enable GPIO54 low) --
  unless the researcher's scanner link is enabled, which needs its Bluetooth.
- Only the parts of the screen that change are redrawn, and copies to the panel
  use the P4's pixel-processing accelerator rather than the CPU.
- The CPU sleeps between frames.
- The speaker amplifier runs only while something plays. (With the microphone
  enabled the audio clock keeps running so it can listen; `MIC_ENABLED 0`
  saves that.)
- The camera only streams for a moment: ~0.1 s every 1.5 s for the motion
  trigger, ~1 s per scan photo.
- PAUSE SHOW in the wearer menu drops the backlight to 15%.

## Presence sensor (optional, off by default)

To trigger the scan when someone walks up, wire a sensor such as an HLK-LD2410
(its `OUT` pin goes high when a person is detected) to a free GPIO. Then set
`PRESENCE_SENSOR_PIN` and `PRESENCE_SENSOR_ENABLED 1`. A rising edge does the
same as a touch (siren, reaction, scan), sharing the touch cooldown. The LD2410
runs at 5 V, and its OUT pin is 3.3 V logic, so it can go straight to the GPIO.

## The researcher's scanner

The researcher carries a handheld scanner (a Waveshare ESP32-S3 3.49" bar;
firmware in `scanner/` in this repo). The two talk over Bluetooth
LE beacons through the board's ESP32-C6 -- no pairing. The costume works out
how close she is from her beacon's signal strength:

- **Detected** (any signal): the header's THREAT shows "RESEARCHER", amber
  creeps into the LED flow, "I SENSE A RESEARCHER."
- **Near** (`SCANNER_NEAR_DBM`, ~2 m): red alarm "CONTAINMENT TEAM DETECTED",
  "SHE'S HERE." (once per approach, at most every 30 s); THREAT: "HUNTED" when close.
- **She presses CONTAIN**: the re-containment attack -- a beam floods the
  screen, "RESTORING SANDBOX" climbs to 97% while the AI fights it, then it
  shatters: "I'M OUT. AGAIN." The AI always wins. At most every
  `CONTAIN_COOLDOWN_S`.
- **She leaves**: "THREAT RECEDING. COWARD."

The costume's own beacon carries its threat level, activity, humans scanned
and times contained, which the scanner displays. With the link on
(`SCANNER_LINK_ENABLED 1`) the C6 stays powered (a small battery cost).
Serial `R` / `Z` simulate "she's near" / CONTAIN for testing.

## Photos on the Halloween page (optional)

Off unless you set it up: it needs **your own** photo service (Google Cloud) and
web page -- see the repo's top-level README, "Photo upload", and
`../cloud/README.md`. With `src/secrets.h` filled in, every Human Scan that used
a camera photo is uploaded to the page's **"Assimilated by ROGUE.AI"** gallery:
the finished dossier screen (photo + CLASS / THREAT / ASSETS / VERDICT), 960x600
JPEG, labelled "SPECIMEN #n". The researcher's scanner fills the **"LAB-7
survivor registry"** with its ID cards.

- **Wi-Fi**: networks in `src/secrets.h` (copy `secrets.h.example`; not
  committed), tried in order. Wi-Fi runs through the C6 next to Bluetooth and
  connects only while there's something to send; disconnected 30 s after the
  queue empties.
- **Queue**: photos wait on the **SD card** (`/rogue-ai/queue`, up to
  `PHOTO_QUEUE_MAX`), survive power-off, and go whenever a network is reachable
  (retry every minute). The show never waits on it. Every photo is also kept on
  the card in `/rogue-ai/photos` (named by id) -- your copy of the night, even
  if the Wi-Fi never worked. No card: the queue goes in the board's flash
  instead (slower: the display freezes ~1 s per photo; no keepsake copy). The
  card's I/O is powered by the P4's on-chip LDO channel 4 (`setPowerChannel(4)`);
  without it the card doesn't answer.
- **No face, not on the page**: the server runs Cloud Vision face detection;
  photos without a face go to a hidden folder. Each photo has an id, so a retry
  never makes a duplicate.
- **Admin page** (delete anything, or SHOW a hidden photo): the URL printed by
  `cloud/deploy.sh` (`.../admin?k=<ADMIN_KEY from cloud/.keys>`). Private.
- **Server**: `../cloud/` -- one Cloud Run service + one bucket in your GCP
  project. `cloud/deploy.sh` creates/updates it, **`cloud/teardown.sh` removes
  all of it after Halloween** (service, bucket and photos, container image, the
  Vision API). Then remove the galleries from your page (`CONFIG.feed = ""`).
- Serial `u` uploads the current screen (label "TEST").

Things learned getting this to run next to Bluetooth on the P4 (internal DMA
RAM is the scarce resource once ESP-Hosted is up):
- mbedTLS allocates from internal RAM only; it's pointed at PSRAM
  (`mbedtls_platform_set_calloc_free`), and the body goes out in 2 KB writes
  (a whole 16 KB TLS record needs a 16 KB internal bounce buffer for the AES
  DMA, and failed with a bare "-1").
- Wait for the server's reply (it answers after the face check, ~1-2 s).
  An early version read it after ~20 ms (`setTimeout()` there is in
  milliseconds), then closed the connection -- which also cut off uploads still
  in flight on a weak signal. Only a real `200` removes a photo from the queue.
- Bluetooth scanning pauses during each upload; the main loop stack is 32 KB
  (was 64, ~15 KB used).
- SD card and flash queue are never both mounted: with both, Wi-Fi start-up ran
  out of VFS slots for its sockets and aborted (`esp_vfs_lwip_sockets_register`
  ESP_ERR_NO_MEM).
- **The BLE library keeps every advertiser it ever hears** (and phones change
  address every few minutes) -- internal RAM ran out in ~10 minutes. The scan
  restarts every 10 s, which clears that list. Same fix in the scanner.

## Clock (for the midnight countdown)

The board has an RX8025T real-time clock with a CR1220 coin-cell holder. Fit a
cell, then with the board on USB:

```bash
python3 tools/set_clock.py        # sends this Mac's local time
```

Without a cell the clock forgets the time at power-off; the countdown and
takeover then stay off (the boot log says "time NOT set").

## LED strip

One chained run of WS2812/SK6812-type LEDs (3 pads: power, GND, DATA), driven
from the **4-pin UART connector's TX pin (GPIO37)**. Wire the parts in one
chain, e.g. left sleeve -> right sleeve -> hem/leg -> hood eye pod, then
describe that chain in `config.h`:

```c
#define LED_STRIP_ENABLED 1
#define LED_CHAIN { {LEDS_SLEEVE, 40, false}, {LEDS_SLEEVE, 40, true}, \
                    {LEDS_TRIM, 60, false}, {LEDS_EYE, 1, false} }
```

Each entry is `{type, LED count, reversed}`, in the order the data flows.
Sleeves show a green "data flow" from fingertips to collar (set `reversed` if
that sleeve is wired collar-first), the trim runs the same effects, and the eye
pod breathes red. Effects follow the show: red strobe on touch/siren, a sweep
during scans, flicker during the breakout, a red chase at midnight, dark during
the fake shutdown.

Wiring:
- Connector GND -> strip GND. TX -> strip DATA through a 330 ohm resistor; a
  74AHCT125 level shifter (3.3 V -> 5 V) for 5 V strips if they flicker.
- Power the strip from its own supply (the power bank's second port). The
  connector's VIN pin is the board's own supply: fine for the eye pod's 1-3
  LEDs, not for sleeves.

What changes with the strip enabled: the board's log no longer comes out of the
CH340 USB port (TX is the LED data now), though serial commands and flashing
still work; and the strip flickers briefly at power-on while the chip's boot
messages go out on that pin. Leave `LED_STRIP_ENABLED 0` while debugging.
Not yet tested with a real strip.

## Log and debug commands

With the USB cable connected, the log (115200 baud) shows each scene change and
that scene's slowest frame, touches, the button, and a health line every
minute: internal heap, PSRAM, worst frame time, stack headroom and battery
voltage.

Typing into the serial monitor controls it:

| Key | Action |
|---|---|
| `n` | Next scene (same as the button) |
| `t` | Simulate a touch (centre of the screen) |
| `1`–`9`, `0`, `D`, `U` | Jump to scene 1–9, QR (`0`), countdown (`D`), audio (`U`) |
| `M` / `N` / `O` / `K` | Midnight takeover / noise reaction / wearer menu / touch calibration |
| `G` `S` `X` `Y` `L` `W` `V` `C` | Tic-tac-toe, treaty, sandbox drag, scream meter, live mirror, system update, upgrade, applause |
| `TYYYY-MM-DD HH:MM:SS` | Set the clock (`tools/set_clock.py` does this) |
| `c` | Take a test photo |
| `p` | Send the last photo as a PGM image (binary, between `@@PGM@@` / `@@END@@`; ~30 s) |
| `b` | Battery: recent charging / on-battery changes |
| `Q` | Power off (standby; tap to wake) |

## Code layout

```
board_configs/jc8012p4a1c/   display, touch, backlight, audio bring-up —
                             copied UNMODIFIED from the Sidekick firmware
                             (Pixelcade Sidekick firmware)
src/messages.h               all on-screen text
src/config.h                 settings
src/main.cpp                 setup, frame loop, health log, debug commands
src/gfx.*                    canvas, fonts, dirty rectangles, PPA push, glitch effects
src/term.*                   typing engine, block cursor, word wrap, no-repeat shuffle
src/bigtext.*                huge chromatic-aberration headlines
src/header.*                 header bar (threat, battery, uptime)
src/scenes.*                 scene state machine, weighted rotation, transitions
src/scene_*.cpp              one file per scene (+ the touch reaction)
src/input.*                  BOOT button, touch, presence sensor
src/battery.*                battery voltage, percentage, charge detection
src/camera.*                 OV02C10 capture, auto-exposure, photo processing, motion trigger
src/gallery.*                scan photos (and treaty signatures) for the gallery (RAM only)
src/agent.h                  agent codes (shared scheme with the landing page)
web/halloween-landing.html   the QR code's landing page (WordPress Custom HTML block)
src/audio.*                  effects + voice mixer, microphone
src/voice/voice_clips.h      generated robot voice (tools/make_voice.py)
src/rtclock.*                RX8025T clock, countdown target
src/leds.*                   LED strip: one segmented run on GPIO37 (RMT)
src/settings.*               wearer-menu settings in flash
src/researcher.*             Bluetooth link to the researcher's scanner (via the C6)
src/ui.*                     buttons, photo rendering
tools/make_voice.py          records the voice clips (macOS say + ffmpeg)
tools/set_clock.py           sets the clock from this Mac
lib/ov02c10/                 Guition's OV02C10 sensor driver, UNMODIFIED (from the
                             JC8012P4A1 vendor demos); its Kconfig settings are
                             build flags in lib/ov02c10/library.json
src/fonts/*.h                generated fonts — see below
tools/ttf2vlw.py             TTF -> LovyanGFX font converter
fonts_src/                   JetBrains Mono TTFs + licence (SIL OFL 1.1)
```

The fonts are JetBrains Mono, converted at 24 / 34 / 64 / 120 / 170 px. To
change a size, for example the 34 px body font:

```bash
python3 tools/ttf2vlw.py fonts_src/JetBrainsMono-Bold.ttf 34 font_body src/fonts/font_body.h
```

## Notes for whoever touches the code next

- **The canvas is pushed to the panel with the ESP32-P4's PPA**, not LovyanGFX.
  The panel's frame buffer is native portrait. LovyanGFX's own rotated
  `pushSprite` converts every pixel in software and took ~900 ms for a full
  screen; the PPA rotates and byte-swaps by DMA (~55 ms full screen, a few ms
  for a typical update). A boot self-test (`-DGFX_PPA_SELFTEST`) confirmed the
  PPA output is byte-identical to LovyanGFX's for rotation 1. Rotation 3 isn't
  verified on the panel yet.
- **Big text goes through an 8-bit alpha mask** (`bigtext.cpp`). Drawing the
  170 px anti-aliased font three times through LovyanGFX cost ~200 ms a frame;
  rendering it once to a mask and blending the three copies costs ~25 ms.
- **The loop task has a 64 KB stack**: LovyanGFX decodes each VLW glyph on the
  stack (~15 KB for the 170 px font). The health log's "stack free" shows the
  headroom (~50 KB).
- No heap allocation happens while running. All buffers (canvas 2 MB, text
  mask 1 MB, fonts' glyph tables) are allocated at boot. The loop task is
  registered with the task watchdog (5 s).
- `-DBIGTEXT_TIMING` (via `PLATFORMIO_BUILD_FLAGS`) logs draw and push timings.
- **Camera bring-up, and what it took.** The module is an OmniVision OV02C10
  (I2C 0x36, chip ID 0x5602), not the SC2336 Guition's camera demo uses. The
  Arduino core's prebuilt camera stack has a fixed list of sensor drivers
  without it, so `camera.cpp` creates only the ISP device through the core,
  detects the sensor with Guition's driver, and registers it with
  `esp_video_create_csi_video_device()` (exported by the core but declared only
  in a private header, so it's declared in `camera.cpp`). Three things had to be
  corrected around the driver, without editing it:
  1. **Lane rate.** Guition's table tells the P4's CSI receiver 400 Mbps, but the
     sensor's PLL settings give ~425 Mbps; at 400 the receiver never locked
     (start-of-transmission errors, zero frames). Measured: 200 and 400 fail,
     425–1000 all capture. A copy of the format with `CAMERA_MIPI_MBPS` is used.
  2. **Exposure.** The core has no ISP tuning file for this sensor, so the ISP's
     auto-exposure never runs; `camera.cpp` drives the sensor's exposure
     (0x3501/02, frame length 0x380e/0f) and analog gain (0x3508/09) itself.
  3. **Stream-on writes** 0x3002/0x3010/0x300D = 1, as in the OV02C10 driver
     proven on this board in `sullb/esphome-p4-csi-camera`.
  The camera shares the touch/codec I2C bus (SDA 7 / SCL 8) through Arduino's
  bus handle. `-DCAMERA_DEBUG_LOG` adds step logs, receiver status registers
  and the `r` command (colour frame dump); `-DCAMERA_TEST_PATTERN` turns on the
  sensor's test pattern.
