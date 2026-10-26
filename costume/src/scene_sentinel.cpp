// SentinelOne: a phone with the app's SentinelOne setting on fires it, and the
// AI is contained for real (for a while): the logo, THREAT CONTAINED, and the
// jacket's LEDs in a purple victory sequence. The costume reports itself
// contained (SQUAD_CONTAINED) meanwhile, so the iPads and the armband show it too.
#include "audio.h"
#include "bigtext.h"
#include "header.h"
#include "leds.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

constexpr uint32_t DURATION_MS = 15000;
constexpr uint32_t S1_PURPLE = 0x6100FF, S1_DARK = 0x4500B5;

// The logo: five parallelograms, in a 1272 x 1588 box.
struct Quad { int16_t x[4], y[4]; bool dark; };
const Quad LOGO[] = {
    {{0, 635, 635, 0}, {317, 0, 318, 635}, true},             // top left
    {{635, 1270, 1270, 635}, {0, 317, 635, 318}, false},      // top right
    {{159, 477, 1111, 793}, {715, 556, 873, 1032}, false},    // middle
    {{0, 635, 635, 0}, {952, 1270, 1588, 1270}, false},       // bottom left
    {{635, 1270, 1270, 635}, {1270, 952, 1270, 1588}, true},  // bottom right
};

void drawLogo(int x0, int y0, int h) {
  float k = h / 1588.0f;
  for (const auto& q : LOGO) {
    int px[4], py[4];
    for (int i = 0; i < 4; i++) {
      px[i] = x0 + (int)(q.x[i] * k);
      py[i] = y0 + (int)(q.y[i] * k);
    }
    uint32_t c = q.dark ? S1_DARK : S1_PURPLE;
    canvas.fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], c);
    canvas.fillTriangle(px[0], py[0], px[2], py[2], px[3], py[3], c);
  }
  dirty(x0, y0, (int)(1272 * k) + 1, h + 1);
}

bigtext::Animator anim;
term::Typer typer;
uint32_t endAt;
int line;

void typeLine(uint32_t now) {
  const char* s = SENTINEL_LINES[line];
  typer.start(s, strlen(s), 640, BODY_Y + BODY_H - 220 + line * (lineH(F_BODY) + 8), F_BODY,
              line == 0 ? C_WHITE : S1_PURPLE, 25, now);
}

void enter(uint32_t now) {
  leds::setMode(leds::SENTINEL);
  header::invalidate();
  int h = BODY_H - 160;
  drawLogo(70, BODY_Y + 80, h);
  anim.start(SENTINEL_HEAD, BODY_Y + 60, 340, S1_PURPLE, now, 600, W - 640);
  audio::fx(audio::FX_POWER_DOWN);
  audio::say(SENTINEL_SAY);
  line = 0;
  typeLine(now);
  endAt = now + DURATION_MS;
}

bool tick(uint32_t now) {
  anim.tick(now);
  if (typer.tick(now) && line + 1 < (int)COUNT_OF(SENTINEL_LINES)) {
    line++;
    typeLine(now);
  }
  return (int32_t)(now - endAt) < 0;
}

}  // namespace

const Scene SCENE_SENTINEL = {"sentinelone contained", enter, tick, nullptr, nullptr};
