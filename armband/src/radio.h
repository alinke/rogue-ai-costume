#pragma once
// Listens for the costume's beacon ("RAI1" + its state) and the researcher's
// scanners: the ESP32 scanner's "RSC1" beacon and the iPhone app's
// 52534331-SSTT-4C41-4237-... service UUID. Formats as in
// ../costume/src/researcher.cpp. Sends only the operator console's commands
// ("RCT1", ../costume/src/remote.h), for a few seconds per press.
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
void shutdown();  // Bluetooth off (power-off); waits up to ~2 s

// Operator console commands -- keep in step with ../costume/src/remote.h.
enum Command : uint8_t {
  CMD_NONE, CMD_SCAN, CMD_KILL_SWITCH, CMD_QR, CMD_NEXT, CMD_COUNTDOWN, CMD_MIDNIGHT, CMD_BOOTH,
  CMD_GALLERY, CMD_PAUSE, CMD_RESUME, CMD_MUTE, CMD_UNMUTE, CMD_BRIGHT_DOWN, CMD_BRIGHT_UP,
  CMD_VOICE_DOWN, CMD_VOICE_UP, CMD_STEALTH, CMD_OVERLOAD,
};
uint8_t send(Command cmd);   // starts advertising it; returns its seq
bool acked(uint8_t seq);     // the costume says it carried that one out

}  // namespace radio
