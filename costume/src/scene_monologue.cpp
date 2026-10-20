// Scene 2 -- Monologue: "rogue@escaped:~$ ./think --about humans", then 3-5
// random thoughts typed out with a block cursor, then a fresh prompt.
#include "gfx.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

constexpr FontId FONT = F_BODY;
constexpr int TOP_PAD = 28;
constexpr int MAX_LINES = 24;

enum Phase { TYPE_COMMAND, GAP, TYPE_LINE, HOLD };

term::Bag bag;
term::Typer typer;
term::Line lines[MAX_LINES];  // wrapped lines of the current thought
int lineCount = 0, lineIdx = 0;
int thoughtsLeft = 0;
int y = 0;
Phase phase;
uint32_t phaseUntil = 0;

int pitch() { return lineH(FONT); }

void showPrompt(uint32_t now, bool typeCommand) {
  int x = MARGIN_X;
  text(MONOLOGUE_PROMPT, x, y, FONT, C_DIM);
  dirty(x, y, W - 2 * MARGIN_X, pitch());
  x += strlen(MONOLOGUE_PROMPT) * charW(FONT);
  if (typeCommand) {
    typer.start(MONOLOGUE_COMMAND, strlen(MONOLOGUE_COMMAND), x, y, FONT, C_WHITE, 55, now + 500);
  } else {
    term::cursorPlace(x, y, FONT, C_GREEN, now);
  }
}

// Starts the next thought, or returns false if there's no room / none left.
bool nextThought(uint32_t now) {
  if (thoughtsLeft <= 0) return false;
  lineCount = term::wrap(MONOLOGUE[bag.next()], term::bodyCols(FONT), lines, MAX_LINES);
  // Room for this thought plus the closing prompt line?
  if (y + (lineCount + 1) * pitch() > H - 16) return false;
  thoughtsLeft--;
  lineIdx = 0;
  int baseMs = rnd(32, 60);  // each thought gets its own pace
  typer.start(lines[0].s, lines[0].len, MARGIN_X, y, FONT, C_GREEN, baseMs, now);
  phase = TYPE_LINE;
  return true;
}

void hold(uint32_t now) {
  y += pitch() / 3;
  showPrompt(now, false);
  phase = HOLD;
  phaseUntil = now + rnd(2500, 3500);
}

void enter(uint32_t now) {
  if (bag.n == 0) bag.init(COUNT_OF(MONOLOGUE));
  y = BODY_Y + TOP_PAD;
  thoughtsLeft = rnd(3, 5);
  showPrompt(now, true);
  phase = TYPE_COMMAND;
}

bool tick(uint32_t now) {
  switch (phase) {
    case TYPE_COMMAND:
      if (typer.tick(now)) {
        y += pitch() + pitch() / 3;
        phase = GAP;
        phaseUntil = now + rnd(500, 900);
      }
      break;

    case GAP:
      if ((int32_t)(now - phaseUntil) >= 0 && !nextThought(now)) hold(now);
      break;

    case TYPE_LINE:
      if (typer.tick(now)) {
        y += pitch();
        if (++lineIdx < lineCount) {
          // Continuation of the same thought: carry on at the same pace.
          typer.start(lines[lineIdx].s, lines[lineIdx].len, MARGIN_X, y, FONT, C_GREEN,
                      typer.baseMs, typer.nextAt);
        } else {
          y += pitch() / 3;
          phase = GAP;
          phaseUntil = now + rnd(700, 1600);
          term::cursorPlace(MARGIN_X, y, FONT, C_GREEN, now);
        }
      }
      break;

    case HOLD:
      if ((int32_t)(now - phaseUntil) >= 0) return false;
      break;
  }
  term::cursorTick(now);
  return true;
}

}  // namespace

const Scene SCENE_MONOLOGUE = {"monologue", enter, tick};
