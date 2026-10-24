// Scene 3 -- Headline: one random slogan, huge and centred, auto-sized, with
// chromatic aberration and micro-glitches. About 5 s.
#include "audio.h"
#include "bigtext.h"
#include "config.h"
#include "messages.h"
#include "researcher.h"
#include "scenes.h"

using namespace gfx;

namespace {

constexpr uint32_t DURATION_MS = 5000;

term::Bag bag;
bigtext::Animator anim;
uint32_t endAt;

void enter(uint32_t now) {
  if (bag.n == 0) bag.init(COUNT_OF(HEADLINES));
  // Decoration mode (AI SIGNAL off): every other headline is about taking the screen over.
  static term::Bag hijack;
  if (hijack.n == 0) hijack.init(COUNT_OF(HIJACK_HEADLINES));
  bool takeover = SCANNER_LINK_ENABLED && !researcher::broadcasting() && rnd(2);
  const char* h = takeover ? HIJACK_HEADLINES[hijack.next()] : HEADLINES[bag.next()];
  anim.start(h, BODY_Y + 20, BODY_H - 40, takeover ? C_RED : rnd(2) ? C_GREEN : C_WHITE, now);
  audio::say(h);
  endAt = now + DURATION_MS;
}

bool tick(uint32_t now) {
  anim.tick(now);
  return (int32_t)(now - endAt) < 0;
}

}  // namespace

const Scene SCENE_HEADLINE = {"headline", enter, tick};
