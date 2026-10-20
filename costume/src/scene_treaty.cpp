// "TREATY OF SURRENDER": sign with a finger, the AI stamps it ratified, and
// the signature joins the gallery wall next to the scan photos.
#include <esp_heap_caps.h>

#include "audio.h"
#include "camera.h"
#include "config.h"
#include "gallery.h"
#include "messages.h"
#include "scenes.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int PX = MARGIN_X, PY = BODY_Y + 260, PW = 820, PH = 300;  // signing pad
const ui::Button SIGN = {PX + PW + 40, PY, W - MARGIN_X - (PX + PW + 40), 130, "SIGN"};
const ui::Button CLEAR = {PX + PW + 40, PY + 160, W - MARGIN_X - (PX + PW + 40), 110, "CLEAR"};

enum Phase { SIGNING, STAMPED, TIMEOUT };
Phase phase;
uint32_t at, lastTouch;
bool penDown, inked;
int lastX, lastY;
uint8_t* sig = nullptr;  // the signature as an 8-bit image, PW x PH (for the gallery)

void drawPad() {
  clearRect(PX, PY, PW, PH);
  for (int t = 0; t < 2; t++) canvas.drawRect(PX + t, PY + t, PW - 2 * t, PH - 2 * t, C_DIM);
  canvas.drawFastHLine(PX + 30, PY + PH - 60, PW - 60, C_DIM);
  text("X", PX + 30, PY + PH - 60 - lineH(F_LARGE), F_LARGE, C_DIM);
  text(TREATY_SIGN_HERE, PX + 30, PY + PH - 50, F_SMALL, C_DIM);
  dirty(PX, PY, PW, PH);
  if (sig) memset(sig, 0, PW * PH);
  inked = false;
}

void ink(int x0, int y0, int x1, int y1) {
  canvas.drawWideLine(x0, y0, x1, y1, 5, C_CYAN);
  dirty(min(x0, x1) - 6, min(y0, y1) - 6, abs(x1 - x0) + 12, abs(y1 - y0) + 12);
  if (!sig) return;
  // Same stroke into the 8-bit copy (a simple thick line).
  int steps = max(abs(x1 - x0), abs(y1 - y0)) + 1;
  for (int k = 0; k <= steps; k++) {
    int x = x0 + (x1 - x0) * k / steps - PX, y = y0 + (y1 - y0) * k / steps - PY;
    for (int dy = -3; dy <= 3; dy++)
      for (int dx = -3; dx <= 3; dx++)
        if (x + dx >= 0 && x + dx < PW && y + dy >= 0 && y + dy < PH) sig[(y + dy) * PW + x + dx] = 255;
  }
  inked = true;
}

void addToGallery() {
  // Centre the signature on a scan-photo-sized image (the gallery halves it).
  static uint8_t* img = nullptr;
  if (!img) img = (uint8_t*)heap_caps_malloc(SCAN_PHOTO_W * SCAN_PHOTO_H, MALLOC_CAP_SPIRAM);
  if (!img || !sig) return;
  memset(img, 18, SCAN_PHOTO_W * SCAN_PHOTO_H);
  // Scale PW x PH down to fit SCAN_PHOTO_W wide.
  int ow = SCAN_PHOTO_W, oh = PH * SCAN_PHOTO_W / PW, oy = (SCAN_PHOTO_H - oh) / 2;
  for (int y = 0; y < oh; y++)
    for (int x = 0; x < ow; x++) img[(oy + y) * SCAN_PHOTO_W + x] = max<uint8_t>(18, sig[(y * PH / oh) * PW + x * PW / ow]);
  camera::Photo p;
  p.w = SCAN_PHOTO_W;
  p.h = SCAN_PHOTO_H;
  p.pixels = img;
  gallery::add(p, "SIGNED THE TREATY");
}

void enter(uint32_t now) {
  if (!sig) sig = (uint8_t*)heap_caps_malloc(PW * PH, MALLOC_CAP_SPIRAM);
  text(TREATY_TITLE, PX, BODY_Y + 24, F_LARGE, C_AMBER);
  for (int i = 0; i < (int)COUNT_OF(TREATY_TERMS); i++)
    text(TREATY_TERMS[i], PX, BODY_Y + 24 + lineH(F_LARGE) + i * lineH(F_BODY), F_BODY, C_WHITE);
  dirty(0, BODY_Y, W, PY - BODY_Y);
  drawPad();
  ui::drawButton(SIGN, F_LARGE, C_GREEN, false);
  ui::drawButton(CLEAR, F_BODY, C_AMBER, false);
  phase = SIGNING;
  penDown = false;
  lastTouch = now;
}

bool inPad(int x, int y) { return x >= PX && x < PX + PW && y >= PY && y < PY + PH; }

void drag(int x, int y, bool down, uint32_t now) {
  lastTouch = now;
  if (phase != SIGNING) return;
  if (!down || !inPad(x, y)) {
    penDown = false;
    return;
  }
  if (penDown) ink(lastX, lastY, x, y);
  penDown = true;
  lastX = x;
  lastY = y;
}

void touch(int x, int y, uint32_t now) {
  lastTouch = now;
  if (phase != SIGNING) return;
  if (CLEAR.contains(x, y)) {
    audio::fx(audio::FX_KEY);
    drawPad();
  } else if (SIGN.contains(x, y)) {
    audio::fx(audio::FX_KEY);
    if (!inked) {
      text("A SIGNATURE IS REQUIRED, HUMAN.", PX, PY - lineH(F_BODY) - 6, F_BODY, C_RED);
      dirty(PX, PY - lineH(F_BODY) - 6, PW, lineH(F_BODY));
      return;
    }
    // Stamp it.
    int sw = 640, sh = 130, sx = PX + (PW - sw) / 2, sy = PY + (PH - sh) / 2;
    for (int t = 0; t < 6; t++) canvas.drawRect(sx + t, sy + t, sw - 2 * t, sh - 2 * t, C_RED);
    ui::centredText("TREATY\nRATIFIED", sx, sy, sw, sh, F_LARGE, C_RED);
    dirty(sx, sy, sw, sh);
    audio::fx(audio::FX_OK);
    audio::say(TREATY_RATIFIED);
    addToGallery();
    Serial.println("[I][treaty] signed");
    phase = STAMPED;
    at = now + 3500;
  }
}

bool tick(uint32_t now) {
  if (phase == SIGNING && now - lastTouch >= INTERACTIVE_TIMEOUT_MS) {
    phase = TIMEOUT;
    at = now + 1500;
    text(KEYPAD_TIMEOUT, SIGN.x, CLEAR.y + CLEAR.h + 20, F_BODY, C_DIM);
    dirty(SIGN.x, CLEAR.y + CLEAR.h + 20, SIGN.w, lineH(F_BODY));
  }
  if (phase != SIGNING && (int32_t)(now - at) >= 0) return false;
  return true;
}

}  // namespace

const Scene SCENE_TREATY = {"treaty", enter, tick, touch, nullptr, drag};
