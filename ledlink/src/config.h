#pragma once
// LEDLINK settings: the jacket's LED chain, as in the costume's config.h (the
// costume's own strip output is off -- this board drives the strips now).

#define LED_STRIP_ENABLED 1
#define LED_DATA_PIN 16            // the Gledopto's output (5 V, level-shifted)
#define LED_ONBOARD_OFF_PIN -1     // no on-board LED to switch off
#define LED_PIN_FINDER 0
#define BUTTON_PIN 17              // the Gledopto's button

// The jacket (see ../costume/README.md, "The jacket"): three uncut 1 m strips
// (160 each) + the matrix, 544 LEDs:
//   strip 1  front: up behind the screen (15, dark), right zipper edge to the
//            collar (45), round the neck under the hood (40, dark), left zipper
//            edge down to the screen (45), behind the screen (15, dark)
//   strip 2  left sleeve, shoulder -> cuff (98), tucked in the cuff (62, dark)
//   matrix   (64)
//   strip 3  right sleeve, shoulder -> cuff (98), tucked in the cuff (62, dark)
// LED_INDEX_TEST 1 shows where each LED number falls, to set these.
// LED_BENCH 1: the strips loose on the bench -- every LED lit, no hidden parts.
#define LED_BENCH 1
#if LED_BENCH
#define LED_CHAIN { {LEDS_TRIM, 160, false}, {LEDS_SLEEVE, 160, true}, {LEDS_MATRIX, 64, false}, \
                    {LEDS_SLEEVE, 160, true} }
#else
#define LED_CHAIN { {LEDS_DARK, 15, false}, {LEDS_TRIM, 45, false}, {LEDS_DARK, 40, false}, \
                    {LEDS_TRIM, 45, true}, {LEDS_DARK, 15, false}, \
                    {LEDS_SLEEVE, 98, true}, {LEDS_DARK, 62, false}, \
                    {LEDS_MATRIX, 64, false}, \
                    {LEDS_SLEEVE, 98, true}, {LEDS_DARK, 62, false} }
#endif
#define LED_BRIGHTNESS_PERCENT 40

#define MATRIX_SERPENTINE 1
#define MATRIX_ROTATION 2         // mounted with its connectors at the bottom
#define MATRIX_MIRROR 0
#define MATRIX_ORIENT_TEST 1
#define MATRIX_BRIGHTNESS_PERCENT 20

#define ORDER_RGB 0
#define ORDER_GRB 1
#define ORDER_BRG 2
#define ORDER_RBG 3
#define ORDER_GBR 4
#define ORDER_BGR 5
#define LED_COLOR_ORDER ORDER_RGB     // the WS2811 FCOB strips (test: LED_TEST_COLOR 0xFF0000 shows red)
#define MATRIX_COLOR_ORDER ORDER_GRB  // the WS2812B 8x8 panel

#define LED_INDEX_TEST 0
#define LED_TEST_COLOR 0

// The costume's beacon: gone this long = show the idle effect on our own.
#define COSTUME_LOST_MS 5000
