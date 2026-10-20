#include "header.h"

#include "battery.h"
#include "config.h"
#include "gfx.h"
#include "messages.h"

using namespace gfx;

namespace header {

static int s_threat = 2;
static uint32_t s_nextThreatChange = 0;
static uint32_t s_shownSecond = UINT32_MAX;
static bool s_valid = false;
static int s_shownBattery = -2;
static bool s_shownCharging = false;
static bool s_hidden = false;
static const char* s_alert = nullptr;
static uint32_t s_alertColor = 0;

static uint32_t threatColor(int level) {
  int n = COUNT_OF(THREAT_LEVELS);
  if (level >= n - 2) return C_RED;
  if (level >= n / 3) return C_AMBER;
  return C_GREEN;
}

static void scheduleThreatChange(uint32_t now) {
  s_nextThreatChange = now + 1000 * rnd(THREAT_CHANGE_MIN_S, THREAT_CHANGE_MAX_S);
}

static void draw(uint32_t now) {
  canvas.fillRect(0, 0, W, HEADER_H, C_HEADER);
  canvas.drawFastHLine(0, HEADER_H - 1, W, C_DIM);

  int y = (HEADER_H - lineH(F_SMALL)) / 2;
  int cw = charW(F_SMALL);
  int x = MARGIN_X / 2;
  text(HEADER_LEFT, x, y, F_SMALL, C_GREEN);
  x += strlen(HEADER_LEFT) * cw;
  if (s_alert) text(s_alert, x, y, F_SMALL, s_alertColor);
  else text(THREAT_LEVELS[s_threat], x, y, F_SMALL, threatColor(s_threat));

  uint32_t s = now / 1000;
  char up[24];
  snprintf(up, sizeof(up), "UPTIME %02u:%02u:%02u", (unsigned)(s / 3600),
           (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  int upX = W - MARGIN_X / 2 - (int)strlen(up) * cw;
  text(up, upX, y, F_SMALL, C_GREEN);

#if SHOW_BATTERY
  // Battery: icon + percentage, left of the uptime.
  int pct = battery::percent();
  bool chg = battery::charging();
  s_shownBattery = pct;
  s_shownCharging = chg;
  char bat[16];
  if (pct < 0) {
    snprintf(bat, sizeof(bat), "USB");
  } else if (chg) {
    snprintf(bat, sizeof(bat), "CHG");  // the % reads high while charging
  } else {
    snprintf(bat, sizeof(bat), "%d%%", pct);
  }
  uint32_t bc = pct < 0 ? C_DIM : chg ? C_CYAN : pct <= 15 ? C_RED : pct <= 35 ? C_AMBER : C_GREEN;
  int bx = upX - 2 * cw - (int)strlen(bat) * cw;
  text(bat, bx, y, F_SMALL, bc);
  constexpr int IW = 34, IH = 18;
  int ix = bx - IW - 12, iy = (HEADER_H - IH) / 2;
  canvas.drawRect(ix, iy, IW, IH, bc);
  canvas.drawRect(ix + 1, iy + 1, IW - 2, IH - 2, bc);
  canvas.fillRect(ix + IW, iy + IH / 2 - 4, 3, 8, bc);  // terminal nub
  if (chg) {
    // Lightning bolt inside the battery outline.
    int cx = ix + IW / 2, cy = iy + IH / 2;
    canvas.fillTriangle(cx + 3, iy + 2, cx - 6, cy + 1, cx + 1, cy + 1, bc);
    canvas.fillTriangle(cx - 3, iy + IH - 2, cx + 6, cy - 1, cx - 1, cy - 1, bc);
  } else if (pct > 0) {
    canvas.fillRect(ix + 4, iy + 4, (IW - 8) * pct / 100, IH - 8, bc);
  }
#endif

  dirty(0, 0, W, HEADER_H);
  s_shownSecond = s;
  s_valid = true;
}

void begin(uint32_t now) {
  s_threat = rnd(1, 3);
  scheduleThreatChange(now);
  draw(now);
}

void tick(uint32_t now) {
  if (s_hidden) return;
  if ((int32_t)(now - s_nextThreatChange) >= 0) {
    int n = COUNT_OF(THREAT_LEVELS);
    int next = s_threat;
    while (next == s_threat) next = rnd(n);
    s_threat = next;
    scheduleThreatChange(now);
    s_valid = false;
  }
  if (!s_valid || now / 1000 != s_shownSecond || battery::percent() != s_shownBattery ||
      battery::charging() != s_shownCharging) {
    draw(now);
  }
}

void invalidate() { s_valid = false; }

void setAlert(const char* text, uint32_t color) {
  if (text == s_alert && color == s_alertColor) return;
  s_alert = text;
  s_alertColor = color;
  s_valid = false;
}

int threatLevel() { return s_threat; }

void setHidden(bool hidden) {
  s_hidden = hidden;
  s_valid = false;
}

void raiseThreat() {
  int n = COUNT_OF(THREAT_LEVELS);
  s_threat = n - 1 - (int)rnd(2);
  s_valid = false;
}

}  // namespace header
