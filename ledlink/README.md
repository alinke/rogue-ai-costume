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

## Without the costume: the jacket's own looks

`src/looks.cpp`: 31 looks -- themes (Light Cycle, Cyberpunk, Code Rain, Fire,
Aurora...), holidays (Christmas, New Year and July 4th fireworks,
Thanksgiving...), bike (turn signals, brake light, Bike / Day Flash), and live
(a music visualizer and a heartbeat, fed by the iPhone app). The look **ROGUE
AI** is the costume's show exactly as above; any other look ignores the
costume. The look, brightness, speed and colour are saved.

Set from the armband's JACKET screen (advertised "RJC1" commands) or the
iPhone app (`../ios-jacket`, a Bluetooth connection); LEDLINK advertises its
status ("RJS1") so both show what it's really doing. Protocol:
`src/jacket_link.h`; the look list: `src/looks.h`.

`LED_NO_MATRIX 1` (`src/config.h`): the 8x8 is off the jacket. Each strip's
4th `LED_CHAIN` number is its side (-1 the wearer's left, 1 right) for the
turn signals.

Testing over Wi-Fi: `/cmd?c=1&a=14` (CMD_LOOK 14, Christmas; the numbers are
in `jacket_link.h`), `/cmd?c=5&a=1` (left turn signal), `/live?bpm=120`,
`/live?bass=200&mid=120&treble=60&level=150&beat=1`.

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
its IP) -- in a browser, or
`curl -H "Origin: http://<ip>" -F "update=@.pio/build/gledopto/firmware.bin" http://<ip>/update`.
Back to WLED: upload a WLED release `.bin` there.

**Status:** its address in a browser (http://rogue-leds.local/) shows whether it
hears the costume, the effect and the matrix number it's showing, and its
Bluetooth packet counts. Confirmed: it follows the costume's MIDNIGHT TEST
countdown (10..1 on the matrix) and switches to the midnight chase with it.
