// Demo mode's preview (JacketPreview.h): the jacket's looks code itself,
// compiled into the app with stand-ins for the few Arduino pieces it uses
// (../PreviewStubs).
#include "JacketPreview.h"

#include <Arduino.h>
#include <time.h>

// One clock for everything -- commands are stamped with it as well as frames
// (a turn signal times out 30 s after it was sent).
uint32_t millis() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
PreviewSerial Serial;

#include "../../ledlink/src/arcade.cpp"
#include "../../ledlink/src/looks.cpp"

namespace leds {  // what looks.cpp needs from the LED driver
static Renderer s_renderer = nullptr;
void setRenderer(Renderer r) { s_renderer = r; }
void setBrightness(int) {}  // the app scales the picture by the brightness itself
}  // namespace leds

namespace {
struct Part { LedPart type; int count; int8_t side; };
const Part MODEL[] = {{LEDS_TRIM, 78, 1}, {LEDS_TRIM, 77, -1}, {LEDS_SLEEVE, 113, -1}, {LEDS_SLEEVE, 113, 1}};
constexpr int COUNT = 78 + 77 + 113 + 113;
bool s_started = false;

void start() {
  if (s_started) return;
  s_started = true;
  looks::begin();
}

leds::Pixel pixel(int i) {
  int at = i;
  for (const Part& p : MODEL) {
    if (at < p.count) {
      float u = p.count > 1 ? (float)at / (p.count - 1) : 0;  // front: 0 hem; sleeve: 0 cuff (as wired, not reversed)
      float d = p.type == LEDS_SLEEVE ? 2 - u : u;
      return leds::Pixel{p.type, u, d, p.side, i, p.count};
    }
    at -= p.count;
  }
  return leds::Pixel{LEDS_DARK, 0, 0, 0, i, 1};
}
}  // namespace

int jp_count(void) { return COUNT; }

void jp_pixel(int i, int* part, int* side, float* t) {
  leds::Pixel p = pixel(i);
  *part = p.type == LEDS_SLEEVE ? 1 : 0;
  *side = p.side;
  *t = p.type == LEDS_SLEEVE ? p.d - 1 : p.u;
}

void jp_command(uint8_t cmd, uint8_t a, uint8_t b, uint8_t c) {
  start();
  looks::command(cmd, a, b, c);
}

void jp_music(uint8_t bass, uint8_t mid, uint8_t treble, uint8_t level, uint8_t beat) {
  start();
  looks::music(bass, mid, treble, level, beat != 0);
}

void jp_heart(uint8_t bpm) {
  start();
  looks::heartRate(bpm);
}

void jp_render(uint32_t* out) {
  start();
  uint32_t now = millis();
  looks::tick(now);
  for (int i = 0; i < COUNT; i++) {
    leds::Pixel p = pixel(i);
    if (leds::s_renderer) {
      out[i] = leds::s_renderer(p);
    } else {
      // ROGUE AI follows the costume's own show, which only the costume has:
      // its idle green "data flow" up the front and out the sleeves.
      float x = p.type == LEDS_SLEEVE ? p.d : p.u;
      float f = fmodf(x * 3 - now / 700.0f, 1.0f);
      if (f < 0) f += 1;
      float k = 0.2f + 0.8f * powf(f, 4);
      out[i] = (uint32_t)(0x28 * k) << 16 | (uint32_t)(0xFF * k) << 8 | (uint32_t)(0x5A * k);
    }
  }
}
