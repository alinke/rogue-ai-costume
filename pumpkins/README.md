# The pumpkins

Two stand-alone Halloween decorations: small screens set into pumpkins, running
their own ROGUE.AI show. They don't need the costume.

- **Pumpkin eye** -- Elecrow CrowPanel 2.1" round knob display (ESP32-S3,
  480x480 touch) -- env `rogue-eye-round-2-1`, `src/rogue_eye.cpp`. A red
  robotic eye that looks around, blinks, glitches and taunts, broken up every
  10-20 s by a scene: radar scan, terminal hijack, code rain, specimen file,
  singularity progress, signal takeover, fake containment, dizzy spin.
  - Knob: skip to the next scene.
  - Tap the eye: it flinches; three taps and it tells you to stop.
  - Hold 4 s: the midnight show, by hand.
- **Pumpkin warning screen** -- Sunton ESP32-4827S043C 4.3" (ESP32-S3,
  480x272) -- env `rogue-wide-4827s043c`, `src/rogue_wide.cpp`. A LAB-7
  warning screen that ROGUE.AI keeps hijacking: a huge red eye between hazard
  warnings, a status board, news tickers, a midnight countdown and more. Big
  text, readable across a room. Not interactive.

## Flashing

From the browser: https://alinke.github.io/rogue-ai-costume/ (Chrome or Edge on
a desktop computer). Or build it:

```
pio run -e rogue-eye-round-2-1 -t upload
pio run -e rogue-wide-4827s043c -t upload
```

## The midnight show

At 23:59:50 both count down to midnight, then the AI takes over. That needs the
time, which comes from Wi-Fi: send `WIFI <ssid>|<password>` over the USB serial
port (115200 baud, newline at the end) once. On a cold boot with no valid time
the board joins that network, gets the time and reboots with the radio off --
the show itself never runs Wi-Fi (it causes display noise on these RGB panels).
Without Wi-Fi there's no automatic midnight show; on the round eye, hold the
screen for 4 s instead.

The time zone is US Pacific: `TZ_INFO` at the top of each source file.

Display bring-up (`boards/`, `board_configs/`, `apply_lovyangfx_rgb_patch.py`)
comes from the Pixelcade Sidekick firmware, copied unmodified, as in
`../costume`.
