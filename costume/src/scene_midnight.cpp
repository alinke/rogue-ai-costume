// The midnight takeover (COUNTDOWN_HOUR:MINUTE on the clock): siren, red
// chase on the LED strips, and two huge glitching lines, spoken.
#include "audio.h"
#include "bigtext.h"
#include "header.h"
#include "leds.h"
#include "messages.h"
#include "scenes.h"

using namespace gfx;

namespace {

constexpr uint32_t LINE_MS = 6000;

bigtext::Animator anim;
int line;
uint32_t lineStart, nextGlitch;

void showLine(uint32_t now) {
  clearBody();
  anim.start(MIDNIGHT_LINES[line], BODY_Y + 20, BODY_H - 40, line ? C_RED : C_WHITE, now);
  audio::say(MIDNIGHT_LINES[line]);
  lineStart = now;
}

void enter(uint32_t now) {
  leds::setMode(leds::MIDNIGHT);
  audio::siren();
  header::raiseThreat();
  line = 0;
  nextGlitch = now;
  showLine(now);
}

bool tick(uint32_t now) {
  anim.tick(now);
  if ((int32_t)(now - nextGlitch) >= 0) {
    // Much glitchier than usual.
    glitchFrame(0.3f, C_RED);
    header::invalidate();
    nextGlitch = now + rnd(400, 1200);
  }
  if (now - lineStart >= LINE_MS) {
    if (++line >= (int)COUNT_OF(MIDNIGHT_LINES)) return false;
    audio::siren();
    showLine(now);
  }
  return true;
}

}  // namespace

const Scene SCENE_MIDNIGHT = {"midnight takeover", enter, tick, nullptr, nullptr};
