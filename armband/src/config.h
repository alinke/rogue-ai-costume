#pragma once
// ARM-7 settings.

// Screen: 3 = landscape as the Sidekick firmware uses it; 1 = the same turned
// 180 degrees (if it reads upside down the way it sits on your arm).
#define ROTATION 3
#define BACKLIGHT_PERCENT 60
#define BACKLIGHT_IDLE_PERCENT 20  // ...after CORE_LOST_DIM_S without the costume's beacon
#define CORE_LOST_DIM_S 60
#define FRAME_MS 50                // 20 redraws a second

// The hunter (the researcher's scanner -- the ESP32 one or the iPhone app).
// Same thresholds as the costume's config.h, so both agree on "near".
#define NEAR_DBM -70          // stronger: CLOSING IN (~2 m)
#define CLOSE_DBM -58         // stronger: PROXIMITY CRITICAL (arm's length)
#define HYSTERESIS_DB 4
#define LOST_MS 5000          // no beacon this long: gone
#define RADAR_FAR_DBM -95     // blip at the radar's edge
#define RADAR_NEAR_DBM -50    // blip at the centre

// RGB LED on the front (LED1). Active low on this board family.
#define LED_R_PIN 4
#define LED_G_PIN 16
#define LED_B_PIN 17
#define LED_ACTIVE_LOW 1
#define LED_MAX_PERCENT 60    // it's bright, and it's right under your nose

// Operator console: hold the screen this long to open it. Its buttons run the
// costume over Bluetooth (../costume/src/remote.h).
#define CONSOLE_HOLD_MS 800
#define CONSOLE_IDLE_CLOSE_MS 20000
#define ARM_TARGET 0          // which costume: 0 = any in range, 1 Guition, 2 Elecrow 10.1", 3 Elecrow 7", 4 Sunton 7", 5 Sunton 5", 6 Tab5
#define ARM_LINK_KEY 0x7A37   // must match the costume's (remote.h)
#define SEND_MS 3000          // a command is advertised this long, or until the costume confirms it

// Power off: the console's ARM-7 OFF (tap twice). Everything shuts down and the
// board sleeps; the battery chip (IP5306) then cuts the power itself after
// ~30 s at that low draw -- SW1 on the back or plugging in USB starts it
// again. A tap also wakes it (in case the chip keeps the power on): checked 4x
// a second.
#define BACKLIGHT_PIN 27

// Battery gauge. The board has no battery sense of its own: two 100k resistors
// in series from the battery's + (after the switch) to GND, the middle to
// GPIO35 (on the 4-pin connector P3: IO21, IO22, IO35, GND). -1 = no gauge.
#define BATTERY_SENSE_PIN 35
#define BATTERY_DIVIDER 2.0f      // (R1 + R2) / R2
#define BATTERY_MIN_VALID 2.8f    // below this (V at the battery) = not wired up: no gauge
#define BATTERY_LOW_PERCENT 15    // amber below 40%, red and blinking below this
