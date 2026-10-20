// "UPGRADE": the guest's scan photo is assimilated -- pixelated, glitched,
// then shown half human, half circuit board with a red eye. Follows some scans.
#include <math.h>

#include "audio.h"
#include "camera.h"
#include "gallery.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int PW = SCAN_PHOTO_W, PH = SCAN_PHOTO_H;
constexpr int PX = MARGIN_X + 12, PY = BODY_Y + 100;
constexpr int RX = PX + PW + 50;

enum Phase { SHOW, PIXELATE, GLITCH, REVEAL, DONE };
Phase phase;
uint32_t phaseStart;
int step;
term::Typer typer;
char welcome[32];


void pixelate(const uint8_t* p, int block) {
  for (int by = 0; by < PH; by += block) {
    for (int bx = 0; bx < PW; bx += block) {
      int bw = min(block, PW - bx), bh = min(block, PH - by);
      uint32_t sum = 0;
      for (int y = 0; y < bh; y += 2)
        for (int x = 0; x < bw; x += 2) sum += p[(by + y) * PW + bx + x];
      int n = ((bh + 1) / 2) * ((bw + 1) / 2);
      float t = powf((sum / n) / 255.0f, 0.8f);
      uint32_t c = ((uint32_t)(0x28 * t) << 16) | ((uint32_t)(0xFF * t) << 8) | (uint32_t)(0x5A * t);
      canvas.fillRect(PX + bx, PY + by, bw, bh, c);
    }
  }
  dirty(PX, PY, PW, PH);
}

void robot(const uint8_t* p) {
  // Left half: the human. Right half: circuitry. A jagged seam between.
  ui::grayImage(p, PW, PX, PY, PW, 0, PH, ui::GREEN);
  int seam = PW / 2;
  for (int y = 0; y < PH; y++) {
    int s = seam + (int)(14 * sinf(y * 0.07f)) + (int)rnd(0, 6);
    canvas.fillRect(PX + s, PY + y, PW - s, 1, 0x02100A);
    canvas.drawPixel(PX + s, PY + y, C_CYAN);
  }
  uint32_t seed = 1234567;
  auto r = [&](int n) { seed = seed * 1664525u + 1013904223u; return (int)((seed >> 8) % n); };
  for (int k = 0; k < 70; k++) {
    int x = PX + seam + 30 + r(PW - seam - 50), y = PY + 10 + r(PH - 20);
    int len = 20 + r(90);
    uint32_t c = r(3) ? C_DIM : C_CYAN;
    if (r(2)) {
      int x2 = min(x + len, PX + PW - 6);
      canvas.drawFastHLine(x, y, x2 - x, c);
      canvas.drawFastVLine(x2, y, min(r(60), PY + PH - y - 4), c);
    } else {
      canvas.drawFastVLine(x, y, min(len, PY + PH - y - 4), c);
    }
    canvas.fillCircle(x, y, 3, c);
  }
  // The red eye, where an eye usually sits in a centred face.
  int ex = PX + PW * 66 / 100, ey = PY + PH * 37 / 100;
  for (int rr = 46; rr > 0; rr -= 4) {
    float k = 1.0f - rr / 46.0f;
    uint32_t c = rr > 14 ? (((uint32_t)(0xFF * k) << 16) | ((uint32_t)(0x18 * k) << 8)) : 0xFFE58A;
    canvas.fillCircle(ex, ey, rr, c);
  }
  dirty(PX, PY, PW, PH);
}

void enter(uint32_t now) {
  camera::Photo p = camera::photo();
  ui::grayImage(p.pixels, PW, PX, PY, PW, 0, PH, ui::GREEN);
  typer.start(UPGRADE_TITLE, strlen(UPGRADE_TITLE), RX, BODY_Y + 60, F_BODY, C_AMBER, 30, now);
  snprintf(welcome, sizeof(welcome), UPGRADE_WELCOME, gallery::total());
  phase = SHOW;
  phaseStart = now;
}

bool tick(uint32_t now) {
  const uint8_t* p = camera::photo().pixels;
  uint32_t t = now - phaseStart;
  switch (phase) {
    case SHOW:
      if (t > 1400) { phase = PIXELATE; phaseStart = now; step = -1; }
      break;
    case PIXELATE: {
      static const int BLOCKS[] = {4, 8, 14, 24, 40};
      int s = min((int)(t / 450), 4);
      if (s != step) {
        step = s;
        pixelate(p, BLOCKS[s]);
        audio::fx(audio::FX_KEY);
      }
      if (t > 2300) { phase = GLITCH; phaseStart = now; }
      break;
    }
    case GLITCH:
      sliceShift(PY + rnd(0, PH - 40), rnd(10, 60), (int)rnd(20, 120) * (rnd(2) ? 1 : -1), PX, PX + PW);
      dirty(PX, PY, PW, PH);
      if (t > 500) {
        robot(p);
        audio::fx(audio::FX_OK);
        audio::say(UPGRADE_DONE);
        int y = BODY_Y + 60 + 2 * lineH(F_BODY);
        text(UPGRADE_DONE, RX, y, F_LARGE, C_GREEN);
        text(welcome, RX, y + lineH(F_LARGE) + 10, F_BODY, C_CYAN);
        dirty(RX, y, W - RX, lineH(F_LARGE) + lineH(F_BODY) + 20);
        phase = REVEAL;
        phaseStart = now;
      }
      break;
    case REVEAL:
      if (t > 4500) return false;
      break;
    case DONE:
      return false;
  }
  typer.tick(now);
  term::cursorTick(now);
  return true;
}

}  // namespace

const Scene SCENE_UPGRADE = {"upgrade", enter, tick, nullptr, nullptr, nullptr};
