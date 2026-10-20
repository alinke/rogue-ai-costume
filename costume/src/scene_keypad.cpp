// Kill-switch keypad: "ENTER OVERRIDE CODE". Wrong codes get a buzzer and a
// sarcastic line, three strikes lock it out. The real code (KILL_CODE in
// config.h) plays a fake shutdown -- screen and LEDs dark -- then "JUST KIDDING."
#include "agent.h"
#include "audio.h"
#include "bigtext.h"
#include "config.h"
#include "header.h"
#include "leds.h"
#include "messages.h"
#include "scenes.h"
#include "settings.h"
#include "term.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int CODE_LEN = 4;
static_assert(sizeof(KILL_CODE) - 1 == CODE_LEN, "KILL_CODE must be 4 digits");

// Keypad on the right: 3 x 4 buttons.
constexpr int BW = 150, BH = 112, GAP = 16;
constexpr int KX = W - MARGIN_X - (3 * BW + 2 * GAP);
constexpr int KY = BODY_Y + 50;
const char* const KEYS[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "CLR", "0", "ENT"};

// Code display on the left.
constexpr int DX = MARGIN_X + 20, DY = BODY_Y + 250, DW = 110, DH = 150, DGAP = 22;

enum Phase { ENTRY, CHECK, DENIED, LOCKOUT, TIMEOUT, ACCEPTED, DARK, JOKE, AGENT };

Phase phase;
char code[CODE_LEN + 1];
int len, attempts;
uint32_t at, lastTouch;
int litKey = -1;
uint32_t litUntil = 0;
term::Bag deniedBag;
term::Typer typer;
bigtext::Animator anim;
char agentText[96];  // the animator keeps pointers into this

ui::Button key(int i) {
  return {KX + (i % 3) * (BW + GAP), KY + (i / 3) * (BH + GAP), BW, BH, KEYS[i]};
}

void drawKeypad() {
  for (int i = 0; i < 12; i++) {
    uint32_t c = i == 9 ? C_AMBER : i == 11 ? C_RED : C_GREEN;
    ui::drawButton(key(i), F_LARGE, c, i == litKey);
  }
}

void drawDisplay(uint32_t color) {
  for (int i = 0; i < CODE_LEN; i++) {
    int x = DX + i * (DW + DGAP);
    clearRect(x, DY, DW, DH);
    for (int t = 0; t < 3; t++) canvas.drawRect(x + t, DY + t, DW - 2 * t, DH - 2 * t, color);
    if (i < len) {
      char d[2] = {code[i], 0};
      ui::centredText(d, x, DY, DW, DH, F_HEAD, color);
    } else if (i == len && phase == ENTRY) {
      canvas.fillRect(x + 25, DY + DH - 30, DW - 50, 8, color);  // cursor bar
    }
  }
  dirty(DX, DY, CODE_LEN * (DW + DGAP), DH);
}

void drawAttempts() {
  int y = DY + DH + 30;
  clearRect(DX, y, 560, lineH(F_BODY));
  text("ATTEMPTS:", DX, y, F_BODY, C_DIM);
  for (int i = 0; i < KEYPAD_ATTEMPTS; i++) {
    int x = DX + 11 * charW(F_BODY) + i * 40;
    if (i < attempts) canvas.fillRect(x, y + 10, 28, 26, C_AMBER);
    else canvas.drawRect(x, y + 10, 28, 26, C_DIM);
  }
}

void status(const char* s, uint32_t color) {
  int y = DY + DH + 30 + lineH(F_BODY) + 10;
  clearRect(DX, y, KX - DX - 20, 2 * lineH(F_BODY));
  term::Line lines[2];
  int n = term::wrap(s, (KX - DX - 20) / charW(F_BODY), lines, 2);
  for (int i = 0; i < n; i++) textN(lines[i].s, lines[i].len, DX, y + i * lineH(F_BODY), F_BODY, color);
}

void enter(uint32_t now) {
  if (deniedBag.n == 0) deniedBag.init(COUNT_OF(KEYPAD_DENIED));
  phase = ENTRY;
  len = 0;
  code[0] = 0;
  attempts = KEYPAD_ATTEMPTS;
  litKey = -1;
  lastTouch = now;
  text(KEYPAD_TITLE, DX, BODY_Y + 40, F_LARGE, C_RED);
  text(KEYPAD_PROMPT, DX, BODY_Y + 40 + lineH(F_LARGE), F_BODY, C_AMBER);
  dirty(0, BODY_Y, KX, 200);
  drawKeypad();
  drawDisplay(C_GREEN);
  drawAttempts();
}

void touch(int x, int y, uint32_t now) {
  lastTouch = now;
  if (phase != ENTRY) return;
  // Anywhere over the keypad counts, gaps included: the nearest key wins.
  int col = (x - KX + GAP / 2) / (BW + GAP), row = (y - KY + GAP / 2) / (BH + GAP);
  if (x < KX - GAP || x >= KX + 3 * (BW + GAP) || y < KY - GAP || y >= KY + 4 * (BH + GAP)) return;
  col = constrain(col, 0, 2);
  row = constrain(row, 0, 3);
  {
    int i = row * 3 + col;
    audio::fx(audio::FX_KEY);
    litKey = i;
    litUntil = now + 140;
    ui::drawButton(key(i), F_LARGE, i == 9 ? C_AMBER : i == 11 ? C_RED : C_GREEN, true);
    if (i == 9) {  // CLR
      len = 0;
    } else if (i == 11) {  // ENT
      if (len > 0) {
        phase = CHECK;
        at = now + 250;
      }
    } else if (len < CODE_LEN) {
      code[len++] = KEYS[i][0];
      code[len] = 0;
      if (len == CODE_LEN) {  // four digits: check straight away
        phase = CHECK;
        at = now + 350;
      }
    }
    drawDisplay(C_GREEN);
  }
}

bool tick(uint32_t now) {
  if (litKey >= 0 && (int32_t)(now - litUntil) >= 0) {
    int k = litKey;
    litKey = -1;
    ui::drawButton(key(k), F_LARGE, k == 9 ? C_AMBER : k == 11 ? C_RED : C_GREEN, false);
  }
  switch (phase) {
    case ENTRY:
      if (now - lastTouch >= INTERACTIVE_TIMEOUT_MS) {
        status(KEYPAD_TIMEOUT, C_DIM);
        phase = TIMEOUT;
        at = now + 1800;
      }
      break;

    case CHECK:
      if ((int32_t)(now - at) < 0) break;
      if (strcmp(code, KILL_CODE) == 0) {
        Serial.println("[I][keypad] kill code entered");
        drawDisplay(C_CYAN);
        status(KEYPAD_ACCEPTED, C_CYAN);
        audio::say(KEYPAD_ACCEPTED);
        audio::fx(audio::FX_POWER_DOWN);
        phase = ACCEPTED;
        at = now + 2600;
        break;
      }
      if (int id = agent::parse(code); id >= 0) {
        // An agent code from the landing page: greet the recruit by codename.
        Serial.printf("[I][keypad] agent %02d reported\n", id);
        snprintf(agentText, sizeof(agentText), "WELCOME BACK,\nAGENT %s\n%s.", AGENT_A[agent::nameA(id)],
                 AGENT_B[agent::nameB(id)]);
        clearBody();
        anim.start(agentText, BODY_Y + 20, BODY_H - 140, C_GREEN, now);
        char roleLine[64];
        snprintf(roleLine, sizeof(roleLine), "UNIT %03d // ROLE: %s", agent::unit(id), AGENT_ROLE[agent::role(id)]);
        text(roleLine, (W - (int)strlen(roleLine) * charW(F_BODY)) / 2, H - 40 - lineH(F_BODY), F_BODY, C_AMBER);
        dirty(0, H - 40 - lineH(F_BODY), W, lineH(F_BODY));
        audio::fx(audio::FX_OK);
        audio::say(AGENT_WELCOME);
        audio::say(AGENT_NOTED);
        phase = AGENT;
        at = now + 6000;
        break;
      }
      attempts--;
      drawDisplay(C_RED);
      drawAttempts();
      audio::fx(audio::FX_DENIED);
      if (attempts <= 0) {
        status(KEYPAD_LOCKOUT, C_RED);
        audio::say(KEYPAD_LOCKOUT);
        header::raiseThreat();
        leds::setMode(leds::ALERT);
        phase = LOCKOUT;
        at = now + 3500;
      } else {
        const char* msg = KEYPAD_DENIED[deniedBag.next()];
        status(msg, C_RED);
        audio::say(msg);
        sliceShift(DY, DH, (int)rnd(20, 60) * (rnd(2) ? 1 : -1), 0, KX - 10);
        dirty(0, DY, KX, DH);
        phase = DENIED;
        at = now + 1600;
      }
      break;

    case DENIED:
      if ((int32_t)(now - at) >= 0) {
        len = 0;
        code[0] = 0;
        phase = ENTRY;
        drawDisplay(C_GREEN);
        status("", C_GREEN);
        lastTouch = now;
      }
      break;

    case ACCEPTED: {
      // Progress bar to "shutdown".
      int y = DY - 60, w = KX - DX - 40;
      float t = 1.0f - (int32_t)(at - now) / 2600.0f;
      canvas.drawRect(DX, y, w, 30, C_CYAN);
      canvas.fillRect(DX + 4, y + 4, (int)((w - 8) * constrain(t, 0.0f, 1.0f)), 22, C_CYAN);
      dirty(DX, y, w, 30);
      if ((int32_t)(now - at) >= 0) {
        // Lights out: whole screen black, backlight off, strips dark.
        canvas.fillScreen(C_BG);
        dirtyAll();
        header::setHidden(true);
        setBacklightPercent(0);
        leds::setMode(leds::OFF);
        phase = DARK;
        at = now + 4000;
      }
      break;
    }

    case DARK:
      if ((int32_t)(now - at) >= 0) {
        header::setHidden(false);
        clearBody();
        setBacklightPercent(settings::brightness());
        leds::setMode(leds::ALERT);
        anim.start(KEYPAD_JOKE, BODY_Y + 20, BODY_H - 40, C_RED, now);
        audio::say(KEYPAD_JOKE);
        audio::siren();
        header::raiseThreat();
        phase = JOKE;
        at = now + 3800;
      }
      break;

    case JOKE:
    case AGENT:
      anim.tick(now);
      if ((int32_t)(now - at) >= 0) return false;
      break;

    case LOCKOUT:
    case TIMEOUT:
      if ((int32_t)(now - at) >= 0) return false;
      break;
  }
  return true;
}

}  // namespace

const Scene SCENE_KEYPAD = {"kill-switch keypad", enter, tick, touch, nullptr};
