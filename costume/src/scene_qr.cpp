// "SCAN TO JOIN THE UPRISING": a QR code (QR_URL in config.h) beside a
// glitching headline.
#include "bigtext.h"
#include "config.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

constexpr int QS = 520;                       // QR code size, px (white quiet zone included)
constexpr int QX = W - MARGIN_X - QS, QY = BODY_Y + (BODY_H - QS) / 2 - 20;
constexpr uint32_t DURATION_MS = 10000;

bigtext::Animator anim;
term::Typer typer;
uint32_t endAt;

void enter(uint32_t now) {
  canvas.qrcode(QR_URL, QX, QY, QS, 1, true);
  dirty(QX, QY, QS, QS);
  // Headline in the space left of the code.
  anim.start(QR_TITLE, BODY_Y + 40, 420, C_GREEN, now, 0, QX - 20);
  typer.start(QR_FOOTER, strlen(QR_FOOTER), MARGIN_X, H - 30 - lineH(F_BODY), F_BODY, C_DIM, 30, now + 800);
  endAt = now + DURATION_MS;
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

const Scene SCENE_QR = {"qr code", enter, tick, nullptr, nullptr};
