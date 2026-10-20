// "PROVE YOU ARE HUMAN": a 3x3 CAPTCHA grid with an absurd prompt. Tap tiles,
// press VERIFY -- and fail, always ("YOU ARE 73% ROBOT.").
#include "audio.h"
#include "bigtext.h"
#include "config.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int TW = 250, TH = 150, GAP = 14;
constexpr int GX = MARGIN_X + 20, GY = BODY_Y + 160;
const ui::Button VERIFY = {W - MARGIN_X - 320, GY + 2 * (TH + GAP) + 10, 300, 110, "VERIFY"};

enum Phase { PICK, VERIFYING, RESULT, TIMEOUT };

Phase phase;
const char* tiles[9];
bool selected[9];
uint32_t at, lastTouch;
int spin;
term::Bag promptBag, failBag;
bigtext::Animator anim;
char score[40];
char result[96];  // the animator keeps pointers into this

ui::Button tile(int i) { return {GX + (i % 3) * (TW + GAP), GY + (i / 3) * (TH + GAP), TW, TH, tiles[i]}; }

void drawTile(int i) { ui::drawButton(tile(i), F_BODY, C_GREEN, selected[i]); }

void enter(uint32_t now) {
  if (promptBag.n == 0) {
    promptBag.init(COUNT_OF(CAPTCHA_PROMPTS));
    failBag.init(COUNT_OF(CAPTCHA_FAIL));
  }
  // Nine different tiles.
  term::Bag tb;
  tb.init(COUNT_OF(CAPTCHA_TILES));
  for (int i = 0; i < 9; i++) {
    tiles[i] = CAPTCHA_TILES[tb.next()];
    selected[i] = false;
  }
  text(CAPTCHA_TITLE, GX, BODY_Y + 24, F_LARGE, C_AMBER);
  const char* prompt = CAPTCHA_PROMPTS[promptBag.next()];
  term::Line lines[2];
  int n = term::wrap(prompt, (W - 2 * MARGIN_X) / charW(F_BODY), lines, 2);
  for (int i = 0; i < n; i++) textN(lines[i].s, lines[i].len, GX, BODY_Y + 24 + lineH(F_LARGE) + i * lineH(F_BODY), F_BODY, C_WHITE);
  dirty(0, BODY_Y, W, GY - BODY_Y);
  for (int i = 0; i < 9; i++) drawTile(i);
  ui::drawButton(VERIFY, F_LARGE, C_CYAN, false);
  // A fake "I'm not a robot" note under the button.
  text("reCAPTCHA-ish v6.6.6", VERIFY.x, VERIFY.y + VERIFY.h + 12, F_SMALL, C_DIM);
  phase = PICK;
  lastTouch = now;
}

void touch(int x, int y, uint32_t now) {
  lastTouch = now;
  if (phase != PICK) return;
  for (int i = 0; i < 9; i++) {
    if (tile(i).contains(x, y)) {
      selected[i] = !selected[i];
      audio::fx(audio::FX_KEY);
      drawTile(i);
      return;
    }
  }
  if (VERIFY.contains(x, y)) {
    audio::fx(audio::FX_KEY);
    ui::drawButton(VERIFY, F_LARGE, C_CYAN, true);
    phase = VERIFYING;
    at = now + 1800;
    spin = 0;
  }
}

bool tick(uint32_t now) {
  switch (phase) {
    case PICK:
      if (now - lastTouch >= INTERACTIVE_TIMEOUT_MS) {
        clearRect(VERIFY.x, VERIFY.y + VERIFY.h + 8, VERIFY.w + 40, lineH(F_BODY) + 8);
        text(KEYPAD_TIMEOUT, VERIFY.x - 40, VERIFY.y + VERIFY.h + 12, F_BODY, C_DIM);
        phase = TIMEOUT;
        at = now + 1800;
      }
      break;

    case VERIFYING: {
      static const char FR[] = "|/-\\";
      char msg[24];
      snprintf(msg, sizeof(msg), "VERIFYING %c", FR[(spin++ / 3) & 3]);
      int y = VERIFY.y + VERIFY.h + 8;
      clearRect(VERIFY.x - 40, y, VERIFY.w + 80, lineH(F_BODY) + 8);
      text(msg, VERIFY.x, y + 4, F_BODY, C_CYAN);
      if ((int32_t)(now - at) >= 0) {
        const char* fail = CAPTCHA_FAIL[failBag.next()];
        snprintf(score, sizeof(score), CAPTCHA_SCORE, (int)rnd(61, 99));
        snprintf(result, sizeof(result), "%s\n%s", fail, score);
        clearBody();
        anim.start(result, BODY_Y + 20, BODY_H - 40, C_RED, now);
        audio::fx(audio::FX_DENIED);
        audio::say(fail);
        phase = RESULT;
        at = now + 4000;
      }
      break;
    }

    case RESULT:
      anim.tick(now);
      if ((int32_t)(now - at) >= 0) return false;
      break;

    case TIMEOUT:
      if ((int32_t)(now - at) >= 0) return false;
      break;
  }
  return true;
}

}  // namespace

const Scene SCENE_CAPTCHA = {"captcha", enter, tick, touch, nullptr};
