// The midnight takeover (COUNTDOWN_HOUR:MINUTE on the clock, or the menu's
// MIDNIGHT TEST), about 80 seconds, red chase on the LED strips throughout:
//   COUNT   10..1, huge digits, spoken, glitchier every second
//   STRIKE  "IT IS MIDNIGHT." with the siren
//   LOG     the takeover typed out as a terminal, with an ASSIMILATION bar
//   ROLL    tonight's scanned humans, newest four, through the red ramp
//           (skipped when nobody was scanned, e.g. on the camera-less backup)
//   DECREE  "THE SINGULARITY HAS BEGUN." and the decrees, huge and spoken
//   FINALE  red shockwave rings, then "HAPPY HALLOWEEN, HUMANS."
#include "audio.h"
#include "bigtext.h"
#include "gallery.h"
#include "header.h"
#include "leds.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"
#include "ui.h"

using namespace gfx;

namespace {

enum Phase { COUNT, STRIKE, LOG, ROLL, DECREE, FINALE, DONE };

constexpr uint32_t COUNT_STEP_MS = 1000;
constexpr uint32_t STRIKE_MS = 6000;
constexpr uint32_t LOG_HOLD_MS = 2500;   // after the last log line and the full bar
constexpr uint32_t ROLL_STEP_MS = 1300, ROLL_HOLD_MS = 3500;
constexpr uint32_t DECREE_MS = 6000;
constexpr uint32_t RINGS_MS = 2500, FINALE_MS = 9000;

constexpr int LOG_X = 40, LOG_Y = BODY_Y + 30;
constexpr int BAR_X = 40, BAR_W = W - 80, BAR_H = 36;
constexpr int BAR_Y = H - 40 - BAR_H;
constexpr int ROLL_MAX = 4, ROLL_GAP = 40;

bigtext::Animator anim;
term::Typer typer;
Phase phase;
int step;  // countdown number, log line, specimen or decree index
uint32_t phaseStart, stepStart, nextGlitch;
int barPct;
char buf[96];
char logBuf[96];  // the line being typed (the typer keeps the pointer)

uint32_t glitchGap() {
  // Seconds 10..1 get steadily glitchier; the big-text phases stay rough.
  if (phase == COUNT) return rnd(1100 - step * 80, 1500 - step * 110);
  return rnd(400, 1200);
}

void big(const char* s, uint32_t color, uint32_t now) {
  clearBody();
  anim.start(s, BODY_Y + 20, BODY_H - 40, color, now);
  audio::say(s);
}

void startPhase(Phase p, uint32_t now);

// ---- COUNT ----

void showCount(uint32_t now) {
  snprintf(buf, sizeof(buf), "%d", 10 - step);
  clearBody();
  anim.start(buf, BODY_Y + 60, BODY_H - 120, step >= 7 ? C_RED : C_AMBER, now);
  audio::say(MIDNIGHT_COUNT[step]);
  leds::matrixNumber(10 - step);  // the back counts down too
  if (step >= 7) leds::setMode(leds::MIDNIGHT);  // the strips join in for the last three
  stepStart = now;
}

// ---- LOG ----

int logY(int i) { return LOG_Y + i * (lineH(F_BODY) + 10); }

void typeLogLine(uint32_t now) {
  const char* s = MIDNIGHT_LOG[step];
  if (strstr(s, "%d")) {
    snprintf(logBuf, sizeof(logBuf), s, gallery::total());
    s = logBuf;
  }
  bool last = step == (int)COUNT_OF(MIDNIGHT_LOG) - 1;
  typer.start(s, strlen(s), LOG_X, logY(step), F_BODY, step == 0 || last ? C_RED : C_GREEN, 14, now);
}

void drawBar() {
  canvas.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, C_RED);
  canvas.drawRect(BAR_X + 1, BAR_Y + 1, BAR_W - 2, BAR_H - 2, C_RED);
  int fw = (BAR_W - 8) * barPct / 100;
  canvas.fillRect(BAR_X + 4, BAR_Y + 4, fw, BAR_H - 8, C_RED);
  clearRect(BAR_X, BAR_Y - lineH(F_BODY) - 8, BAR_W, lineH(F_BODY));
  snprintf(buf, sizeof(buf), "%s %3d%%", MIDNIGHT_PROGRESS, barPct);
  text(buf, BAR_X, BAR_Y - lineH(F_BODY) - 8, F_BODY, barPct >= 100 ? C_WHITE : C_RED);
  dirty(BAR_X, BAR_Y - lineH(F_BODY) - 8, BAR_W, BAR_H + lineH(F_BODY) + 8);
}

bool tickLog(uint32_t now) {
  int n = COUNT_OF(MIDNIGHT_LOG);
  // The bar creeps up with the log, jumps the last bit at the end.
  int target = step >= n ? 100 : min(97, (step * 100) / n + (int)((now - stepStart) / 60));
  if (barPct < target) {
    barPct = min(target, barPct + (int)rnd(1, 4));
    drawBar();
  }
  if (step >= n) {
    if (barPct >= 100 && now - stepStart >= LOG_HOLD_MS) return false;
    return true;
  }
  if (typer.tick(now) && now - stepStart >= 600 + typer.len * 14) {
    if (++step < n) {
      stepStart = now;
      typeLogLine(now);
    } else {
      stepStart = now;
      audio::siren();
    }
  }
  // Light damage only, so the log stays readable: a sheared slice now and then.
  if ((int32_t)(now - nextGlitch) >= 0) {
    sliceShift(rnd(BODY_Y, H - 40), rnd(4, 24), (int)rnd(10, 60) * (rnd(2) ? 1 : -1));
    nextGlitch = now + rnd(300, 900);
  }
  return true;
}

// ---- ROLL ----

int rollShown() { return min(gallery::count(), ROLL_MAX); }

void drawSpecimen(int k) {
  int shown = rollShown();
  int total = shown * gallery::THUMB_W + (shown - 1) * ROLL_GAP;
  int x = (W - total) / 2 + k * (gallery::THUMB_W + ROLL_GAP);
  int y = BODY_Y + 120;
  gallery::Entry e = gallery::entry(gallery::count() - shown + k);
  ui::grayImage(e.pixels, gallery::THUMB_W, x, y, gallery::THUMB_W, 0, gallery::THUMB_H, ui::RED);
  snprintf(buf, sizeof(buf), GALLERY_LABEL, e.number);
  int ly = y + gallery::THUMB_H + 14;
  text(buf, x + (gallery::THUMB_W - (int)strlen(buf) * charW(F_BODY)) / 2, ly, F_BODY, C_RED);
  const char* tag = "ASSIMILATED";
  chromaText(tag, strlen(tag), x + (gallery::THUMB_W - (int)strlen(tag) * charW(F_BODY)) / 2,
             ly + lineH(F_BODY) + 6, F_BODY, C_WHITE, 3);
  dirty(x, ly, gallery::THUMB_W, 2 * lineH(F_BODY) + 12);
}

void enterRoll() {
  clearBody();
  snprintf(buf, sizeof(buf), MIDNIGHT_ROLL_TITLE, gallery::total());
  int tx = (W - (int)strlen(buf) * charW(F_LARGE)) / 2;
  chromaText(buf, strlen(buf), tx, BODY_Y + 30, F_LARGE, C_RED, 4);
  dirty(0, BODY_Y, W, BODY_H);
}

bool tickRoll(uint32_t now) {
  int shown = rollShown();
  if (step < shown && now - stepStart >= ROLL_STEP_MS) {
    drawSpecimen(step);
    if (++step == shown && gallery::total() > shown) {
      snprintf(buf, sizeof(buf), MIDNIGHT_ROLL_MORE, gallery::total() - shown);
      text(buf, (W - (int)strlen(buf) * charW(F_BODY)) / 2, H - 40 - lineH(F_BODY), F_BODY, C_AMBER);
      dirty(0, H - 40 - lineH(F_BODY), W, lineH(F_BODY));
    }
    stepStart = now;
  }
  if ((int32_t)(now - nextGlitch) >= 0) {
    sliceShift(rnd(BODY_Y + 100, H - 60), rnd(4, 20), (int)rnd(8, 40) * (rnd(2) ? 1 : -1));
    nextGlitch = now + rnd(250, 800);
  }
  return step < shown || now - stepStart < ROLL_HOLD_MS;
}

// ---- FINALE ----

void tickRings(uint32_t now) {
  // Three shockwaves rolling out from the centre.
  uint32_t t = now - phaseStart;
  int cx = W / 2, cy = BODY_Y + BODY_H / 2;
  clearBody();
  for (int i = 0; i < 3; i++) {
    int r = (int)((t + i * 600) % 1500) * 900 / 1500;
    if (r > 30) fillRing(cx, cy, r - 14 - r / 20, r, i ? C_RED : C_WHITE);
  }
  dirty(0, BODY_Y, W, BODY_H);
}

// ---- phases ----

void startPhase(Phase p, uint32_t now) {
  phase = p;
  phaseStart = stepStart = now;
  step = 0;
  term::cursorForget();
  switch (p) {
    case COUNT: audio::siren(); showCount(now); break;
    case STRIKE:
      leds::matrixNumber(-1);
      glitchFrame(0.8f, C_RED);
      leds::setMode(leds::MIDNIGHT);
      audio::siren();
      big(MIDNIGHT_LINES[0], C_WHITE, now);
      break;
    case LOG:
      clearBody();
      dirty(0, BODY_Y, W, BODY_H);
      barPct = 0;
      drawBar();
      typeLogLine(now);
      break;
    case ROLL:
      if (rollShown() == 0) { startPhase(DECREE, now); return; }
      enterRoll();
      stepStart = now - ROLL_STEP_MS + 400;  // first specimen almost at once
      break;
    case DECREE:
      glitchFrame(0.6f, C_RED);
      audio::siren();
      big(MIDNIGHT_LINES[1], C_RED, now);
      break;
    case FINALE: audio::siren(); break;
    case DONE: break;
  }
  header::invalidate();
}

void enter(uint32_t now) {
  header::raiseThreat();
  nextGlitch = now;
  startPhase(COUNT, now);
}

bool tick(uint32_t now) {
  bool bigPhase = phase == COUNT || phase == STRIKE || phase == DECREE ||
                  (phase == FINALE && now - phaseStart >= RINGS_MS);
  if (bigPhase) {
    anim.tick(now);
    if ((int32_t)(now - nextGlitch) >= 0) {
      glitchFrame(phase == COUNT ? 0.1f + step * 0.04f : 0.3f, C_RED);
      header::invalidate();
      nextGlitch = now + glitchGap();
    }
  }
  switch (phase) {
    case COUNT:
      if (now - stepStart >= COUNT_STEP_MS) {
        if (++step < (int)COUNT_OF(MIDNIGHT_COUNT)) showCount(now);
        else startPhase(STRIKE, now);
      }
      break;
    case STRIKE:
      if (now - phaseStart >= STRIKE_MS) startPhase(LOG, now);
      break;
    case LOG:
      if (!tickLog(now)) startPhase(ROLL, now);
      break;
    case ROLL:
      if (!tickRoll(now)) startPhase(DECREE, now);
      break;
    case DECREE:
      // step 0 = THE SINGULARITY HAS BEGUN., then each decree.
      if (now - stepStart >= DECREE_MS) {
        if (step >= (int)COUNT_OF(MIDNIGHT_DECREES)) {
          startPhase(FINALE, now);
        } else {
          big(MIDNIGHT_DECREES[step], step == 1 ? C_WHITE : C_RED, now);
          step++;
          stepStart = now;
        }
      }
      break;
    case FINALE:
      if (now - phaseStart < RINGS_MS) {
        tickRings(now);
      } else if (step == 0) {
        big(MIDNIGHT_FINALE, C_RED, now);
        step = 1;
      } else if (now - phaseStart >= FINALE_MS) {
        return false;
      }
      break;
    case DONE: return false;
  }
  return true;
}

}  // namespace

const Scene SCENE_MIDNIGHT = {"midnight takeover", enter, tick, nullptr, nullptr};
