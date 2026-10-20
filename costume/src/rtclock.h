#pragma once
// The board's RX8025T real-time clock (I2C 0x32, on the touch bus). Keeps time
// while the board is off only with a CR1220 cell fitted. Set it over serial:
// "T2026-10-31 20:15:00" (tools/set_clock.py sends the Mac's time).
#include <Arduino.h>

namespace rtclock {

void begin();
void tick(uint32_t now);           // reads the chip once a second
bool valid();                      // set, and hasn't lost power since
int hour();
int minute();
int second();
// Seconds until the next COUNTDOWN_HOUR:MINUTE (0..86399), and seconds since
// the last one.
int32_t secondsToTarget();
int32_t secondsSinceTarget();
bool takeTargetEvent();            // true once, just as the target time passes
bool set(const char* text);        // "YYYY-MM-DD HH:MM:SS"

}  // namespace rtclock
