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
  CONTAIN,    // containment beam: white/red strobe
};

void begin();
void setMode(Mode m);
void tick(uint32_t now);  // ~30 fps from loop()
// The matrix shows a big number instead of the eye (the midnight countdown);
// -1 = back to the eye.
void matrixNumber(int n);
Mode mode();              // the effect running now (sent in the beacon)
int matrixNumberNow();    // -1 = the eye

}  // namespace leds
