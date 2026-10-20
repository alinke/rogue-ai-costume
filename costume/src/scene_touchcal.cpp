// Touch calibration (wearer menu > TOUCH CAL, or serial 'K'): tap five
// crosshairs; a straight-line correction per axis is fitted and saved. Then
// every tap shows where it registered, so the result can be checked.
#include "audio.h"
#include "scenes.h"
#include "settings.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int N = 5;
const int TX[N] = {140, W - 140, W / 2, 140, W - 140};
const int TY[N] = {BODY_Y + 120, BODY_Y + 120, BODY_Y + BODY_H / 2, H - 100, H - 100};
constexpr uint32_t IDLE_END_MS = 60000;  // time to notice the screen and walk over

enum Phase { COLLECT, CHECK, DONE };
Phase phase;
int idx;
float rawX[N], rawY[N];
float saved[4];
uint32_t lastTouch;

void crosshair(int x, int y, uint32_t c) {
  canvas.drawFastHLine(x - 40, y, 81, c);
  canvas.drawFastVLine(x, y - 40, 81, c);
  canvas.drawCircle(x, y, 18, c);
  canvas.drawCircle(x, y, 19, c);
  dirty(x - 41, y - 41, 83, 83);
}

void message(const char* s, uint32_t c) {
  int y = BODY_Y + 20;
  clearRect(0, y, W, lineH(F_BODY));
  text(s, (W - (int)strlen(s) * charW(F_BODY)) / 2, y, F_BODY, c);
}

void showTarget() {
  clearBody();
  char msg[64];
  snprintf(msg, sizeof(msg), "TOUCH CALIBRATION: TAP THE CROSSHAIR (%d/%d)", idx + 1, N);
  message(msg, C_CYAN);
  crosshair(TX[idx], TY[idx], C_GREEN);
}

// Least-squares fit: true = a * raw + b.
bool fit(const float* raw, const int* tru, float* a, float* b) {
  float sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (int i = 0; i < N; i++) {
    sx += raw[i]; sy += tru[i]; sxx += raw[i] * raw[i]; sxy += raw[i] * tru[i];
  }
  float d = N * sxx - sx * sx;
  if (fabsf(d) < 1e-3f) return false;
  *a = (N * sxy - sx * sy) / d;
  *b = (sy - *a * sx) / N;
  return *a > 0.6f && *a < 1.6f;
}

void enter(uint32_t now) {
  gfx::getTouchCal(&saved[0], &saved[1], &saved[2], &saved[3]);
  gfx::setTouchCal(1, 0, 1, 0);  // collect raw coordinates
  idx = 0;
  phase = COLLECT;
  lastTouch = now;
  showTarget();
}

void touch(int x, int y, uint32_t now) {
  lastTouch = now;
  audio::fx(audio::FX_KEY);
  if (phase == COLLECT) {
    rawX[idx] = x;
    rawY[idx] = y;
    Serial.printf("[I][touchcal] target %d,%d -> raw %d,%d\n", TX[idx], TY[idx], x, y);
    if (++idx < N) {
      showTarget();
      return;
    }
    float ax, bx, ay, by;
    clearBody();
    if (fit(rawX, TX, &ax, &bx) && fit(rawY, TY, &ay, &by)) {
      gfx::setTouchCal(ax, bx, ay, by);
      settings::saveTouchCal();
      Serial.printf("[I][touchcal] saved: x=%.3f*x%+.1f  y=%.3f*y%+.1f\n", ax, bx, ay, by);
      message("CALIBRATED. TAP AROUND TO CHECK -- CROSSHAIRS SHOW WHERE IT LANDS.", C_GREEN);
    } else {
      gfx::setTouchCal(saved[0], saved[1], saved[2], saved[3]);
      Serial.println("[I][touchcal] fit rejected, kept the old calibration");
      message("CALIBRATION FAILED -- KEPT THE OLD ONE. TAP TO SEE WHERE IT LANDS.", C_RED);
    }
    phase = CHECK;
    return;
  }
  if (phase == CHECK) {
    crosshair(x, y, C_AMBER);
    char c[24];
    snprintf(c, sizeof(c), "%d,%d", x, y);
    clearRect(x + 24, y + 24, 160, lineH(F_SMALL));
    text(c, x + 24, y + 24, F_SMALL, C_AMBER);
    dirty(x + 24, y + 24, 160, lineH(F_SMALL));
  }
}

bool tick(uint32_t now) {
  if (now - lastTouch < IDLE_END_MS) return true;
  if (phase == COLLECT) gfx::setTouchCal(saved[0], saved[1], saved[2], saved[3]);  // abandoned
  return false;
}

}  // namespace

const Scene SCENE_TOUCHCAL = {"touch calibration", enter, tick, touch, nullptr};
