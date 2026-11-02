#pragma once
// Listens for the costume's beacon ("RAI1" + its state) and the researcher's
// scanners: the ESP32 scanner's "RSC1" beacon and the iPhone app's
// 52534331-SSTT-4C41-4237-... service UUID. Formats as in
// ../costume/src/researcher.cpp. Sends only the operator console's commands
// ("RCT1", ../costume/src/remote.h), for a few seconds per press.
//
// Also the jacket's remote, for wearing it without the costume: LEDLINK's
// status advert ("RJS1") and its commands ("RJC1"), ../../ledlink/src/jacket_link.h.
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
  CMD_VOICE_DOWN, CMD_VOICE_UP, CMD_STEALTH, CMD_OVERLOAD, CMD_LEDS_DOWN, CMD_LEDS_UP,
};
uint8_t send(Command cmd);   // starts advertising it; returns its seq
bool acked(uint8_t seq);     // the costume says it carried that one out

// The jacket (LEDLINK, the Gledopto driving the LED strips).
struct Jacket {
  bool seen = false;
  int rssi = -100;
  uint8_t look = 0, bright = 0, speed = 100, flags = 0, ack = 0, turn = 0, bpm = 0;
  uint32_t color = 0;
};
Jacket jacket();
uint8_t sendJacket(uint8_t cmd, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0);  // jacket_link.h; returns its seq
bool jacketAcked(uint8_t seq);

}  // namespace radio
