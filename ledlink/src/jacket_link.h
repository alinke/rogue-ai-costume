#pragma once
// The jacket's own remote control, for when it's worn without the costume:
// LEDLINK's looks (looks.h), set from the armband (ARM-7) or the iPhone app
// (../../ios-jacket). Shared by ../../armband; the app has the same numbers in
// JacketLink.swift -- keep all three in step.
//
// Armband -> LEDLINK, advertised (manufacturer data, like the costume's "RCT1"):
//   0xFFFF "RJC1" seq cmd a b c keyLo keyHi
// A new seq = a new press (repeated for a few seconds so a scan window can't
// miss it); key = JACKET_LINK_KEY, so a stray advert can't drive it.
//
// iPhone -> LEDLINK over a Bluetooth connection (an iPhone can't put
// manufacturer data in its own adverts): GATT service JACKET_SERVICE_UUID,
//   COMMAND (write):                    cmd a b c
//   LIVE (write without response):      'M' bass mid treble level beat  (music, ~30/s)
//                                       'H' bpm                           (heart rate, ~1/s)
//   STATUS (read, notify):              the status bytes below
//   LAYOUT (read, write):               the LED layout -- the app's settings page:
//     read:  'L' 1 flags n totalLo totalHi maxLo maxHi pin maLo maHi, n parts, then
//            the Wi-Fi address as text (empty without Wi-Fi). A part is 5 bytes:
//            type (LedPart: 0 sleeve, 1 front, 2 eye, 3 matrix, 4 dark) side
//            (int8: -1 left, 1 right) reversed countLo countHi.
//            flags: LAYOUT_SAVED (not config.h's LED_CHAIN), LAYOUT_NO_MATRIX (the
//            8x8 part is skipped now: no costume, or its NO MATRIX).
//     write: 'L' 1 n, n parts           use this layout from now on (saved)
//            'D'                        back to config.h's LED_CHAIN
//            'T' kind aLo aHi bLo bHi   a test pattern (leds::testPattern: 0 off,
//                                       1 numbered, 2 LEDs a..b, 3 everything lit);
//                                       off again when the app disconnects
//
// LEDLINK -> everyone, its own advert (and STATUS):
//   0xFFFF "RJS1" look bright speed flags ackSeq r g b turn bpm
#include <stdint.h>

#ifndef JACKET_LINK_KEY
#define JACKET_LINK_KEY 0x4A4B
#endif

#define JACKET_SERVICE_UUID "524a4b31-0000-4c41-4237-000000000000"
#define JACKET_COMMAND_UUID "524a4b31-0001-4c41-4237-000000000000"
#define JACKET_LIVE_UUID    "524a4b31-0002-4c41-4237-000000000000"
#define JACKET_STATUS_UUID  "524a4b31-0003-4c41-4237-000000000000"
#define JACKET_LAYOUT_UUID  "524a4b31-0004-4c41-4237-000000000000"

namespace jacket {

static const uint8_t TAG_COMMAND[4] = {'R', 'J', 'C', '1'};
static const uint8_t TAG_STATUS[4] = {'R', 'J', 'S', '1'};

enum Command : uint8_t {
  CMD_NONE,
  CMD_LOOK,     // a = look (looks.h)
  CMD_BRIGHT,   // a = 10..100 %
  CMD_SPEED,    // a = 10..250 %
  CMD_COLOR,    // a b c = r g b: the colour the SOLID / BREATHE / SPARKLE / COMET looks use
  CMD_TURN,     // a = TURN_OFF / TURN_LEFT / TURN_RIGHT / TURN_HAZARD
  CMD_BRAKE,    // a = 1 on, 0 off (the app's motion sensing, or a button: on for ~1.5 s at most)
  CMD_POWER,    // a = 0 dark, 1 on
  CMD_NEXT,     // a = 1 next look, 255 previous
};

enum Turn : uint8_t { TURN_OFF, TURN_LEFT, TURN_RIGHT, TURN_HAZARD };

// Status flags.
constexpr uint8_t F_ON = 1;        // not switched dark
constexpr uint8_t F_COSTUME = 2;   // following the costume (look ROGUE AI, costume in range)
constexpr uint8_t F_MUSIC = 4;     // music levels arriving from the app
constexpr uint8_t F_HEART = 8;     // a heart rate arriving from the app
constexpr uint8_t F_BRAKE = 16;    // brake light on
constexpr uint8_t F_APP = 32;      // an iPhone is connected

constexpr int STATUS_LEN = 4 + 10;  // after 0xFFFF: tag + the ten bytes above

constexpr uint8_t LAYOUT_SAVED = 1;
constexpr uint8_t LAYOUT_NO_MATRIX = 2;
constexpr int LAYOUT_PART_LEN = 5;

}  // namespace jacket
