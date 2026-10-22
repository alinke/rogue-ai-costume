#pragma once
// Listens (only -- it sends nothing) for the costume's beacon ("RAI1" + its
// state) and the researcher's scanners: the ESP32 scanner's "RSC1" beacon and
// the iPhone app's 52534331-SSTT-4C41-4237-... service UUID. Formats as in
// ../costume/src/researcher.cpp.
#include <Arduino.h>

namespace radio {

struct Core {             // the costume
  bool seen = false;
  int rssi = -100;
  uint8_t threat = 0, activity = 0, scanned = 0, contained = 0;
};
struct Hunter {           // the nearest researcher scanner
  bool seen = false;
  int rssi = -100;        // smoothed dBm
};

void begin();
Core core();
Hunter hunter();
// True once per CONTAIN press on any scanner in range (its beacon's trigger
// counter went up within the same session) -- straight from the scanner, so
// it doesn't wait for the costume.
bool beamFired();

}  // namespace radio
