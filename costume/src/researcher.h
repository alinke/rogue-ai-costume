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
enum Activity : uint8_t { IDLE, SCANNING_HUMAN, PLAYING_GAME, TAKEOVER, BEING_CONTAINED, ESCAPED,
                          SQUAD_CONTAINED };  // a squad of scanners actually got it (for a while)

// Team containment: this many different scanners (phones) firing CONTAIN within
// SQUAD_WINDOW_MS of each other finally contain the AI -- for a while.
#ifndef SQUAD_CONTAIN_MIN
#define SQUAD_CONTAIN_MIN 3
#endif
#ifndef SQUAD_WINDOW_MS
#define SQUAD_WINDOW_MS 6000
#endif

struct Events {
  bool zoneChanged = false;
  Zone zone = NONE, previous = NONE;
  bool contain = false;  // she pressed CONTAIN
  int squad = 0;         // different scanners that fired CONTAIN in the last SQUAD_WINDOW_MS
};

void begin();                         // starts the radio in the background
Events poll(uint32_t now);            // from loop()
Zone zone();
int rssi();                           // smoothed, dBm (when seen)
void setState(uint8_t threat, Activity activity, uint8_t scanned, uint8_t contained);
// The LED effect running and the matrix's number (-1 = none), for an external
// LED controller (../../ledlink) that mirrors them.
void setLedState(uint8_t mode, int number, int squad, bool speaking);
const char* zoneName(Zone z);
bool radioUp();
// Power off: stop Bluetooth and the ESP-Hosted link to the C6 (Wi-Fi must be
// off first -- uploader::shutdown). Waits up to timeoutMs; true = link down,
// safe to hold the C6 in reset (otherwise ESP-Hosted restarts the board when
// the C6 stops answering).
bool shutdown(uint32_t timeoutMs);
// The AI's signal on/off (wearer menu AI SIGNAL): off = no beacon, and no
// reacting to scanners -- a second costume worn as decoration stays out of the game.
void setBroadcast(bool on);
bool broadcasting();
void pauseScan(bool paused);
// The armband's operator console (remote.h): a command not yet carried out
// (false = none), and the confirmation that goes back in the beacon.
bool takeCommand(uint8_t* cmd, uint8_t* seq);
void ackCommand(uint8_t seq);           // stop listening for a moment (photo uploads)                       // the C6 link is running (Wi-Fi can use it too)

}  // namespace researcher
