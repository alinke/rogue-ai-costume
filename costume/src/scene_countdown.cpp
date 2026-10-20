// "SINGULARITY IN hh:mm:ss" -- counting down to COUNTDOWN_HOUR:MINUTE on the
// real-time clock (the takeover itself is SCENE_MIDNIGHT). Within a few hours
// after, it counts up instead ("SINGULARITY ACHIEVED +hh:mm:ss").
#include "bigtext.h"
#include "messages.h"
#include "rtclock.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

constexpr uint32_t DURATION_MS = 8000;
constexpr int32_t SHOW_BEFORE_S = 12 * 3600, SHOW_AFTER_S = 4 * 3600;

bigtext::Animator anim;
term::Typer typer;
uint32_t endAt;
int shownSecond = -1;
char digits[24];

bool counting() { return rtclock::secondsToTarget() <= SHOW_BEFORE_S; }

void update(uint32_t now) {
  int32_t s = counting() ? rtclock::secondsToTarget() : rtclock::secondsSinceTarget();
  snprintf(digits, sizeof(digits), "%s%02d:%02d:%02d", counting() ? "" : "+", (int)(s / 3600),
           (int)(s / 60 % 60), (int)(s % 60));
  anim.start(digits, BODY_Y + 120, BODY_H - 260, counting() && s < 600 ? C_RED : C_GREEN, now);
  shownSecond = rtclock::second();
}

void enter(uint32_t now) {
  const char* title = counting() ? COUNTDOWN_TITLE : "SINGULARITY ACHIEVED";
  int x = (W - (int)strlen(title) * charW(F_LARGE)) / 2;
  text(title, x, BODY_Y + 30, F_LARGE, C_AMBER);
  dirty(0, BODY_Y, W, 130);
  update(now);
  typer.start(COUNTDOWN_FOOTER, strlen(COUNTDOWN_FOOTER),
              (W - (int)strlen(COUNTDOWN_FOOTER) * charW(F_BODY)) / 2, H - 30 - lineH(F_BODY),
              F_BODY, C_DIM, 35, now + 600);
  endAt = now + DURATION_MS;
}

bool tick(uint32_t now) {
  if (rtclock::second() != shownSecond) update(now);
  anim.tick(now);
  typer.tick(now);
  term::cursorTick(now);
  if ((int32_t)(now - endAt) >= 0) {
    term::cursorHide();
    return false;
  }
  return true;
}

bool available() {
  return rtclock::valid() &&
         (rtclock::secondsToTarget() <= SHOW_BEFORE_S || rtclock::secondsSinceTarget() <= SHOW_AFTER_S);
}

}  // namespace

const Scene SCENE_COUNTDOWN = {"countdown", enter, tick, nullptr, available};
