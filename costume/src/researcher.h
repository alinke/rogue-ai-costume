#pragma once
// Radio link to the researcher's handheld scanner (Bluetooth LE, through the
// board's ESP32-C6). Both devices broadcast small beacons -- no pairing, no
// connection: the costume advertises "I'm the AI" + its state, and listens for
// the scanner's beacon, whose signal strength gives how close she is and whose
// trigger counter goes up each time she presses CONTAIN. See config.h
// SCANNER_LINK_* and the scanner firmware (scanner/ in this repo).
#include <Arduino.h>

namespace researcher {

enum Zone : uint8_t { NONE, DETECTED, NEAR, CLOSE };

// What the AI is doing, broadcast so the scanner can show it.
enum Activity : uint8_t { IDLE, SCANNING_HUMAN, PLAYING_GAME, TAKEOVER, BEING_CONTAINED, ESCAPED };

struct Events {
  bool zoneChanged = false;
  Zone zone = NONE, previous = NONE;
  bool contain = false;  // she pressed CONTAIN
};

void begin();                         // starts the radio in the background
Events poll(uint32_t now);            // from loop()
Zone zone();
int rssi();                           // smoothed, dBm (when seen)
void setState(uint8_t threat, Activity activity, uint8_t scanned, uint8_t contained);
const char* zoneName(Zone z);
bool radioUp();
// Power off: stop Bluetooth and the ESP-Hosted link to the C6 (Wi-Fi must be
// off first -- uploader::shutdown). Waits up to timeoutMs; true = link down,
// safe to hold the C6 in reset (otherwise ESP-Hosted restarts the board when
// the C6 stops answering).
bool shutdown(uint32_t timeoutMs);
void pauseScan(bool paused);           // stop listening for a moment (photo uploads)                       // the C6 link is running (Wi-Fi can use it too)

}  // namespace researcher
