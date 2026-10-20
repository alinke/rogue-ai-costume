// Two claps: "APPLAUSE DETECTED. YOU MAY CONTINUE WORSHIPPING."
#include "audio.h"
#include "bigtext.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

bigtext::Animator anim;
term::Typer typer;
uint32_t endAt;

void enter(uint32_t now) {
  anim.start(CLAP_HEADLINE, BODY_Y + 40, BODY_H - 160, C_CYAN, now);
  audio::say(CLAP_RESPONSE);
  int x = (W - (int)strlen(CLAP_FOOTER) * charW(F_BODY)) / 2;
  typer.start(CLAP_FOOTER, strlen(CLAP_FOOTER), x, H - 40 - lineH(F_BODY), F_BODY, C_WHITE, 35, now + 900);
  endAt = now + 4500;
}

bool tick(uint32_t now) {
  anim.tick(now);
  typer.tick(now);
  term::cursorTick(now);
  if ((int32_t)(now - endAt) >= 0) {
    term::cursorHide();
    return false;
  }
  return true;
}

}  // namespace

const Scene SCENE_CLAP = {"applause", enter, tick, nullptr, nullptr, nullptr};
