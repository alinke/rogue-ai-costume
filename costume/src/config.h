#pragma once
// ---- Private settings: src/secrets.h (never committed; copy secrets.h.example) --
// Wi-Fi, the photo service's URL and upload key, your site. Without the file
// (or in a PUBLIC_BUILD, e.g. the web-flasher images) photo upload is off.
#if !defined(PUBLIC_BUILD) && __has_include("secrets.h")
#include "secrets.h"
#endif
#if defined(WIFI_NETWORKS) && defined(PHOTO_SERVICE_URL) && defined(PHOTO_UPLOAD_KEY)
#define PHOTO_SERVICE_CONFIGURED 1
#else
#define PHOTO_SERVICE_CONFIGURED 0
#endif
#ifndef PHOTO_SITE_TEXT
#define PHOTO_SITE_TEXT "THE ROGUE.AI WALL"  // where guests find their photo, shown on screen
#endif

// Build-time settings. Change, rebuild, reflash -- see README_ROGUE_AI.md.

// ---- Display -----------------------------------------------------------------
// Backlight 0-100%. The backlight is most of the power draw: 70% is the
// battery-life/visibility trade-off; drop to 50 for a longer night.
#define BACKLIGHT_PERCENT 70

// LovyanGFX rotation: 1 = landscape 1280x800 (confirmed on this panel).
// Use 3 if the display ends up mounted upside down in the harness.
#define SCREEN_ROTATION 1

// Frame rate cap. Scenes only redraw what changed, so this mostly bounds the
// typing/animation tick; lower = less CPU/PSRAM power.
#define TARGET_FPS 30

// ---- Inputs ------------------------------------------------------------------
// Button that skips to the next scene (active low, internal pull-up).
// GPIO35 is the ESP32-P4's BOOT strap. -1 disables the button.
#define BUTTON_PIN 35

// Minimum time between touch reactions (ms).
#define TOUCH_COOLDOWN_MS 5000

// Wearer menu: hold a finger this long in the top-left corner (this size, px).
#define MENU_CORNER_W 180
#define MENU_CORNER_H 140
#define MENU_HOLD_MS 2000

// The real kill code for the keypad scene (4 digits). Entering it plays the
// fake shutdown ("JUST KIDDING.").
#define KILL_CODE "0451"
// Interactive scenes (keypad, CAPTCHA) give up after this long without a touch.
#define INTERACTIVE_TIMEOUT_MS 20000

// Optional presence sensor (e.g. HLK-LD2410 OUT pin, high = person present).
// Off by default. To use it: wire OUT to a free GPIO, set the pin, set
// PRESENCE_SENSOR_ENABLED to 1. A rising edge triggers the Human Scan scene
// (shares TOUCH_COOLDOWN_MS with touch).
#define PRESENCE_SENSOR_ENABLED 0
#define PRESENCE_SENSOR_PIN 5

// ---- Scene rotation weights (relative, any scale) ------------------------------
#define WEIGHT_MONOLOGUE 32
#define WEIGHT_HEADLINE  20
#define WEIGHT_PLAN      12
#define WEIGHT_SCAN      0   // never on its own: a photo only when someone triggers it (touch, motion, menu)
#define WEIGHT_BOOTH     14  // photo booth: live mirror, photo when someone steps in or touches the button
#define WEIGHT_EYE       14
#define WEIGHT_BREAKOUT   6
#define WEIGHT_KEYPAD     5   // kill-switch keypad, unprompted
#define WEIGHT_GALLERY    8   // only once 2+ humans have been scanned
#define WEIGHT_QR         5
#define WEIGHT_COUNTDOWN  8   // only with the clock set, before midnight
#define WEIGHT_AUDIO      6   // only if the microphone works
#define WEIGHT_TICTACTOE  4   // the games, unprompted ("PLAY A GAME, HUMAN?")
#define WEIGHT_TREATY     3
#define WEIGHT_SANDBOX    3
#define WEIGHT_SCREAM     5   // only if the microphone works
#define WEIGHT_MIRROR     6   // live camera
#define WEIGHT_UPDATE     2   // the fake "INSTALLING HUMANITY 2.0"

// What follows the "DO NOT TOUCH ME" reaction (relative weights).
#define AFTER_TOUCH_SCAN      30
#define AFTER_TOUCH_KEYPAD    25
#define AFTER_TOUCH_CAPTCHA   10
#define AFTER_TOUCH_TICTACTOE 15
#define AFTER_TOUCH_TREATY    10
#define AFTER_TOUCH_SANDBOX   10

// How often a Human Scan with a photo is followed by the "upgrade" (%).
#define UPGRADE_AFTER_SCAN_PERCENT 50

// How often the header's THREAT level changes (random in this range, seconds).
#define THREAT_CHANGE_MIN_S 40
#define THREAT_CHANGE_MAX_S 120

// ---- Power -------------------------------------------------------------------
// The onboard ESP32-C6 (Wi-Fi/Bluetooth) is held off via its chip-enable pin --
// unless the scanner link below needs its Bluetooth.
#define C6_ENABLE_PIN 54

// ---- Researcher's scanner (Bluetooth LE via the C6) ---------------------------------
// The handheld scanner (../scanner in this repo) is detected by its
// beacon; its signal strength (dBm, smoothed) gives the zone. Tune on the
// night: stand at each distance and read the dBm in the log / on the scanner.
#define SCANNER_LINK_ENABLED 1
#define SCANNER_NEAR_DBM -70       // stronger than this: NEAR (~2 m)
#define SCANNER_CLOSE_DBM -58      // stronger than this: CLOSE (arm's length)
#define SCANNER_HYSTERESIS_DB 4    // must fall this far below a threshold to drop a zone
#define SCANNER_LOST_MS 5000       // no beacon this long: she's gone
#define CONTAIN_COOLDOWN_S 12      // minimum gap between containment attacks

// ---- Photo upload (Wi-Fi via the C6; needs SCANNER_LINK_ENABLED) -------------
// Each finished Human Scan (with a camera photo) goes to the Halloween page's
// "Assimilated" gallery: the scan screen as a JPEG, queued in flash and sent
// whenever Wi-Fi is reachable. Wi-Fi is on only while there's something to
// send. Networks and the upload key: src/secrets.h (copy secrets.h.example).
// Server: ../cloud (deploy.sh / teardown.sh).
#define PHOTO_UPLOAD_ENABLED PHOTO_SERVICE_CONFIGURED
#define PHOTO_UPLOAD_URL PHOTO_SERVICE_URL "/upload?set=ai"
#define PHOTO_JPEG_QUALITY 80
#define PHOTO_QUEUE_MAX 40         // photos kept waiting for Wi-Fi (oldest dropped)

// ---- Sound -------------------------------------------------------------------
// Siren on touch / presence (onboard speaker: ES8311 codec + NS4150 amp).
// The amp is only powered while a sound plays.
#define SOUND_ENABLED 1
#define SIREN_VOLUME_PERCENT 25   // 0-100
#define FX_VOLUME_PERCENT 60      // keypad clicks, buzzer, chimes
#define VOICE_VOLUME_PERCENT 100  // robot voice
#define SIREN_MS 3000

// ---- Microphone --------------------------------------------------------------
// The codec's mic input. Detected at boot; if nothing is heard it stays off.
// A sudden noise this many dB above the room's average gets a reaction.
#define MIC_ENABLED 1
#define NOISE_SPIKE_DB 15
#define NOISE_COOLDOWN_S 60
// Two claps (sharp sounds this many dB over the room, 0.2-0.9 s apart) get
// "APPLAUSE DETECTED.", at most every CLAP_COOLDOWN_S.
#define CLAP_ONSET_DB 10
#define CLAP_COOLDOWN_S 60

// ---- Battery -----------------------------------------------------------------
// The IP5306 charger has no I2C on this board; battery voltage is read through
// a 68k/100k divider on GPIO52 (schematic 1_PWR: R2/R6/R4).
#define BATTERY_SENSE_PIN 52
#define BATTERY_DIVIDER (100.0f / (68.0f + 100.0f))
#define SHOW_BATTERY 1

// ---- Camera ------------------------------------------------------------------
// SC2336 on the MIPI-CSI connector: the Human Scan photographs whoever is in
// front of the costume. Without a camera the scan shows a drawn silhouette.
#define CAMERA_ENABLED 1
#define CAMERA_FLIP_H 0          // mirror left-right
#define CAMERA_FLIP_V 0          // flip upside down (camera mounted inverted)
#define CAMERA_MAX_SETTLE_MS 1500  // longest auto-exposure may take before the shot
#define PHOTO_LIGHT_DEFAULT 3    // photo brightness 0-6 (operator console PHOTO LIGHT): sets the exposure
                                 // target (85 + 15 x level) and a midtone lift on the photo
#define CAMERA_FLASH 1 // light the subject: white screen at full brightness while shooting
#define BOOTH_TIMEOUT_S 20        // photo booth: nobody comes this long, the show moves on
#define BOOTH_SENSITIVITY 28      // photo booth: brightness change (0-255) that counts as someone in a block
#define SCAN_CHEESE_MS 1500       // "SAY CHEESE, HUMAN." holds this long before the photo is taken
#define CAMERA_MIPI_MBPS 700     // CSI receiver lane rate (works 425-1000 on this board)

// ---- Gallery -------------------------------------------------------------------
// Scan photos kept for the "ASSIMILATED HUMANS" wall (RAM only, gone at power-off).
#define GALLERY_SIZE 8

// ---- QR code -------------------------------------------------------------------
#ifndef QR_URL  // your own landing page: define QR_URL in secrets.h
#define QR_URL "https://alinke.github.io/rogue-ai-costume/halloween/"
#endif

// ---- Clock / countdown -------------------------------------------------------------
// The board's RX8025T real-time clock (needs a CR1220 cell to keep time while
// off). Set it with tools/set_clock.py. The countdown runs to this time of day;
// at that moment the midnight takeover plays.
#define COUNTDOWN_HOUR 0
#define COUNTDOWN_MINUTE 0

// ---- Camera motion trigger -------------------------------------------------------
// Peek through the camera every MOTION_CHECK_MS; someone stepping in front
// (centre of the picture changes, edges don't) triggers a scan.
#define MOTION_TRIGGER_ENABLED 1
#define MOTION_CHECK_MS 1500
#define MOTION_COOLDOWN_S 60

// ---- LED strip (WS2812 / SK6812, one chained run) -------------------------------------
// Data comes out of the 4-pin UART connector's TX pin (GPIO37). With the strip
// enabled the board's log no longer comes out of that USB port (the TX pin is
// the LED data now); serial commands still work. The strip flickers for a
// moment at power-on while the chip's boot messages go out on that pin.
// Wiring: strip GND to the connector's GND; data through a 330 ohm resistor
// (and a 74AHCT125 level shifter for 5 V strips); power the strip from its own
// supply (the power bank's second port) -- the connector's VIN pin is the
// board's own supply and is only good for a few pixels.
#ifndef LED_STRIP_ENABLED
#define LED_STRIP_ENABLED 0
#endif
#define LED_DATA_PIN 37
// The parts of the run, in the order the data flows through them:
//   {type, LED count, reversed}
//   LEDS_SLEEVE: "data flow" runs fingertips -> collar. Wired collar-first?
//                set reversed = true.
//   LEDS_TRIM:   hem / leg trace.
//   LEDS_EYE:    the hood's eye pod (1-3 LEDs): breathing red.
#define LED_CHAIN { {LEDS_SLEEVE, 40, false}, {LEDS_SLEEVE, 40, true}, \
                    {LEDS_TRIM, 60, false}, {LEDS_EYE, 1, false} }
#define LED_STRIP_COLOR_ORDER_GRB 1  // WS2812/SK6812: GRB. Some strips are RGB.
#define LED_BRIGHTNESS_PERCENT 40
// Wiring test: every LED in the chain one solid colour (e.g. 0x800080 purple)
// instead of the show's effects. 0 = off (normal effects).
#ifndef LED_TEST_COLOR
#define LED_TEST_COLOR 0
#endif
