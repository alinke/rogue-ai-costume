#pragma once
// LEDLINK settings: the jacket's LED chain, as in the costume's config.h (the
// costume's own strip output is off -- this board drives the strips now).

#define LED_STRIP_ENABLED 1
#define LED_DATA_PIN 16            // the Gledopto's output (5 V, level-shifted)
#define LED_ONBOARD_OFF_PIN -1     // no on-board LED to switch off
#define LED_PIN_FINDER 0
#define BUTTON_PIN 17              // the Gledopto's button

// The jacket (see ../costume/README.md, "The jacket"), counted on the real one
// (2026-10-26, the Gledopto's /test?mode=index): three 1 m strips (160 each;
// the third cut short) + the matrix:
//   strip 1  front: right zipper edge up, round the neck, left edge down (155,
//            split at the collar so the effects run chest -> collar on both
//            sides), the last 5 inside (dark)
//   strip 2  left sleeve: 3 inside (dark), shoulder -> cuff (113), 44 inside
//   matrix   (64) -- skipped when the costume's operator console says NO MATRIX
//   strip 3  right sleeve: 25 inside (dark), shoulder -> cuff (113), the rest dark
// /test?mode=index (numbers) and /test?mode=lit (exactly what's lit) check it.
// The 4th number is the side, for the turn signals: -1 the wearer's left, 1 the right.
// LED_BENCH 1: the strips loose on the bench -- every LED lit, no hidden parts.
#define LED_BENCH 0
#if LED_BENCH
#define LED_CHAIN { {LEDS_TRIM, 160, false}, {LEDS_SLEEVE, 160, true, -1}, {LEDS_MATRIX, 64, false}, \
                    {LEDS_SLEEVE, 160, true, 1} }
#else
#define LED_CHAIN { {LEDS_TRIM, 78, false, 1}, {LEDS_TRIM, 77, true, -1}, {LEDS_DARK, 5, false}, \
                    {LEDS_DARK, 3, false}, {LEDS_SLEEVE, 113, true, -1}, {LEDS_DARK, 44, false}, \
                    {LEDS_MATRIX, 64, false}, \
                    {LEDS_DARK, 25, false}, {LEDS_SLEEVE, 113, true, 1}, {LEDS_DARK, 22, false} }
#endif
// Worn without the costume, the 8x8 matrix is taken off and the right sleeve's
// strip plugged straight into the left's: its 64 LEDs are skipped. With the
// costume on, its beacon says whether the 8x8 is wired (operator console: NO MATRIX).
#define LED_NO_MATRIX 1
// The layout above is the default: the app's settings page can change it (saved
// in flash, see jacket_link.h LAYOUT) up to these sizes.
#define LED_MAX_SEGMENTS 16
#define LED_MAX_LEDS 1200
#define LED_BRIGHTNESS_PERCENT 40
// Current limit for the whole chain: frames that would draw more are dimmed to
// fit (a white strobe on every LED is far over). 1000 on the bench (a Mac's USB
// port), ~2000 on a 2-3 A power bank port.
#define LED_MAX_MA 2000

#define MATRIX_SERPENTINE 1
#define MATRIX_ROTATION 3         // with MATRIX_MIRROR 1: connectors at the bottom, seen from the front
#define MATRIX_MIRROR 1
#define MATRIX_ORIENT_TEST 0
#define MATRIX_BRIGHTNESS_PERCENT 20

#define ORDER_RGB 0
#define ORDER_GRB 1
#define ORDER_BRG 2
#define ORDER_RBG 3
#define ORDER_GBR 4
#define ORDER_BGR 5
#define LED_COLOR_ORDER ORDER_GRB     // the WS2811 FCOB strips (confirmed: RGB showed idle green as pink)
#define MATRIX_COLOR_ORDER ORDER_GRB  // the WS2812B 8x8 panel

#define LED_INDEX_TEST 0
#define LED_TEST_COLOR 0

// The costume's beacon: gone this long = show the idle effect on our own.
#define COSTUME_LOST_MS 5000
