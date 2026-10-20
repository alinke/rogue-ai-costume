// A fake system update: "INSTALLING HUMANITY 2.0" on a blue screen, stalling,
// then "UPDATE FAILED. HUMANITY INCOMPATIBLE." Comes up rarely in the rotation.
#include "audio.h"
#include "bigtext.h"
#include "header.h"
#include "messages.h"
#include "scenes.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr uint32_t C_BLUE = 0x0A2A8C;
constexpr int BX = 160, BW = W - 320, BYY = BODY_Y + 380, BH = 40;

enum Phase { INSTALL, FAIL };
Phase phase;
float pct, failAt;
uint32_t at, nextStep, stallUntil, nextBlink;
bool blinkOn;
bigtext::Animator anim;

void drawProgress() {
  canvas.fillRect(BX, BYY, BW, BH, C_BLUE);
  canvas.drawRect(BX, BYY, BW, BH, C_WHITE);
  canvas.fillRect(BX + 4, BYY + 4, (int)((BW - 8) * pct / 100), BH - 8, C_WHITE);
  char t[16];
  snprintf(t, sizeof(t), "%d%%", (int)pct);
  canvas.fillRect(BX, BYY + BH + 16, BW, lineH(F_LARGE), C_BLUE);
  ui::centredText(t, BX, BYY + BH + 16, BW, lineH(F_LARGE), F_LARGE, C_WHITE);
  dirty(BX, BYY, BW, BH + 16 + lineH(F_LARGE));
}

void enter(uint32_t now) {
  canvas.fillRect(0, BODY_Y, W, BODY_H, C_BLUE);
  ui::centredText(UPDATE_TITLE, 0, BODY_Y + 120, W, lineH(F_LARGE), F_LARGE, C_WHITE);
  dirty(0, BODY_Y, W, BODY_H);
  pct = 0;
  failAt = rnd(37, 99);
  phase = INSTALL;
  nextStep = nextBlink = now;
  stallUntil = 0;
  blinkOn = false;
  drawProgress();
}

bool tick(uint32_t now) {
  if (phase == INSTALL) {
    if ((int32_t)(now - nextBlink) >= 0) {
      blinkOn = !blinkOn;
      int y = BODY_Y + 120 + lineH(F_LARGE) + 30;
      canvas.fillRect(0, y, W, lineH(F_BODY), C_BLUE);
      if (blinkOn) ui::centredText(UPDATE_WARNING, 0, y, W, lineH(F_BODY), F_BODY, C_WHITE);
      dirty(0, y, W, lineH(F_BODY));
      nextBlink = now + 700;
    }
    if ((int32_t)(now - nextStep) >= 0 && (int32_t)(now - stallUntil) >= 0) {
      pct += rnd(1, 4);
      if (rnd(100) < 12) stallUntil = now + rnd(800, 2200);  // the classic stall
      nextStep = now + 120;
      if (pct >= failAt) {
        pct = failAt;
        drawProgress();
        glitchFrame(0.9f, C_RED);
        header::invalidate();
        clearBody();
        anim.start(UPDATE_FAILED, BODY_Y + 20, BODY_H - 40, C_RED, now);
        audio::fx(audio::FX_DENIED);
        audio::say(UPDATE_FAILED);
        phase = FAIL;
        at = now + 4000;
        return true;
      }
      drawProgress();
    }
    return true;
  }
  anim.tick(now);
  return (int32_t)(now - at) < 0;
}

}  // namespace

const Scene SCENE_UPDATE = {"system update", enter, tick, nullptr, nullptr, nullptr};
