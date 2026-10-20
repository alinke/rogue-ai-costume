// Photo booth: the camera live as a green mirror on the left, "STEP INTO THE
// RED FRAME, OR TOUCH THE BUTTON" and a big pulsing button on the right.
// Someone stepping in front (the middle of the picture changes a lot against
// how the empty view looked) or a touch anywhere starts a 3-2-1 countdown over
// the live preview -- time to get into the frame -- and at 0 the Human Scan
// flashes and takes the photo straight away. Nobody for BOOTH_TIMEOUT_S: the
// show moves on.
#include "audio.h"
#include "camera.h"
#include "config.h"
#include "leds.h"
#include "messages.h"
#include "scenes.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int VW = SCAN_PHOTO_W, VH = SCAN_PHOTO_H;
constexpr int VX = MARGIN_X + 12, VY = BODY_Y + 100;
constexpr int RX = VX + VW + 50;  // right column
constexpr int FRAME = 12;
constexpr int BTN_Y = BODY_Y + 400, BTN_H = 250;

// Presence: the middle of the picture as a grid of blocks, each block's mean
// brightness compared with the empty view's.
constexpr int GX = 8, GY = 8;               // blocks
constexpr int AX = VW / 5, AY = VH / 6;     // the middle area: x from 1/5 to 4/5, y from 1/6 to 5/6
constexpr int AW = VW * 3 / 5, AH = VH * 4 / 6;
constexpr uint32_t SETTLE_MS = 1200;        // exposure settles before the empty view is recorded
constexpr int NEEDED = GX * GY / 4;         // a quarter of the blocks changed...
constexpr int FRAMES_IN_A_ROW = 3;          // ...in this many frames running

uint32_t enteredAt, endAt, lastFrame, nextPulse;
uint32_t countAt;  // countdown started (0 = not counting)
int countShown;    // the number on screen (3, 2, 1)
bool triggered, haveBaseline, pulseOn;
int streak;
uint8_t baseline[GX * GY];

const ui::Button BUTTON = {RX, BTN_Y, W - MARGIN_X - RX, BTN_H, BOOTH_BUTTON};

void blockMeans(const uint8_t* px, uint8_t* out) {
  for (int by = 0; by < GY; by++) {
    for (int bx = 0; bx < GX; bx++) {
      uint32_t sum = 0, n = 0;
      int x0 = AX + bx * AW / GX, y0 = AY + by * AH / GY;
      for (int y = y0; y < y0 + AH / GY; y += 4) {
        for (int x = x0; x < x0 + AW / GX; x += 4) {
          sum += px[y * VW + x];
          n++;
        }
      }
      out[by * GX + bx] = n ? sum / n : 0;
    }
  }
}

void startCountdown(const char* why, uint32_t now) {
  Serial.printf("[I][booth] %s: countdown\n", why);
  countAt = now;
  countShown = 0;
  endAt = now + 3000 + 600;  // safety: the countdown ends the scene itself
  // The button becomes the instruction.
  canvas.fillRect(BUTTON.x, BUTTON.y, BUTTON.w, BUTTON.h, C_BG);
  ui::centredText(BOOTH_HOLD, BUTTON.x, BUTTON.y, BUTTON.w, BUTTON.h, F_BODY, C_AMBER);
  dirty(BUTTON.x, BUTTON.y, BUTTON.w, BUTTON.h);
}

void enter(uint32_t now) {
  triggered = haveBaseline = false;
  streak = 0;
  countAt = 0;
  countShown = 0;
  lastFrame = 0;
  enteredAt = now;
  endAt = now + BOOTH_TIMEOUT_S * 1000;
  camera::startLive();
  leds::setMode(leds::SCAN);
  canvas.fillRect(VX - FRAME, VY - FRAME, VW + 2 * FRAME, VH + 2 * FRAME, C_WHITE);
  canvas.fillRect(VX, VY, VW, VH, C_BG);
  dirty(VX - FRAME, VY - FRAME, VW + 2 * FRAME, VH + 2 * FRAME);
  text(BOOTH_TITLE, RX, BODY_Y + 30, F_LARGE, C_GREEN);
  int y = BODY_Y + 50 + lineH(F_LARGE);
  for (size_t i = 0; i < COUNT_OF(BOOTH_LINES); i++) {
    text(BOOTH_LINES[i], RX, y, F_BODY, i == 0 ? C_DIM : C_WHITE);
    y += lineH(F_BODY) + 6;
  }
#if PHOTO_UPLOAD_ENABLED
  text(BOOTH_NOTE, RX, BTN_Y + BTN_H + 20, F_SMALL, C_AMBER);
#endif
  dirty(RX, BODY_Y, W - RX, BODY_H);
  pulseOn = true;
  ui::drawButton(BUTTON, F_LARGE, C_RED, pulseOn);
  nextPulse = now + 500;
  audio::say(BOOTH_SAY);
}

bool tick(uint32_t now) {
  if ((int32_t)(now - endAt) >= 0) {
    triggered = countAt != 0;
    camera::stopLive();
    return false;
  }
  if (countAt) {
    int n = 3 - (int)((now - countAt) / 1000);
    if (n <= 0) {  // smile: the scan flashes and shoots now
      triggered = true;
      camera::stopLive();
      return false;
    }
    if (n != countShown) {
      countShown = n;
      audio::say(BOOTH_COUNT[3 - n]);
      audio::fx(audio::FX_KEY);
    }
  } else if ((int32_t)(now - nextPulse) >= 0) {  // the button breathes
    pulseOn = !pulseOn;
    ui::drawButton(BUTTON, F_LARGE, C_RED, pulseOn);
    nextPulse = now + 500;
  }
  const uint8_t* px;
  uint32_t f = camera::liveFrame(&px);
  if (!f || f == lastFrame) return true;
  lastFrame = f;
  ui::grayImage(px, VW, VX, VY, VW, 0, VH, ui::GREEN);
  // A frame showing where to stand.
  for (int t = 0; t < 3; t++) canvas.drawRect(VX + AX + t, VY + AY + t, AW - 2 * t, AH - 2 * t, C_RED);
  if (countAt) {
    // The count, big, over the preview.
    char d[2] = {(char)('0' + countShown), 0};
    ui::centredText(d, VX, VY, VW, VH, F_HUGE, C_WHITE);
    return true;
  }

  uint8_t now8[GX * GY];
  blockMeans(px, now8);
  if (!haveBaseline) {
    if (now - enteredAt >= SETTLE_MS) {
      memcpy(baseline, now8, sizeof(baseline));
      haveBaseline = true;
    }
    return true;
  }
  int changed = 0;
  for (int i = 0; i < GX * GY; i++) changed += abs(now8[i] - baseline[i]) > BOOTH_SENSITIVITY;
  streak = changed >= NEEDED ? streak + 1 : 0;
  if (streak >= FRAMES_IN_A_ROW) {
    audio::say(BOOTH_DETECTED);
    startCountdown("someone stepped in", now);
  }
  return true;
}

void touch(int, int, uint32_t now) {
  // Anywhere counts (the button is just the invitation).
  if (!countAt) {
    audio::fx(audio::FX_OK);
    startCountdown("touched", now);
  }
}

}  // namespace

const Scene SCENE_BOOTH = {"photo booth", enter, tick, touch, nullptr, nullptr};

bool boothTriggered() { return triggered; }
