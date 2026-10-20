#pragma once
// Bluetooth LE beacons with the costume (no pairing): this scanner advertises
// "RSC1" + a trigger counter (+1 per CONTAIN press) + a per-boot session id; the costume advertises
// "RAI1" + its state. Format shared with rogue-ai-costume/src/researcher.cpp.
#include <Arduino.h>

namespace radio {

struct AiState {
  bool seen = false;      // heard within LOST_MS
  int rssi = -100;        // smoothed dBm
  uint8_t threat = 0;     // index into THREAT_LEVELS
  uint8_t activity = 0;   // 0 idle, 1 scanning a human, 2 playing a game, 3 takeover, 4 being contained
  uint8_t scanned = 0;    // humans scanned tonight
  uint8_t contained = 0;  // times it's been hit with CONTAIN
};

void begin();
AiState ai();
void fireContain();  // bumps the trigger counter in our beacon
bool up();           // Bluetooth running (the C6 link is up; Wi-Fi can use it too)
void pauseScan(bool paused);  // stop listening for a moment (photo uploads)

}  // namespace radio
