// "I SEE YOU.": the camera live, as green scan-line video in a white frame
// (which doubles as a light on the guest's face), a reticle on top, and
// readouts typed out beside it. About 9 s.
#include "audio.h"
#include "camera.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int VW = SCAN_PHOTO_W, VH = SCAN_PHOTO_H;
constexpr int VX = MARGIN_X + 12, VY = BODY_Y + 100;
constexpr int RX = VX + VW + 50;  // readout column
constexpr int FRAME = 12;         // white border, px
constexpr uint32_t DURATION_MS = 9000;

uint32_t endAt, lastFrame, nextLine;
int line;
term::Typer typer;
char lines[5][48];

void reticle() {
  constexpr int L = 50;
  for (int t = 0; t < 3; t++) {
    canvas.drawFastHLine(VX + 20, VY + 20 + t, L, C_RED);
    canvas.drawFastVLine(VX + 20 + t, VY + 20, L, C_RED);
    canvas.drawFastHLine(VX + VW - 20 - L, VY + 20 + t, L, C_RED);
    canvas.drawFastVLine(VX + VW - 21 - t, VY + 20, L, C_RED);
    canvas.drawFastHLine(VX + 20, VY + VH - 21 - t, L, C_RED);
    canvas.drawFastVLine(VX + 20 + t, VY + VH - 20 - L, L, C_RED);
    canvas.drawFastHLine(VX + VW - 20 - L, VY + VH - 21 - t, L, C_RED);
    canvas.drawFastVLine(VX + VW - 21 - t, VY + VH - 20 - L, L, C_RED);
  }
  int cx = VX + VW / 2, cy = VY + VH * 2 / 5;
  canvas.drawCircle(cx, cy, 120, C_RED);
  canvas.drawCircle(cx, cy, 121, C_RED);
  canvas.drawFastHLine(cx - 150, cy, 50, C_RED);
  canvas.drawFastHLine(cx + 100, cy, 50, C_RED);
}

void enter(uint32_t now) {
  camera::startLive();
  canvas.fillRect(VX - FRAME, VY - FRAME, VW + 2 * FRAME, VH + 2 * FRAME, C_WHITE);
  canvas.fillRect(VX, VY, VW, VH, C_BG);
  dirty(VX - FRAME, VY - FRAME, VW + 2 * FRAME, VH + 2 * FRAME);
  text(MIRROR_TITLE, RX, BODY_Y + 40, F_LARGE, C_RED);
  dirty(RX, BODY_Y + 40, W - RX, lineH(F_LARGE));
  audio::say(MIRROR_TITLE);
  for (int i = 0; i < 5; i++) {
    snprintf(lines[i], sizeof(lines[i]), MIRROR_READOUTS[i % COUNT_OF(MIRROR_READOUTS)],
             (int)rnd(i == 2 ? 3 : 41, i == 2 ? 10 : 99));
  }
  line = 0;
  nextLine = now + 1200;
  lastFrame = 0;
  endAt = now + DURATION_MS;
}

bool tick(uint32_t now) {
  const uint8_t* px;
  uint32_t f = camera::liveFrame(&px);
  if (f && f != lastFrame) {
    lastFrame = f;
    ui::grayImage(px, VW, VX, VY, VW, 0, VH, ui::GREEN);
    reticle();
  }
  if (line < 5 && (int32_t)(now - nextLine) >= 0 && typer.done()) {
    int y = BODY_Y + 60 + lineH(F_LARGE) + line * (lineH(F_BODY) + 14);
    typer.start(lines[line], strlen(lines[line]), RX, y, F_BODY, line == 4 ? C_AMBER : C_GREEN, 22, now);
    line++;
    nextLine = now + 900;
  }
  typer.tick(now);
  if ((int32_t)(now - endAt) >= 0) {
    camera::stopLive();
    term::cursorHide();
    return false;
  }
  return true;
}

bool available() { return camera::available(); }

}  // namespace

const Scene SCENE_MIRROR = {"live mirror", enter, tick, nullptr, available, nullptr};
