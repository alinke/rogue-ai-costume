// "RE-CONTAIN THE AI": drag the handle to 100% to put the AI back in its
// sandbox. Just before the end it always slips back. Two tries.
#include "audio.h"
#include "bigtext.h"
#include "config.h"
#include "messages.h"
#include "scenes.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int TX = 120, TW = W - 240, TY = BODY_Y + 330, TH = 70;  // track
constexpr int HW = 70, HH = 150;                                  // handle

enum Phase { DRAGGING, SLIPPING, OVER, TIMEOUT };
Phase phase;
float pos;  // 0..1
int tries;
uint32_t at, lastTouch, slipStart;
bool grabbed;
bigtext::Animator anim;

void drawTrack() {
  clearRect(TX - HW, TY - (HH - TH) / 2 - 4, TW + 2 * HW, HH + 8 + 2 * lineH(F_LARGE));
  for (int t = 0; t < 3; t++) canvas.drawRect(TX + t, TY + t, TW - 2 * t, TH - 2 * t, C_DIM);
  int fill = (int)((TW - 12) * pos);
  canvas.fillRect(TX + 6, TY + 6, fill, TH - 12, pos > 0.9f ? C_AMBER : C_GREEN);
  int hx = TX + (int)(TW * pos) - HW / 2, hy = TY + TH / 2 - HH / 2;
  canvas.fillRoundRect(hx, hy, HW, HH, 12, C_WHITE);
  for (int k = -1; k <= 1; k++) canvas.drawFastVLine(hx + HW / 2 + k * 12, hy + 30, HH - 60, C_BG);
  char pct[16];
  snprintf(pct, sizeof(pct), "%d%%", (int)(pos * 100 + 0.5f));
  ui::centredText(pct, TX, TY + HH, TW, lineH(F_LARGE) + 20, F_LARGE, pos > 0.9f ? C_AMBER : C_GREEN);
  dirty(TX - HW, TY - (HH - TH) / 2 - 4, TW + 2 * HW, HH + 8 + 2 * lineH(F_LARGE));
}

void enter(uint32_t now) {
  text(SANDBOX_TITLE, TX, BODY_Y + 40, F_LARGE, C_AMBER);
  text(SANDBOX_HINT, TX, BODY_Y + 40 + lineH(F_LARGE), F_BODY, C_WHITE);
  dirty(0, BODY_Y, W, 200);
  pos = 0;
  tries = 2;
  grabbed = false;
  phase = DRAGGING;
  lastTouch = now;
  drawTrack();
}

void drag(int x, int y, bool down, uint32_t now) {
  lastTouch = now;
  if (phase != DRAGGING) return;
  if (!down) {
    grabbed = false;
    return;
  }
  if (y < TY - HH || y > TY + TH + HH) return;  // only near the track
  grabbed = true;
  float p = constrain((float)(x - TX) / TW, 0.0f, 1.0f);
  if (p <= pos + 0.25f) pos = max(pos, p);  // no teleporting to the end
  if (pos > 0.96f) {
    // So close. The AI fights back.
    pos = 0.99f;
    drawTrack();
    phase = SLIPPING;
    slipStart = now;
    audio::fx(audio::FX_DENIED);
    audio::say(SANDBOX_FAIL);
    return;
  }
  drawTrack();
}

bool tick(uint32_t now) {
  switch (phase) {
    case DRAGGING:
      if (now - lastTouch >= INTERACTIVE_TIMEOUT_MS) {
        phase = TIMEOUT;
        at = now + 1500;
      }
      break;
    case SLIPPING: {
      // Slide back to zero with a couple of glitches.
      float t = (now - slipStart) / 900.0f;
      pos = 0.99f * (1 - constrain(t, 0.0f, 1.0f)) * (1 - constrain(t, 0.0f, 1.0f));
      drawTrack();
      if (rnd(100) < 25) sliceShift(TY - 40, 150, (int)rnd(20, 80) * (rnd(2) ? 1 : -1));
      if (t >= 1) {
        pos = 0;
        drawTrack();
        if (--tries > 0) {
          phase = DRAGGING;
          lastTouch = now;
        } else {
          clearBody();
          anim.start(SANDBOX_FAIL, BODY_Y + 20, BODY_H - 40, C_RED, now);
          phase = OVER;
          at = now + 2500;
        }
      }
      break;
    }
    case OVER:
      anim.tick(now);
      if ((int32_t)(now - at) >= 0) return false;
      break;
    case TIMEOUT:
      if ((int32_t)(now - at) >= 0) return false;
      break;
  }
  return true;
}

}  // namespace

const Scene SCENE_SANDBOX = {"sandbox drag", enter, tick, nullptr, nullptr, drag};
