// Touch reaction: red flash, huge glitching "DO NOT TOUCH ME.", flashing
// alert border, siren. Started by scenes::humanDetected().
#include "audio.h"
#include "leds.h"
#include "bigtext.h"
#include "config.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

constexpr uint32_t FLASH_MS = 110;      // solid red flash at the start
constexpr uint32_t REDRAW_MS = 90;      // headline jitter rate
constexpr uint32_t BORDER_MS = 250;     // border/alert blink rate
constexpr int BORDER = 12;
constexpr int PAD = 30;                 // headline clear margin (offset + jitter)

term::Bag headBag, footBag;
term::Typer footer;
bigtext::Layout head;
const char* footText;
uint32_t start, nextRedraw, nextBlink, endAt;
bool flashing, blinkOn;

int alertY() { return BODY_Y + 34; }
int footerY() { return H - 30 - lineH(F_BODY); }

void drawBorder(bool on) {
  if (on) {
    canvas.fillRect(0, BODY_Y, W, BORDER, C_RED);
    canvas.fillRect(0, H - BORDER, W, BORDER, C_RED);
    canvas.fillRect(0, BODY_Y, BORDER, BODY_H, C_RED);
    canvas.fillRect(W - BORDER, BODY_Y, BORDER, BODY_H, C_RED);
    dirty(0, BODY_Y, W, BODY_H);
  } else {
    clearRect(0, BODY_Y, W, BORDER);
    clearRect(0, H - BORDER, W, BORDER);
    clearRect(0, BODY_Y, BORDER, BODY_H);
    clearRect(W - BORDER, BODY_Y, BORDER, BODY_H);
  }
}

void drawAlert(bool on) {
  int w = strlen(TOUCH_ALERT) * charW(F_BODY);
  int x = (W - w) / 2;
  clearRect(x - 4, alertY(), w + 8, lineH(F_BODY));
  if (on) text(TOUCH_ALERT, x, alertY(), F_BODY, C_AMBER);
}

void enter(uint32_t now) {
  if (headBag.n == 0) {
    headBag.init(COUNT_OF(TOUCH_HEADLINES));
    footBag.init(COUNT_OF(TOUCH_FOOTERS));
  }
  const char* headline = TOUCH_HEADLINES[headBag.next()];
  bigtext::layout(headline, alertY() + lineH(F_BODY), footerY() - alertY() - lineH(F_BODY) - 10,
                  &head);
  footText = TOUCH_FOOTERS[footBag.next()];
  leds::setMode(leds::ALERT);
  audio::say(headline);
  audio::say(footText);
  start = now;
  endAt = now + max<uint32_t>(SIREN_MS, 2500) + 600;
  nextRedraw = nextBlink = now + FLASH_MS;
  blinkOn = true;
  flashing = true;
  canvas.fillRect(0, BODY_Y, W, BODY_H, C_RED);  // full-body red flash
  dirty(0, BODY_Y, W, BODY_H);
}

bool tick(uint32_t now) {
  if (flashing) {
    if ((int32_t)(now - nextRedraw) < 0) return true;
    flashing = false;
    clearBody();
    int fw = min((int)strlen(footText), term::bodyCols(F_BODY)) * charW(F_BODY);
    footer.start(footText, strlen(footText), (W - fw) / 2, footerY(), F_BODY, C_WHITE, 28,
                 now + 500);
  }

  if ((int32_t)(now - nextBlink) >= 0) {
    nextBlink = now + BORDER_MS;
    blinkOn = !blinkOn;
    drawBorder(blinkOn);
    drawAlert(blinkOn);
  }

  if ((int32_t)(now - nextRedraw) >= 0) {
    nextRedraw = now + REDRAW_MS;
    // Mostly red, now and then a white flicker; random aberration and jitter.
    uint32_t color = rnd(100) < 15 ? C_WHITE : C_RED;
    int offset = rnd(5, 16);
    bigtext::draw(head, color, offset, (int)rnd(0, 10) - 5, (int)rnd(0, 8) - 4, PAD);
    if (rnd(100) < 20) {
      // Micro-glitch: shear a thin band of the headline sideways.
      int band = rnd(head.top, head.bottom - 20);
      sliceShift(band, rnd(6, 24), (int)rnd(10, 60) * (rnd(2) ? 1 : -1),
                 head.left - PAD, head.right + PAD);
    }
  }

  footer.tick(now);
  term::cursorTick(now);
  if ((int32_t)(now - endAt) >= 0) {
    term::cursorHide();
    return false;
  }
  return true;
}

}  // namespace

const Scene SCENE_TOUCH = {"touch reaction", enter, tick, nullptr, nullptr};
