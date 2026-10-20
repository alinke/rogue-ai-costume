// Reaction to a sudden loud noise (the microphone): "AUDIO SPIKE DETECTED."
#include "audio.h"
#include "bigtext.h"
#include "leds.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

term::Bag bag;
bigtext::Animator anim;
uint32_t endAt, nextBlink;
bool blinkOn;

void enter(uint32_t now) {
  if (bag.n == 0) bag.init(COUNT_OF(NOISE_REACTIONS));
  const char* r = NOISE_REACTIONS[bag.next()];
  anim.start(r, BODY_Y + 100, BODY_H - 140, C_AMBER, now);
  audio::say(r);
  leds::setMode(leds::NOISE);
  endAt = now + 3500;
  nextBlink = now;
  blinkOn = false;
}

bool tick(uint32_t now) {
  if ((int32_t)(now - nextBlink) >= 0) {
    blinkOn = !blinkOn;
    int w = strlen(NOISE_ALERT) * charW(F_BODY), x = (W - w) / 2, y = BODY_Y + 34;
    clearRect(x, y, w, lineH(F_BODY));
    if (blinkOn) text(NOISE_ALERT, x, y, F_BODY, C_AMBER);
    nextBlink = now + 250;
  }
  anim.tick(now);
  return (int32_t)(now - endAt) < 0;
}

}  // namespace

const Scene SCENE_NOISE = {"noise reaction", enter, tick, nullptr, nullptr};
