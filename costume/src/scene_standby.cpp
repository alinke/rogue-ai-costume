// Show paused from the wearer menu: a dim "SYSTEM IDLE" at low backlight.
// Touches are ignored; the menu corner still works.
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

constexpr char TEXT[] = "SYSTEM IDLE";

void enter(uint32_t now) {
  setBacklightPercent(15);
  int x = (W - (int)strlen(TEXT) * charW(F_BODY)) / 2, y = BODY_Y + BODY_H / 2 - lineH(F_BODY);
  text(TEXT, x, y, F_BODY, C_DIM);
  term::cursorPlace(x + strlen(TEXT) * charW(F_BODY) + charW(F_BODY), y, F_BODY, C_DIM, now);
}

bool tick(uint32_t now) {
  term::cursorTick(now);
  return scenes::paused();
}

}  // namespace

const Scene SCENE_STANDBY = {"standby", enter, tick, nullptr, nullptr};
