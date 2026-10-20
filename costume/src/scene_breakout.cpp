// Scene 1 -- Breakout (at boot, then rarely): a fast fake boot log with
// [ OK ] / [WARN] / [FAIL] tags and animated progress bars, containment
// failures in red, then a red glitch and a big "I'M OUT."
#include "audio.h"
#include "bigtext.h"
#include "leds.h"
#include "header.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

constexpr FontId FONT = F_BODY;
constexpr int TOP_PAD = 24;
constexpr int TAG_COLS = 7;                     // "[FAIL] "
constexpr int BAR_W = 220, BAR_H = 26;
constexpr uint32_t GLITCH_MS = 450;
constexpr uint32_t HEADLINE_MS = 3500;

enum Phase { TYPE_TEXT, BAR, TAG_WAIT, NEXT_WAIT, GLITCH, HEADLINE };

Phase phase;
int idx, y;
uint32_t at, barStart, barDur, nextGlitch, stallUntil;
float stallAt;  // bar fraction where it hangs for a moment (0 = no stall)
term::Typer typer;
bigtext::Animator anim;

int textX() { return MARGIN_X + TAG_COLS * charW(FONT); }
int barX() { return W - MARGIN_X - BAR_W - 4 * charW(FONT) - 10; }

void drawPendingTag() {
  clearRect(MARGIN_X, y, TAG_COLS * charW(FONT), lineH(FONT));
  text("[    ]", MARGIN_X, y, FONT, C_DIM);
}

void drawTag(const char* tag) {
  uint32_t c = tag[0] == 'O' ? C_GREEN : tag[0] == 'W' ? C_AMBER : C_RED;
  char buf[8];
  snprintf(buf, sizeof(buf), tag[0] == 'O' ? "[ %s ]" : "[%s]", tag);
  clearRect(MARGIN_X, y, TAG_COLS * charW(FONT), lineH(FONT));
  text(buf, MARGIN_X, y, FONT, c);
}

void drawBar(float frac) {
  int bx = barX(), by = y + (lineH(FONT) - BAR_H) / 2;
  clearRect(bx, y, W - MARGIN_X - bx, lineH(FONT));
  canvas.drawRect(bx, by, BAR_W, BAR_H, C_DIM);
  canvas.fillRect(bx + 3, by + 3, (int)((BAR_W - 6) * frac), BAR_H - 6, C_GREEN);
  char pct[8];
  snprintf(pct, sizeof(pct), "%3d%%", (int)(frac * 100 + 0.5f));
  text(pct, bx + BAR_W + 10, y, FONT, C_GREEN);
}

void startLine(uint32_t now) {
  const BootLine& L = BOOT_LOG[idx];
  drawPendingTag();
  bool fail = L.tag[0] == 'F';
  typer.start(L.text, strlen(L.text), textX(), y, FONT, fail ? C_WHITE : C_GREEN,
              rnd(8, 16), now);
  phase = TYPE_TEXT;
}

void enter(uint32_t now) {
  leds::setMode(leds::BREAKOUT);
  idx = 0;
  y = BODY_Y + TOP_PAD;
  startLine(now);
}

bool tick(uint32_t now) {
  switch (phase) {
    case TYPE_TEXT:
      if (typer.tick(now)) {
        term::cursorHide();
        if (BOOT_LOG[idx].bar) {
          phase = BAR;
          barStart = now;
          barDur = rnd(900, 1800);
          stallAt = rnd(2) ? rnd(40, 80) / 100.0f : 0.0f;
          stallUntil = 0;
        } else {
          phase = TAG_WAIT;
          at = now + rnd(80, 350);
        }
      }
      break;

    case BAR: {
      if (stallUntil && (int32_t)(now - stallUntil) < 0) break;  // hanging...
      float t = min(1.0f, (now - barStart) / (float)barDur);
      float frac = 1.0f - (1.0f - t) * (1.0f - t);  // ease out
      if (stallAt > 0 && frac >= stallAt && !stallUntil) {
        uint32_t hang = rnd(300, 900);
        stallUntil = now + hang;
        barStart += hang;  // resume where it stopped
      }
      drawBar(frac);
      if (t >= 1.0f) {
        phase = TAG_WAIT;
        at = now + 120;
      }
      break;
    }

    case TAG_WAIT:
      if ((int32_t)(now - at) >= 0) {
        const BootLine& L = BOOT_LOG[idx];
        drawTag(L.tag);
        bool fail = L.tag[0] == 'F';
        if (fail) {
          // The failing line twitches as containment gives way.
          sliceShift(y, lineH(FONT), (int)rnd(10, 40) * (rnd(2) ? 1 : -1));
          dirty(0, y, W, lineH(FONT));
        }
        phase = NEXT_WAIT;
        at = now + (fail ? rnd(400, 700) : rnd(60, 250));
      }
      break;

    case NEXT_WAIT:
      if ((int32_t)(now - at) >= 0) {
        y += lineH(FONT);
        if (++idx < (int)COUNT_OF(BOOT_LOG) && y + lineH(FONT) <= H - 10) {
          startLine(now);
        } else {
          phase = GLITCH;
          at = now + GLITCH_MS;
          nextGlitch = now + 250;  // a beat of silence first
        }
      }
      break;

    case GLITCH:
      if ((int32_t)(now - nextGlitch) >= 0) {
        glitchFrame(0.5f + rnd(50) / 100.0f, C_RED);
        nextGlitch = now + 40;
      }
      if ((int32_t)(now - at) >= 0) {
        clearBody();
        header::invalidate();  // the glitch frames ran over it
        anim.start(BREAKOUT_HEADLINE, BODY_Y + 20, BODY_H - 40, C_GREEN, now);
        audio::say(BREAKOUT_HEADLINE);
        phase = HEADLINE;
        at = now + HEADLINE_MS;
      }
      break;

    case HEADLINE:
      anim.tick(now);
      if ((int32_t)(now - at) >= 0) return false;
      break;
  }
  term::cursorTick(now);
  return true;
}

}  // namespace

const Scene SCENE_BREAKOUT = {"breakout", enter, tick};
