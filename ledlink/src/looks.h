#pragma once
// The jacket's own looks, for wearing it without the costume (biking, the
// holidays, concerts). LEDLINK draws them (looks.cpp); the armband shows the
// same list. Numbers are sent over the air (jacket_link.h): add new looks at the
// end, never renumber. The iPhone app has the same table in JacketLink.swift.
#include <stdint.h>

namespace looks {

enum Category : uint8_t { CAT_THEME, CAT_HOLIDAY, CAT_BIKE, CAT_LIVE, CAT_COUNT };
static const char* const CATEGORY_NAMES[CAT_COUNT] = {"THEMES", "HOLIDAYS", "BIKE", "LIVE"};

enum Look : uint8_t {
  ROGUE_AI,      // the costume's own show: follows the costume when it's on, its idle flow when not
  LIGHT_CYCLE,   // dark-blue grid, cyan light cycles racing out with long trails, an orange rival
  CYBERPUNK,     // magenta/cyan neon drifting, yellow glitches
  CODE_RAIN,     // green code raining down the sleeves and the front
  SYNTHWAVE,     // a pink-purple-orange sunset gradient rolling by
  RAINBOW,
  FIRE,          // flames climbing from the cuffs and the hem
  AURORA,        // slow green/teal/violet curtains
  OCEAN,         // blue waves rolling out
  SCANNER,       // a red sweep back and forth
  SOLID,         // the chosen colour
  BREATHE,       // the chosen colour, breathing
  SPARKLE,       // the chosen colour twinkling
  COMET,         // the chosen colour racing out from the chest
  CHRISTMAS,     // red and green marching, white twinkles
  CANDY_CANE,    // red and white stripes
  WINTER,        // icy blue with falling snow
  HANUKKAH,      // blue and white, candle flicker
  NEW_YEAR,      // gold and silver glitter, fireworks
  VALENTINE,     // pink and red heartbeats
  ST_PATRICK,    // greens with gold glints
  EASTER,        // pastels
  JULY_4TH,      // red, white and blue waving, fireworks
  HALLOWEEN,     // orange and purple flicker, lightning
  THANKSGIVING,  // autumn: amber, orange and red leaves drifting
  PRIDE,         // the rainbow flag rolling out
  BIKE,          // white on the front (it faces forward), amber running lights on the sleeves
  BIKE_FLASH,    // daytime: bright white/amber flashes
  MUSIC,         // visualizer: the sleeves are level meters, beats shoot up the front (the app's microphone)
  MUSIC_BEAT,    // the whole jacket flashes a new colour on each beat
  HEARTBEAT,     // pulses out from the chest at your heart rate, coloured by zone (the app: a heart-rate band or an Apple Watch)
  LOOK_COUNT
};

struct Info { const char* name; Category cat; };
static const Info INFO[LOOK_COUNT] = {
    {"ROGUE AI", CAT_THEME},     {"LIGHT CYCLE", CAT_THEME}, {"CYBERPUNK", CAT_THEME},    {"CODE RAIN", CAT_THEME},
    {"SYNTHWAVE", CAT_THEME},    {"RAINBOW", CAT_THEME},     {"FIRE", CAT_THEME},         {"AURORA", CAT_THEME},
    {"OCEAN", CAT_THEME},        {"SCANNER", CAT_THEME},     {"SOLID", CAT_THEME},        {"BREATHE", CAT_THEME},
    {"SPARKLE", CAT_THEME},      {"COMET", CAT_THEME},       {"CHRISTMAS", CAT_HOLIDAY},  {"CANDY CANE", CAT_HOLIDAY},
    {"WINTER", CAT_HOLIDAY},     {"HANUKKAH", CAT_HOLIDAY},  {"NEW YEAR", CAT_HOLIDAY},   {"VALENTINE", CAT_HOLIDAY},
    {"ST PATRICK", CAT_HOLIDAY}, {"EASTER", CAT_HOLIDAY},    {"JULY 4TH", CAT_HOLIDAY},   {"HALLOWEEN", CAT_HOLIDAY},
    {"THANKSGIVING", CAT_HOLIDAY}, {"PRIDE", CAT_HOLIDAY},   {"BIKE", CAT_BIKE},          {"DAY FLASH", CAT_BIKE},
    {"MUSIC", CAT_LIVE},         {"MUSIC BEAT", CAT_LIVE},   {"HEARTBEAT", CAT_LIVE},
};

#ifndef LOOKS_INFO_ONLY  // the armband only needs the table above
void begin();               // restores the saved look, brightness, speed and colour
void tick(uint32_t now);    // every loop(), before leds::tick()
bool following();           // look ROGUE_AI: the costume's beacon runs the LEDs (main.cpp)

void command(uint8_t cmd, uint8_t a, uint8_t b, uint8_t c);  // jacket_link.h
void music(uint8_t bass, uint8_t mid, uint8_t treble, uint8_t level, bool beat);
void heartRate(uint8_t bpm);

// The status bytes (jacket_link.h, after the tag): look bright speed flags
// ackSeq r g b turn bpm.
void status(uint8_t out[10], uint8_t ackSeq, bool app, bool costume);
uint8_t look();
#endif

}  // namespace looks
