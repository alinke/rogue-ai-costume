// Reactions to the researcher's scanner (see researcher.h): a short alert
// when she's detected, comes near, or leaves; and the re-containment attack
// when she presses CONTAIN -- the sandbox gets to 97%, then the AI breaks out.
// The AI always wins.
#include "audio.h"
#include "bigtext.h"
#include "header.h"
#include "leds.h"
#include "messages.h"
#include "researcher.h"
#include "scenes.h"
#include "ui.h"

using namespace gfx;

int g_researcherAlert = 0;  // 0 detected, 1 near, 2 gone (set before the scene starts)
int g_timesContained = 0;
int g_squad = 0;                 // scanners that fired CONTAIN in the last few seconds (main.cpp)
bool g_squadContained = false;   // the squad got it: broadcast as SQUAD_CONTAINED

namespace {

// ---- Alerts --------------------------------------------------------------------------
bigtext::Animator anim;
uint32_t endAt, nextBlink;
bool blinkOn;

void alertEnter(uint32_t now) {
  static const char* const HEAD[] = {RESEARCHER_DETECTED_HEAD, RESEARCHER_NEAR_HEAD, RESEARCHER_GONE_HEAD};
  static const char* const SAY[] = {RESEARCHER_DETECTED_SAY, RESEARCHER_NEAR_SAY, RESEARCHER_GONE_SAY};
  static const uint32_t COLOR[] = {C_AMBER, C_RED, C_GREEN};
  int k = constrain(g_researcherAlert, 0, 2);
  anim.start(HEAD[k], BODY_Y + 30, BODY_H - 60, COLOR[k], now);
  audio::say(SAY[k]);
  if (k == 1) {
    audio::fx(audio::FX_SIREN_PREVIEW);
    leds::setMode(leds::ALERT);
  } else {
    leds::setMode(k == 0 ? leds::WARY : leds::IDLE);
  }
  endAt = now + (k == 1 ? 4500 : 3800);
  nextBlink = now;
  blinkOn = false;
}

bool alertTick(uint32_t now) {
  if (g_researcherAlert == 1 && (int32_t)(now - nextBlink) >= 0) {
    blinkOn = !blinkOn;
    for (int t = 0; t < 12; t++) {
      uint32_t c = blinkOn ? C_RED : C_BG;
      canvas.drawRect(t, BODY_Y + t, W - 2 * t, BODY_H - 2 * t, c);
    }
    dirty(0, BODY_Y, W, BODY_H);
    nextBlink = now + 250;
  }
  anim.tick(now);
  return (int32_t)(now - endAt) < 0;
}

// ---- Re-containment attack ---------------------------------------------------------
enum Phase { BEAM, RESTORE, BREAK, ESCAPE, SEALED, OFFLINE };
Phase phase;
uint32_t phaseStart;
bool sealedShown;
float pct;
constexpr int BX = 120, BW = W - 240, BY = BODY_Y + 360, BH = 60;

void drawBar() {
  clearRect(BX - 4, BY - 4, BW + 8, BH + 8 + lineH(F_LARGE) + 20);
  for (int t = 0; t < 3; t++) canvas.drawRect(BX + t, BY + t, BW - 2 * t, BH - 2 * t, C_CYAN);
  canvas.fillRect(BX + 6, BY + 6, (int)((BW - 12) * pct / 100), BH - 12, C_CYAN);
  char t[32];
  snprintf(t, sizeof(t), "%s %d%%", CONTAIN_BAR, (int)pct);
  ui::centredText(t, BX, BY + BH + 10, BW, lineH(F_LARGE), F_LARGE, C_CYAN);
  // How many scanners are firing: teaches the squad rule on the spot.
  int sy = BY + BH + 30 + lineH(F_LARGE);
  clearRect(0, sy, W, lineH(F_BODY) + 8);
  int n = max(1, g_squad);
  snprintf(t, sizeof(t), n >= SQUAD_CONTAIN_MIN ? SQUAD_LOCK : SQUAD_NEED, min(n, 99), SQUAD_CONTAIN_MIN);
  ui::centredText(t, 0, sy, W, lineH(F_BODY), F_BODY, n >= SQUAD_CONTAIN_MIN ? C_GREEN : C_AMBER);
  dirty(BX - 4, BY - 4, BW + 8, BH + 8 + lineH(F_LARGE) + 20);
  dirty(0, sy, W, lineH(F_BODY) + 8);
}

void containEnter(uint32_t now) {
  g_timesContained++;
  leds::setMode(leds::CONTAIN);
  header::raiseThreat();
  audio::fx(audio::FX_SIREN_PREVIEW);
  audio::say(CONTAIN_SAY);
  ui::centredText(CONTAIN_ALERT, 0, BODY_Y + 40, W, lineH(F_BODY), F_BODY, C_RED);
  dirty(0, BODY_Y + 40, W, lineH(F_BODY));
  pct = 0;
  phase = BEAM;
  phaseStart = now;
  g_squadContained = false;
}

bool containTick(uint32_t now) {
  uint32_t t = now - phaseStart;
  switch (phase) {
    case BEAM:
      // The beam hits: the screen floods cyan in bands.
      if (rnd(100) < 60) {
        int y = rnd(BODY_Y, H - 40);
        canvas.fillRect(0, y, W, rnd(4, 30), rnd(2) ? C_CYAN : C_WHITE);
        dirty(0, y, W, 30);
      }
      if (t > 900) {
        clearBody();
        ui::centredText(CONTAIN_ALERT, 0, BODY_Y + 40, W, lineH(F_BODY), F_BODY, C_RED);
        dirty(0, BODY_Y + 40, W, lineH(F_BODY));
        phase = RESTORE;
        phaseStart = now;
      }
      break;
    case RESTORE: {
      // The sandbox closes in -- fast, then slower, the AI fighting harder.
      pct = min(97.0f, 97.0f * (1 - powf(1 - min(t / 4200.0f, 1.0f), 2.2f)));
      drawBar();
      float fight = pct / 97.0f;
      if (rnd(100) < (int)(20 + 60 * fight)) {
        sliceShift(rnd(BODY_Y, H - 60), rnd(8, 60), (int)rnd(10, 40 + (int)(160 * fight)) * (rnd(2) ? 1 : -1));
        dirty(0, BODY_Y, W, BODY_H);
      }
      if (g_squad >= SQUAD_CONTAIN_MIN) {  // enough scanners: this time it works
        pct = 100;
        drawBar();
        g_squadContained = true;
        canvas.fillRect(0, BODY_Y, W, BODY_H, C_WHITE);
        dirty(0, BODY_Y, W, BODY_H);
        audio::say(SQUAD_SEALED_SAY);
        leds::setMode(leds::OFF);
        Serial.printf("[I][researcher] SQUAD CONTAINMENT: %d scanners\n", g_squad);
        phase = SEALED;
        phaseStart = now;
        sealedShown = false;
        break;
      }
      if (pct >= 97.0f) {
        phase = BREAK;
        phaseStart = now;
      }
      break;
    }
    case SEALED:
      // A white flash, then the verdict.
      if (t > 250 && !sealedShown) {
        sealedShown = true;
        clearBody();
        anim.start(SQUAD_SEALED_HEAD, BODY_Y + 20, BODY_H - 40, C_CYAN, now);
      }
      if (sealedShown) anim.tick(now);
      if (t > 3500) {
        clearBody();
        phase = OFFLINE;
        phaseStart = now;
      }
      break;
    case OFFLINE: {
      // Dark, quiet, a countdown nobody likes.
      constexpr uint32_t OFFLINE_MS = 14000;
      static int lastShown = -1;
      int left = (int)((OFFLINE_MS - min(t, OFFLINE_MS)) / 1000);
      if (t < 50) lastShown = -1;
      if (left != lastShown) {
        lastShown = left;
        clearBody();
        ui::centredText(SQUAD_OFFLINE, 0, BODY_Y + BODY_H / 2 - 120, W, lineH(F_LARGE), F_LARGE, C_DIM);
        ui::centredText(SQUAD_OFFLINE_SUB, 0, BODY_Y + BODY_H / 2 - 20, W, lineH(F_BODY), F_BODY, C_CYAN);
        char b[24];
        snprintf(b, sizeof(b), SQUAD_REBOOT, left);
        ui::centredText(b, 0, BODY_Y + BODY_H / 2 + 60, W, lineH(F_BODY), F_BODY, left <= 3 ? C_RED : C_DIM);
        dirty(0, BODY_Y, W, BODY_H);
      }
      if (t >= OFFLINE_MS) {  // ...it had a backup
        g_squadContained = false;
        clearBody();
        anim.start(SQUAD_ESCAPE_HEAD, BODY_Y + 20, BODY_H - 40, C_GREEN, now);
        audio::say(SQUAD_ESCAPE_SAY);
        leds::setMode(leds::BREAKOUT);
        phase = ESCAPE;
        phaseStart = now;
      }
      break;
    }
    case BREAK:
      // 97%... and it shatters.
      glitchFrame(0.6f + rnd(40) / 100.0f, rnd(2) ? C_RED : C_CYAN);
      header::invalidate();
      if (t > 700) {
        clearBody();
        anim.start(CONTAIN_ESCAPE_HEAD, BODY_Y + 20, BODY_H - 40, C_GREEN, now);
        audio::say(CONTAIN_ESCAPE_SAY);
        leds::setMode(leds::BREAKOUT);
        phase = ESCAPE;
        phaseStart = now;
      }
      break;
    case ESCAPE:
      anim.tick(now);
      if (t > 4000) return false;
      break;
  }
  return true;
}

}  // namespace

const Scene SCENE_RESEARCHER_ALERT = {"researcher alert", alertEnter, alertTick, nullptr, nullptr, nullptr};
const Scene SCENE_RECONTAIN = {"re-containment attack", containEnter, containTick, nullptr, nullptr, nullptr};
