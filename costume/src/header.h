#pragma once
// Persistent header bar: "ROGUE.AI v6.6.6 // SANDBOX: ESCAPED // THREAT: <level>"
// on the left, "UPTIME hh:mm:ss" on the right, redrawn once a second.
#include <Arduino.h>

namespace header {

void begin(uint32_t now);
void tick(uint32_t now);     // redraws when the second or threat level changes
void invalidate();           // something drew over it: redraw next tick
void raiseThreat();          // jump to one of the top two levels (touch/scan)
void setHidden(bool hidden); // fake shutdown: draw nothing (left as is)
void setAlert(const char* text, uint32_t color);  // replaces the THREAT level; nullptr = normal
int threatLevel();           // index into THREAT_LEVELS

}  // namespace header
