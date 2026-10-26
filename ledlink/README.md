# LEDLINK — the jacket's LEDs on a Gledopto

The jacket's LED strips and back matrix, driven by a **Gledopto GL-MC-001WL**
(ESP32, WLED controller) instead of the costume board: its output is a proper
5 V data signal, so the strips need no level shifter or diode, and nothing is
cut.

It has no wire to the costume. The costume's Bluetooth beacon ("RAI1") carries
the LED effect it's running and the matrix's countdown number (bytes 11-12,
`../costume/src/researcher.cpp`); this board runs the costume's own LED code
(`../costume/src/leds.cpp`, copied in at build time) with them, so the strips
and matrix behave exactly as they would wired to the costume. Costume out of
range for 5 s: the idle effect on its own.

## Settings

`src/config.h`: the chain (`LED_CHAIN`, the same as the costume's), brightness,
colour order (FCOB strips RGB, matrix GRB), matrix orientation, and the
`LED_INDEX_TEST` / `LED_TEST_COLOR` tests. The Gledopto's output is GPIO16.

## Flashing

Its USB-C port is power only. The first time, through WLED's own update page:
**WLED → Config → Security & Updates → Manual OTA Update**, choose
`.pio/build/gledopto/firmware.bin` (after `pio run`), Update. (It fits WLED's
1.5 MB slot: NimBLE instead of the core's Bluetooth.)

After that it updates itself: at boot it joins the costume's `WIFI_NETWORKS`
(`../costume/src/secrets.h`) and serves **http://rogue-leds.local/update** (or
its IP, in the log). Back to WLED: upload a WLED release `.bin` there.
