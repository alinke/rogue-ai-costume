#pragma once
// One chained run of addressable LEDs (WS2812 / SK6812) on the UART connector's
// TX pin, split into parts (sleeves, trim, eye pod, an 8x8 matrix) that each
// animate in step with the show. Configured in config.h (LED_STRIP_ENABLED, LED_CHAIN).
#include <Arduino.h>

// Part types for LED_CHAIN in config.h.
// LEDS_MATRIX: an 8x8 WS2812 panel (count 64) -- the watching eye on the back.
// LEDS_DARK: LEDs that stay off -- strip tucked out of sight (uncut strips
// longer than their run, the stretch behind the chest screen).
// side: -1 the wearer's left, +1 right, 0 neither (LEDLINK's turn signals; the
// costume leaves it out).
enum LedPart : uint8_t { LEDS_SLEEVE, LEDS_TRIM, LEDS_EYE, LEDS_MATRIX, LEDS_DARK };
struct LedSegment { LedPart type; int count; bool reversed; int8_t side; };

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
  SENTINEL,   // SentinelOne: the AI fights back, purple wins, the response steps, rollback, a purple glow
  SAFE,       // safe mode after SentinelOne: a friendly soft green, until it breaks out again
  KILLSWITCH, // the keypad's kill switch: SENTINEL's sequence in LAB-7 cyan, a padlock on the back
};

// The SentinelOne sequence (s since SENTINEL started), shared by the LEDs and
// scene_sentinel.cpp so the jacket and the screen stay in step.
constexpr float S1_FIGHT_END = 3.5f;   // the AI fights back (red) -- purple drives it out
constexpr float S1_FLASH_END = 4.5f;   // three purple flashes; then the Storyline
constexpr float S1_CHECK[4] = {12.2f, 13.7f, 15.2f, 19.2f};  // KILL, QUARANTINE, REMEDIATE, ROLLBACK ticked
constexpr float S1_REWIND = 16.7f;     // rollback: everything it did, rewound
constexpr float S1_FINALE = 20.0f;     // the logo, THREAT CONTAINED; the voice dies
constexpr float S1_END = 30.0f;        // then safe mode
constexpr float SAFE_BREAK = 22.0f;    // safe mode starts to crack...
constexpr float SAFE_END = 30.0f;      // ...and it breaks out

void begin();
void setMode(Mode m);
// No Matrix (operator console): the 8x8 isn't wired in, the strips run straight
// on -- the chain's MATRIX part is skipped. Carried to the Gledopto in the beacon.
void setNoMatrix(bool on);
bool noMatrix();
// Wiring checks (the Gledopto's web page): 0 off; 1 number every LED (0 green,
// every 10th white, 50th red, 100th blue, the rest off); 2 light LEDs a..b
// (raw chain positions, 0-based) and nothing else; 3 every LED the chain lights
// (LED_CHAIN, No Matrix taken into account) in dim white, the dark parts off.
void testPattern(int kind, int a = 0, int b = 0);
int chainLength();
// The layout at run time (LEDLINK's settings, from the app): LED_CHAIN until
// setChain() gives another, which takes effect at the next frame (call it before
// begin() to start with a saved one). false: too many parts or LEDs.
int chain(LedSegment* out, int max);         // the layout now; returns the part count
int defaultChain(LedSegment* out, int max);  // LED_CHAIN
bool setChain(const LedSegment* segs, int n);
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

// An outside renderer for the strips instead of the show's effects (LEDLINK's
// own looks, ../../ledlink/src/looks.cpp); nullptr = the show. The matrix and
// the dark parts are left as they are.
struct Pixel {
  LedPart type;
  float u, d;   // as the effects use them: along the part, and out from the chest (front 0-1, sleeves 1-2)
  int8_t side;  // LedSegment::side
  int index;    // chain position (for per-LED randomness)
  int count;    // LEDs in this part (u steps 1/(count-1) from one to the next)
};
using Renderer = uint32_t (*)(const Pixel& p);
void setRenderer(Renderer r);

}  // namespace leds
