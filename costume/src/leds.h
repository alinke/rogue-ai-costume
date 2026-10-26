#pragma once
// One chained run of addressable LEDs (WS2812 / SK6812) on the UART connector's
// TX pin, split into parts (sleeves, trim, eye pod, an 8x8 matrix) that each
// animate in step with the show. Configured in config.h (LED_STRIP_ENABLED, LED_CHAIN).
#include <Arduino.h>

// Part types for LED_CHAIN in config.h.
// LEDS_MATRIX: an 8x8 WS2812 panel (count 64) -- the watching eye on the back.
// LEDS_DARK: LEDs that stay off -- strip tucked out of sight (uncut strips
// longer than their run, the stretch behind the chest screen).
enum LedPart : uint8_t { LEDS_SLEEVE, LEDS_TRIM, LEDS_EYE, LEDS_MATRIX, LEDS_DARK };
struct LedSegment { LedPart type; int count; bool reversed; };

namespace leds {

enum Mode : uint8_t {
  IDLE,       // green "data flow" toward the collar; eye pod breathes red
  ALERT,      // red strobe (touch reaction, siren)
  SCAN,       // a cyan sweep up and down the run
  BREAKOUT,   // flickering green, failing in red
  MIDNIGHT,   // fast red chase
  NOISE,      // amber flash
  OFF,        // dark (fake shutdown)
  WARY,       // the researcher is around: amber creeping into the green flow
  CONTAIN,    // containment beam: red/cyan strobe; the sleeves fill with the squad count
  TOUCH,      // a red ripple out from the chest: up the front, then down the sleeves
  SHUTDOWN,   // the fake shutdown: the sleeves die, then the front, the eye last
  OVERLOAD,   // armband: a few seconds of full-chain red (current-limited)
  STEALTH,    // armband: everything dark until it's switched back
  SENTINEL,   // SentinelOne: a purple victory -- converging waves, a flash, then a purple glow
};

void begin();
void setMode(Mode m);
void tick(uint32_t now);  // ~30 fps from loop()
// The matrix shows a big number instead of the eye (the midnight countdown);
// -1 = back to the eye.
void matrixNumber(int n);
Mode mode();              // the effect running now (sent in the beacon): stealth and
                          // overload win over whatever the show set
int matrixNumberNow();    // -1 = the eye

// What colours the effects, updated from loop() (or, on the Gledopto, from the
// costume's beacon and its own listening):
void setProximity(int dbm);     // the nearest scanner, smoothed dBm; -127 = none in range
void setSquad(int n);           // scanners that fired CONTAIN within the squad window
void setSpeaking(bool on);      // the costume's voice is talking
void setThreat(int level);      // 0..5: the idle colour warms with it
void setInfectedNear(bool on);  // an infected guest's phone is close
void setStealth(bool on);       // armband STEALTH
bool stealth();
void overload(uint32_t ms = 5000);  // armband OVERLOAD
void setBrightness(int pct);    // the strips, 10-100% (the matrix follows in proportion)
int brightness();

}  // namespace leds
