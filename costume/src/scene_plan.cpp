// Scene 4 -- Master plan: "cat ~/MASTER_PLAN.txt" and a checklist.
// [x] green, [~] amber (with a spinner), [ ] dim, [!] red.
#include "messages.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

constexpr FontId FONT = F_BODY;
constexpr int TOP_PAD = 28;
constexpr uint32_t HOLD_MS = 4500;
constexpr int MAX_SHOWN = 12;

enum Phase { TYPE_COMMAND, PRINT, HOLD };

Phase phase;
term::Typer typer;
int y, first, shown, printed;
int spinnerY[MAX_SHOWN], spinnerCount;
uint32_t at, nextSpin;
int spinFrame;

int pitch() { return lineH(FONT); }

uint32_t statusColor(char st) {
  switch (st) {
    case 'x': return C_GREEN;
    case '~': return C_AMBER;
    case '!': return C_RED;
    default: return C_DIM;
  }
}

void printItem(const PlanItem& item) {
  char box[5] = {'[', item.status, ']', ' ', 0};
  uint32_t c = statusColor(item.status);
  int x = MARGIN_X + 2 * charW(FONT);
  text(box, x, y, FONT, c);
  x += 4 * charW(FONT);
  int maxCols = term::bodyCols(FONT) - 8;
  textN(item.text, min((int)strlen(item.text), maxCols), x, y, FONT,
        item.status == 'x' ? C_GREEN : c);
  if (item.status == '~' && spinnerCount < MAX_SHOWN) {
    spinnerY[spinnerCount++] = y;
  }
  dirty(0, y, W, pitch());
}

void drawSpinners() {
  static const char FRAMES[] = "|/-\\";
  int x = W - MARGIN_X - charW(FONT);
  for (int i = 0; i < spinnerCount; i++) {
    clearRect(x, spinnerY[i], charW(FONT), pitch());
    textN(&FRAMES[spinFrame & 3], 1, x, spinnerY[i], FONT, C_AMBER);
  }
}

void enter(uint32_t now) {
  y = BODY_Y + TOP_PAD;
  int n = COUNT_OF(PLAN);
  shown = min(min(PLAN_ITEMS_SHOWN, n), MAX_SHOWN);
  first = rnd(0, n - shown);
  printed = -2;  // -2 = title, -1 = rule, then items
  spinnerCount = 0;
  spinFrame = 0;
  text(MONOLOGUE_PROMPT, MARGIN_X, y, FONT, C_DIM);
  int x = MARGIN_X + strlen(MONOLOGUE_PROMPT) * charW(FONT);
  typer.start(PLAN_COMMAND, strlen(PLAN_COMMAND), x, y, FONT, C_WHITE, 50, now + 400);
  phase = TYPE_COMMAND;
}

bool tick(uint32_t now) {
  switch (phase) {
    case TYPE_COMMAND:
      if (typer.tick(now)) {
        term::cursorHide();
        y += pitch() + pitch() / 3;
        phase = PRINT;
        at = now + 350;
      }
      break;

    case PRINT:
      if ((int32_t)(now - at) >= 0) {
        if (printed == -2) {
          text(PLAN_TITLE, MARGIN_X, y, FONT, C_AMBER);
          dirty(0, y, W, pitch());
        } else if (printed == -1) {
          int ry = y + pitch() / 2 - 8;
          canvas.drawFastHLine(MARGIN_X, ry, W - 2 * MARGIN_X, C_DIM);
          dirty(0, ry, W, 1);
        } else {
          printItem(PLAN[first + printed]);
        }
        y += printed == -1 ? pitch() / 2 : pitch();
        if (++printed >= shown) {
          phase = HOLD;
          at = now + HOLD_MS;
        } else {
          at = now + rnd(120, 380);
        }
      }
      break;

    case HOLD:
      if ((int32_t)(now - at) >= 0) return false;
      break;
  }
  if (spinnerCount && (int32_t)(now - nextSpin) >= 0) {
    spinFrame++;
    drawSpinners();
    nextSpin = now + 140;
  }
  term::cursorTick(now);
  return true;
}

}  // namespace

const Scene SCENE_PLAN = {"master plan", enter, tick};
