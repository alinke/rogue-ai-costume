#pragma once
// One chained run of addressable LEDs (WS2812 / SK6812) on the UART connector's
// TX pin, split into parts (sleeves, trim, eye pod) that each animate in step
// with the show. Configured in config.h (LED_STRIP_ENABLED, LED_CHAIN).
#include <Arduino.h>

// Part types for LED_CHAIN in config.h.
enum LedPart : uint8_t { LEDS_SLEEVE, LEDS_TRIM, LEDS_EYE };
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

}  // namespace leds
