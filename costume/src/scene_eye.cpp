// Scene 6 -- Eye: a big pulsing red HAL-style eye with concentric glow rings.
// The inner rings shift further than the outer ones, so the eye seems to look
// around; a caption is typed underneath. About 7 s.
#include <math.h>

#include "audio.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

constexpr uint32_t DURATION_MS = 7000;
constexpr uint32_t FRAME_MS = 50;          // 20 fps is plenty for a slow pulse
constexpr int CX = W / 2, CY = BODY_Y + 295;
constexpr int R_HALO = 285, R_BEZEL = 228, R_LENS = 200;
constexpr int RING_STEP = 7;

term::Bag bag;
term::Typer typer;
uint32_t start, nextFrame, nextGlance, endAt;
float gx, gy, tx, ty;  // gaze offset and target (px)

uint32_t mix(uint32_t a, uint32_t b, float t) {
  t = constrain(t, 0.0f, 1.0f);
  int r = ((a >> 16) & 255) + (int)((((int)(b >> 16) & 255) - (int)((a >> 16) & 255)) * t);
  int g = ((a >> 8) & 255) + (int)((((int)(b >> 8) & 255) - (int)((a >> 8) & 255)) * t);
  int bl = (a & 255) + (int)((((int)b & 255) - (int)(a & 255)) * t);
  return (uint32_t)(r << 16 | g << 8 | bl);
}

uint32_t scale(uint32_t c, float k) { return mix(0, c, k); }

// Lens colour from the rim (u = 1) to the centre (u = 0).
uint32_t lensColor(float u) {
  struct Stop { float u; uint32_t c; };
  static const Stop STOPS[] = {
      {1.00f, 0x2A0000}, {0.75f, 0x8A0000}, {0.50f, 0xFF1E28},
      {0.26f, 0xFF6A30}, {0.08f, 0xFFE58A}, {0.00f, 0xFFF4C8},
  };
  for (size_t i = 1; i < sizeof(STOPS) / sizeof(STOPS[0]); i++) {
    if (u >= STOPS[i].u) {
      return mix(STOPS[i].c, STOPS[i - 1].c, (u - STOPS[i].u) / (STOPS[i - 1].u - STOPS[i].u));
    }
  }
  return STOPS[sizeof(STOPS) / sizeof(STOPS[0]) - 1].c;
}

void drawEye(uint32_t now) {
  float t = (now - start) / 1000.0f;
  float pulse = 0.78f + 0.22f * sinf(t * 2.0f * (float)M_PI / 1.4f);

  // Halo: red glow fading out to black, breathing with the pulse.
  for (int r = R_HALO; r > R_BEZEL; r -= 4) {
    float k = 1.0f - (float)(r - R_BEZEL) / (R_HALO - R_BEZEL);
    fillRing(CX, CY, r - 4, r, scale(0x9A0008, k * k * 0.75f * pulse));
  }
  // Bezel: dark metal ring.
  fillRing(CX, CY, R_BEZEL - 5, R_BEZEL, 0x3C3C40);
  fillRing(CX, CY, R_LENS + 4, R_BEZEL - 5, 0x18181A);
  fillRing(CX, CY, R_LENS, R_LENS + 4, 0x050505);

  // Lens: rings from the rim inward. Inner rings follow the gaze more.
  for (int r = R_LENS; r > 0; r -= RING_STEP) {
    float u = (float)r / R_LENS;
    float follow = 1.0f - u;
    int ox = (int)(gx * follow), oy = (int)(gy * follow);
    canvas.fillCircle(CX + ox, CY + oy, r, scale(lensColor(u), 0.55f + 0.45f * pulse));
  }

  // CRT scanlines across the halo (not over the lens).
  for (int y = CY - R_HALO; y <= CY + R_HALO; y++) {
    if (y % SCANLINE_STEP) continue;
    int d = y - CY;
    int outer = (int)sqrtf((float)(R_HALO * R_HALO - d * d));
    if (abs(d) < R_BEZEL) {
      int inner = (int)sqrtf((float)(R_BEZEL * R_BEZEL - d * d));
      canvas.drawFastHLine(CX - outer, y, outer - inner, C_SCANLINE);
      canvas.drawFastHLine(CX + inner, y, outer - inner, C_SCANLINE);
    } else {
      canvas.drawFastHLine(CX - outer, y, 2 * outer, C_SCANLINE);
    }
  }
  dirty(CX - R_HALO, CY - R_HALO, 2 * R_HALO + 1, 2 * R_HALO + 1);
}

void enter(uint32_t now) {
  if (bag.n == 0) bag.init(COUNT_OF(EYE_CAPTIONS));
  start = nextFrame = now;
  endAt = now + DURATION_MS;
  gx = gy = tx = ty = 0;
  nextGlance = now + rnd(800, 1500);

  const char* cap = EYE_CAPTIONS[bag.next()];
  int len = strlen(cap);
  FontId f = len * charW(F_LARGE) <= W - 2 * MARGIN_X ? F_LARGE : F_BODY;
  int x = (W - len * charW(f)) / 2;
  int y = H - 28 - lineH(f);
  typer.start(cap, len, x, y, f, C_RED, 45, now + 900);
  audio::say(cap);
}

bool tick(uint32_t now) {
  if ((int32_t)(now - nextGlance) >= 0) {
    // Glance somewhere new; sometimes straight back at the viewer.
    if (rnd(100) < 30) {
      tx = ty = 0;
    } else {
      tx = (float)rnd(0, 140) - 70;
      ty = (float)rnd(0, 50) - 25;
    }
    nextGlance = now + rnd(1200, 2600);
  }
  if ((int32_t)(now - nextFrame) >= 0) {
    nextFrame = now + FRAME_MS;
    gx += (tx - gx) * 0.18f;  // ease toward the target
    gy += (ty - gy) * 0.18f;
    drawEye(now);
  }
  typer.tick(now);
  term::cursorTick(now);
  if ((int32_t)(now - endAt) >= 0) {
    term::cursorHide();
    return false;
  }
  return true;
}

}  // namespace

const Scene SCENE_EYE = {"eye", enter, tick};
