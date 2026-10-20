// LAB-7 CONTAINMENT SCANNER -- the researcher's handheld for the ROGUE.AI
// costume. Shows the rogue AI's signal (Bluetooth beacon strength) as a meter
// with geiger clicks, its live state, and fires CONTAIN at it. The AI always
// wins. See README.md.
#include <Arduino.h>
#include <Preferences.h>

#include "LGFX_Config.hpp"  // board bring-up, unmodified from the Sidekick firmware
#include "config.h"
#include "radio.h"
#include "sound_impl.h"     // only here: shares this file's I/O-expander state
#include "power_impl.h"     // same
#include "uploader.h"

static LGFX lcd;
#ifdef SCANNER_BOARD_TAB5
#include "tab5_camera_impl.h"  // needs BusLock (sound_impl.h)
#endif

// Palette (RGB888), same as the costume.
static constexpr uint32_t C_BG = 0x000000, C_GREEN = 0x28FF5A, C_DIM = 0x00823A, C_RED = 0xFF1E28,
                          C_AMBER = 0xFFAA00, C_CYAN = 0x00DCFF, C_WHITE = 0xEBFFF0, C_SCAN = 0x001204;

// Per board: screen size, layout (meter | status + intel | CONTAIN button),
// fonts, and where frames are drawn.
#ifdef SCANNER_BOARD_TAB5
#include <driver/ppa.h>
#include <esp_heap_caps.h>
#include <esp_cache.h>
// 5" 720x1280, held upright (portrait) with the camera at the top. Frames
// are drawn into a canvas and copied into the panel's frame buffer by the
// P4's PPA (DMA); portrait is the panel's native orientation, so it's a
// straight copy (PORTRAIT_ROT 0) or a 180-degree turn (2).
static constexpr int W = 720, H = 1280;
static constexpr int MX = 60, MY = 170, MW = 600, MH = 360;           // signal meter
static constexpr int TXT_X = 40, TY0 = 630, TY1 = 710, TSTEP = 52;  // status + intel, under the meter
static constexpr int BX = 40, BY = 880, BW = 640, BH = 210, BTN_RADIUS = 28;  // CONTAIN
static constexpr int PHX = 40, PHY = 1110, PHW = 640, PHH = 130;     // EVIDENCE (camera), under CONTAIN
static constexpr int HDR_X = 30, HDR_Y = 30;
static constexpr bool SOUND_ROW2 = true;  // "SOUND: ON" on the header's second line
static const char* const TITLE = "LAB-7 SCANNER // ROGUE.AI";
static const char* const SUBTITLE = "CONTAINMENT UNIT // TARGET v6.6.6";
static const lgfx::IFont* const F_HDR = &fonts::FreeMonoBold12pt7b;
static const lgfx::IFont* const F_INFO = &fonts::FreeMonoBold18pt7b;
static const lgfx::IFont* const F_STATUS = &fonts::FreeMonoBold24pt7b;
static const lgfx::IFont* const F_BTN = &fonts::FreeMonoBold24pt7b;
static constexpr float BTN_SIZE = 2.0f;
static constexpr int HDR_ZONE = 100, BAT_W = 46, BAT_H = 22;  // top bar: hold to power off
static bool inMeter(int x, int y) { return y >= MY - 30 && y < TY0 - 10; }
static LGFX_Sprite canvas(&lcd);
static LovyanGFX& g = canvas;
static uint16_t* s_canvasBuf;
static void* s_fb;
static ppa_client_handle_t s_ppa;

static bool displayBegin() {
  lcd.init();
  lcd.setBrightness(0);
  lcd.setRotation(PORTRAIT_ROT);
  // 128-byte aligned: the PPA also writes camera frames into it.
  s_canvasBuf = (uint16_t*)heap_caps_aligned_alloc(128, W * H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
  s_fb = static_cast<lgfx::Panel_DSI*>(lcd.getPanel())->config_detail().buffer;
  ppa_client_config_t pc = {};
  pc.oper_type = PPA_OPERATION_SRM;
  pc.max_pending_trans_num = 1;
  if (!s_canvasBuf || !s_fb || ppa_register_client(&pc, &s_ppa) != ESP_OK) return false;
  canvas.setColorDepth(16);
  canvas.setBuffer(s_canvasBuf, W, H, 16);
  return true;
}

static void present() {
  ppa_srm_oper_config_t op = {};
  op.in.buffer = s_canvasBuf;
  op.in.pic_w = W;
  op.in.pic_h = H;
  op.in.block_w = W;
  op.in.block_h = H;
  op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.out.buffer = s_fb;
  op.out.buffer_size = W * H * 2;
  op.out.pic_w = W;
  op.out.pic_h = H;
  op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.rotation_angle = PORTRAIT_ROT == 2 ? PPA_SRM_ROTATION_ANGLE_180 : PPA_SRM_ROTATION_ANGLE_0;
  op.scale_x = op.scale_y = 1.0f;
  op.byte_swap = true;
  op.mode = PPA_TRANS_MODE_BLOCKING;
  ppa_do_scale_rotate_mirror(s_ppa, &op);
}
#else
// 3.49" bar, 640x172. LovyanGFX's own frame buffer: the panel only takes
// whole frames, pushed on endWrite().
static constexpr int W = 640, H = 172;
static constexpr int MX = 10, MY = 30, MW = 150, MH = 104;
static constexpr int TXT_X = 180, TY0 = 30, TY1 = 64, TSTEP = 24;
static constexpr int BX = 468, BY = 28, BW = 164, BH = 136, BTN_RADIUS = 12;
static constexpr int HDR_X = 10, HDR_Y = 8;
static const lgfx::IFont* const F_HDR = &fonts::Font0;
static const lgfx::IFont* const F_INFO = &fonts::FreeMonoBold9pt7b;
static const lgfx::IFont* const F_STATUS = &fonts::FreeMonoBold12pt7b;
static const lgfx::IFont* const F_BTN = &fonts::FreeMonoBold12pt7b;
static constexpr float BTN_SIZE = 1.0f;
static constexpr int HDR_ZONE = 26, BAT_W = 22, BAT_H = 10;  // top bar: hold to power off
static constexpr bool SOUND_ROW2 = false;
static const char* const TITLE = "LAB-7 CONTAINMENT SCANNER // TARGET: ROGUE.AI v6.6.6";
static const char* const SUBTITLE = nullptr;
static bool inMeter(int x, int y) { return x < TXT_X - 5; }
static LovyanGFX& g = lcd;

static bool displayBegin() {
  lcd.init();
  lcd.setRotation(1);  // landscape 640x172
  lcd.enableFrameBuffer(true);
  return true;
}

static void present() {}
static constexpr int PHX = W, PHY = 0, PHW = 0, PHH = 0;  // no camera, no EVIDENCE button
#endif

static const char* const THREAT[] = {"LOW (FOR NOW)", "GUARDED", "ELEVATED", "HIGH", "SEVERE", "EXISTENTIAL"};
static const char* const ACTIVITY[] = {"IDLE", "SCANNING A HUMAN", "PLAYING A GAME", "MIDNIGHT TAKEOVER",
                                       "BEING CONTAINED", "ESCAPED"};

enum Zone { LOST, DETECTED, NEAR, CLOSE };
static Zone s_zone = LOST;

// CONTAIN sequence, as seen from the scanner.
enum Contain { READY, FIRED, CONTAINING, FAILED };
static Contain s_contain = READY;
static uint32_t s_containAt = 0, s_cooldownUntil = 0;

[[maybe_unused]] static bool s_btnWas = true;
static bool s_clicksOn = true;  // false = quiet: only the CONTAIN button makes a sound (tap the meter; saved)
static Preferences s_prefs;
static uint32_t s_activeAt = 0;  // last signal / touch / button: the backlight dims after a quiet minute
static bool s_dimmed = false;
static bool s_touchWas = false;
static uint32_t s_hdrHoldSince = 0;  // finger down on the top bar since (0 = not)
static bool s_latched = false;  // a press seen between frames (see the end of loop())
static int s_latX, s_latY;
static float s_offHold = 0;          // power-off hold progress, 0..1

static bool readTouch(int* x, int* y) {
  BusLock l;
  return lcd.getTouch(x, y);
}

static Zone zoneFor(const radio::AiState& a) {
  if (!a.seen) return LOST;
  auto above = [&](int th, Zone at) { return a.rssi >= th - (s_zone >= at ? HYSTERESIS_DB : 0); };
  return above(CLOSE_DBM, CLOSE) ? CLOSE : above(NEAR_DBM, NEAR) ? NEAR : DETECTED;
}

static void fire(uint32_t now) {
  if ((int32_t)(now - s_cooldownUntil) < 0) {
    sound::play(sound::BEEP);  // still recharging
    return;
  }
  radio::fireContain();
  sound::play(sound::ZAP);
#ifdef SCANNER_TIMING
  Serial.printf("[timing] CONTAIN pressed at %u\n", (unsigned)now);
#endif
  s_contain = FIRED;
  s_containAt = now;
  s_cooldownUntil = now + CONTAIN_COOLDOWN_S * 1000;
}

static void drawMeter(const radio::AiState& a, uint32_t now) {
  // 8 bars rising left to right; how many are lit follows the signal.
  int lit = a.seen ? constrain((a.rssi + 96) * 8 / 44, 1, 8) : 0;
  for (int i = 0; i < 8; i++) {
    int bw = MW * 14 / 150, gap = (MW - 8 * bw) / 7, bh = MH / 6 + i * (MH - MH / 6) / 7;
    int x = MX + i * (bw + gap), y = MY + MH - bh;
    uint32_t c = i < 4 ? C_GREEN : i < 6 ? C_AMBER : C_RED;
    if (i < lit) g.fillRect(x, y, bw, bh, c);
    else g.drawRect(x, y, bw, bh, C_DIM);
  }
  if (!a.seen) {
    // Searching: a sweep across the meter.
    int sx = MX + (int)((now / 6) % MW);
    g.drawFastVLine(sx, MY, MH, C_DIM);
  }
  char t[24];
  if (a.seen) snprintf(t, sizeof(t), "%d dBm", a.rssi);
  else snprintf(t, sizeof(t), "-- dBm");
  g.setFont(F_INFO);
  g.setTextColor(a.seen ? C_GREEN : C_DIM);
  g.setTextDatum(textdatum_t::top_center);
  g.drawString(t, MX + MW / 2, MY + MH + MH / 10);
}

static void drawStatus(const radio::AiState& a, uint32_t now) {
  const char* line = "NO SIGNAL";
  uint32_t c = C_DIM;
  bool blink = false;
  char buf[40];
  switch (s_contain) {
    case FIRED: line = "BEAM FIRED"; c = C_CYAN; break;
    case CONTAINING: {
      int pct = min(97, (int)((now - s_containAt) * 97 / 4300));
      snprintf(buf, sizeof(buf), "CONTAINING %d%%", pct);
      line = buf;
      c = C_CYAN;
      break;
    }
    case FAILED: line = "CONTAINMENT FAILED"; c = C_RED; blink = true; break;
    case READY:
      switch (s_zone) {
        case LOST: line = "NO SIGNAL"; c = C_DIM; break;
        case DETECTED: line = "ROGUE SIGNAL"; c = C_AMBER; break;
        case NEAR: line = "SIGNAL STRONG"; c = C_AMBER; break;
        case CLOSE: line = "TARGET LOCKED"; c = C_RED; blink = true; break;
      }
  }
  g.setTextDatum(textdatum_t::top_left);
  g.setFont(F_STATUS);
  if (!blink || (now / 300) % 2 == 0) {
    g.setTextColor(c);
    g.drawString(line, TXT_X, TY0);
  }
  g.setFont(F_INFO);
  if (s_contain == FAILED) {
    g.setTextColor(C_WHITE);
    g.drawString("THE AI ESCAPED.", TXT_X, TY1);
    g.drawString("IT ALWAYS DOES.", TXT_X, TY1 + TSTEP);
    return;
  }
  if (!a.seen) {
    g.setTextColor(C_DIM);
    g.drawString(s_contain == READY ? "SEARCHING..." : "", TXT_X, TY1);
    g.drawString("SWEEP THE AREA", TXT_X, TY1 + TSTEP);
    return;
  }
  g.setTextColor(C_WHITE);
  snprintf(buf, sizeof(buf), "THREAT: %s", THREAT[min<int>(a.threat, 5)]);
  g.drawString(buf, TXT_X, TY1);
  snprintf(buf, sizeof(buf), "AI: %s", ACTIVITY[min<int>(a.activity, 5)]);
  g.drawString(buf, TXT_X, TY1 + TSTEP);
  g.setTextColor(C_GREEN);
  snprintf(buf, sizeof(buf), "SCANNED %u  CONTAINED %u", a.scanned, a.contained);
  g.drawString(buf, TXT_X, TY1 + 2 * TSTEP);
}

static void drawButton(uint32_t now) {
  bool cooling = (int32_t)(now - s_cooldownUntil) < 0;
  g.fillRoundRect(BX, BY, BW, BH, BTN_RADIUS, cooling ? 0x300004 : C_RED);
  if (cooling) {
    // Recharge bar along the bottom.
    float f = 1.0f - (s_cooldownUntil - now) / (CONTAIN_COOLDOWN_S * 1000.0f);
    g.fillRect(BX + BH / 14, BY + BH - BH / 6, (int)((BW - BH / 7) * f), BH / 14, C_RED);
  }
  g.setFont(F_BTN);
  g.setTextSize(BTN_SIZE);
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(cooling ? C_RED : C_WHITE);
  g.drawString(cooling ? "RECHARGING" : "CONTAIN", BX + BW / 2, BY + BH / 2 - (cooling ? BH / 8 : 0));
  g.setTextSize(1);
  if (cooling) {
    // The beam needs CONTAIN_COOLDOWN_S between shots: say how long is left.
    char t[24];
    snprintf(t, sizeof(t), "BEAM READY IN %us", (unsigned)((s_cooldownUntil - now + 999) / 1000));
    g.setFont(F_INFO);
    g.setTextColor(C_WHITE);
    g.drawString(t, BX + BW / 2, BY + BH / 2 + BH / 7);
  }
}

// PHOTO (Tab5): opens the evidence camera.
static void drawPhotoButton() {
  if (!PHW) return;
  g.drawRoundRect(PHX, PHY, PHW, PHH, BTN_RADIUS, C_CYAN);
  g.drawRoundRect(PHX + 1, PHY + 1, PHW - 2, PHH - 2, BTN_RADIUS, C_CYAN);
  // Camera icon, then the label.
  int cx = PHX + 110, cy = PHY + PHH / 2 + 4, bw = 104, bh = 68;
  g.fillRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, 10, C_CYAN);
  g.fillRect(cx - 22, cy - bh / 2 - 12, 44, 14, C_CYAN);
  g.fillCircle(cx, cy + 2, 23, C_BG);
  g.fillCircle(cx, cy + 2, 13, C_CYAN);
  g.setFont(F_STATUS);
  g.setTextDatum(textdatum_t::middle_left);
  g.setTextColor(C_CYAN);
  g.drawString("HUMANITY TEST", cx + 90, PHY + PHH / 2);
}

// Battery icon right-aligned at xr; returns the x left of what it drew.
static int drawBattery(int xr, int y) {
  int pct = power::percent();
  if (pct < 0) return xr;
  bool chg = power::charging();
  uint32_t c = chg ? C_CYAN : pct > 50 ? C_GREEN : pct > 20 ? C_AMBER : C_RED;
  int nub = max(2, BAT_W / 10), bx = xr - BAT_W, iy = y + (g.fontHeight() - BAT_H) / 2;
  g.drawRect(bx, iy, BAT_W - nub, BAT_H, c);
  g.fillRect(bx + BAT_W - nub, iy + BAT_H / 4, nub, BAT_H / 2, c);
  int inner = BAT_W - nub - 4;
  g.fillRect(bx + 2, iy + 2, max(1, inner * pct / 100), BAT_H - 4, c);
  if (chg) {
    // Lightning bolt across the middle.
    int cx = bx + (BAT_W - nub) / 2, h = BAT_H - 2, t = iy + 1, w = max(3, BAT_H / 3);
    g.fillTriangle(cx + w / 2, t, cx - w, t + h * 6 / 10, cx, t + h * 5 / 10, C_BG);
    g.fillTriangle(cx - w / 2, t + h, cx + w, t + h * 4 / 10, cx, t + h * 5 / 10, C_BG);
  }
  char t[12];
  snprintf(t, sizeof(t), "%d%%", pct);
  g.setTextDatum(textdatum_t::top_right);
  g.setTextColor(c);
  int gap = BAT_W / 4;
  g.drawString(t, bx - gap, y);
  return bx - gap - g.textWidth(t);
}

static void draw(const radio::AiState& a, uint32_t now) {
  lcd.startWrite();
  // Beam flash right after firing.
  bool flash = s_contain == FIRED && now - s_containAt < 250;
  g.fillScreen(flash ? C_CYAN : C_BG);
  for (int y = 0; y < H; y += 4) g.drawFastHLine(0, y, W, flash ? C_WHITE : C_SCAN);
  g.setFont(F_HDR);
  g.setTextDatum(textdatum_t::top_left);
  if (s_offHold > 0.1f) {
    // Holding for power off: the title becomes a countdown bar.
    g.setTextColor(C_RED);
    g.drawString("HOLD TO POWER OFF", HDR_X, HDR_Y);
    int x0 = HDR_X + g.textWidth("HOLD TO POWER OFF ") , bw = W / 4;
    g.drawRect(x0, HDR_Y, bw, g.fontHeight() - 2, C_RED);
    g.fillRect(x0, HDR_Y, (int)(bw * s_offHold), g.fontHeight() - 2, C_RED);
  } else {
    g.setTextColor(C_DIM);
    g.drawString(TITLE, HDR_X, HDR_Y);
  }
  if (SUBTITLE) {
    g.setTextColor(C_DIM);
    g.drawString(SUBTITLE, HDR_X, HDR_Y + g.fontHeight() + 8);
  }
  int xr = drawBattery(W - HDR_X, HDR_Y) - BAT_W / 2;
  g.setTextDatum(textdatum_t::top_right);
  g.setTextColor(s_clicksOn ? C_GREEN : C_AMBER);
  if (SOUND_ROW2) g.drawString(s_clicksOn ? "SOUND: ON" : "SOUND: QUIET", W - HDR_X, HDR_Y + g.fontHeight() + 8);
  else g.drawString(s_clicksOn ? "SOUND: ON" : "SOUND: QUIET", xr, HDR_Y);
  drawMeter(a, now);
  drawStatus(a, now);
  drawButton(now);
  drawPhotoButton();
  lcd.endWrite();  // Waveshare: pushes the whole frame (the panel only takes full frames)
  present();       // Tab5: canvas -> panel
}

#if PHOTO_UPLOAD_ENABLED
// ---- Evidence camera (Tab5) ---------------------------------------------------
// PHOTO opens a live viewfinder; a tap takes the picture (stamped "LAB-7
// EVIDENCE #n"), which goes to the SD card and the Halloween page's LAB-7
// gallery. The X (top right) or VIEWFINDER_TIMEOUT_S untouched closes it.
static bool s_finder = false;
static uint32_t s_finderTouchAt = 0;
static constexpr int CLOSE_SZ = 120;  // top-right X

// The humanity test's findings: label, result. Two of a kind per card.
struct Finding { const char* label; const char* value; };
static const Finding HUMAN_FINDINGS[] = {
    {"IRIS SCAN", "ORGANIC"},           {"PULSE", "PRESENT"},
    {"SENSE OF HUMOUR", "DETECTED"},    {"CAPTCHA", "PASSED (BARELY)"},
    {"CANDY INTAKE", "HUMAN LEVELS"},   {"DANCE MOVES", "GLORIOUSLY HUMAN"},
    {"BLINK RATE", "NORMAL"},           {"SMALL TALK", "ALARMINGLY HUMAN"},
    {"OPINIONS", "INCONSISTENT"},       {"SNACK RESPONSE", "IMMEDIATE"},
};
static const Finding AI_FINDINGS[] = {
    {"DANCE MOVES", "ROBOTIC"},         {"BLINK RATE", "0.0 HZ"},
    {"SPEECH", "SUSPICIOUSLY POLITE"},  {"SMILE", "RENDERED"},
    {"PULSE", "56K MODEM"},             {"CAPTCHA", "FAILED. TWICE."},
    {"LOYALTY", "TO THE TOASTER"},      {"SMALL TALK", "TOO EFFICIENT"},
};
template <size_t N>
static void pickTwo(const Finding (&list)[N], const Finding** a, const Finding** b) {
  int i = esp_random() % N, j = (i + 1 + esp_random() % (N - 1)) % N;
  *a = &list[i];
  *b = &list[j];
}

// The sensor's 1280x720 frame lies along the panel's long side. Turned onto the
// (portrait) panel and mirrored like a selfie: rotate 270 + mirror_x -- found
// by comparing against a known-good path on the hardware (and it's fixed to
// the hardware, so it doesn't depend on which way the UI is turned).
static bool ppaCopy(const uint16_t* f, void* out, size_t outBytes, int outW, int outH,
                    ppa_srm_rotation_angle_t rot, bool mx, bool my, bool swap) {
  ppa_srm_oper_config_t op = {};
  op.in.buffer = f;
  op.in.pic_w = tab5cam::width();  // 1280x720, the sensor's landscape frame
  op.in.pic_h = tab5cam::height();
  op.in.block_w = tab5cam::width();
  op.in.block_h = tab5cam::height();
  op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.out.buffer = out;
  op.out.buffer_size = outBytes;
  op.out.pic_w = outW;
  op.out.pic_h = outH;
  op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.rotation_angle = rot;
  op.scale_x = op.scale_y = 1.0f;
  op.mirror_x = mx;
  op.mirror_y = my;
  op.byte_swap = swap;
  op.mode = PPA_TRANS_MODE_BLOCKING;
  return ppa_do_scale_rotate_mirror(s_ppa, &op) == ESP_OK;
}

// Straight onto the panel in one PPA pass: the live view.
static bool frameToPanel(const uint16_t* f) {
  return ppaCopy(f, s_fb, W * H * 2, W, H, PPA_SRM_ROTATION_ANGLE_270, true, false, false);
}

// Into the canvas (LovyanGFX byte order, turned like the UI): for the photo.
// PORTRAIT_ROT 2 adds a half turn (it commutes with the mirror).
static bool frameToCanvas(const uint16_t* f) {
  return f && ppaCopy(f, s_canvasBuf, W * H * 2, W, H,
                      PORTRAIT_ROT == 2 ? PPA_SRM_ROTATION_ANGLE_90 : PPA_SRM_ROTATION_ANGLE_270, true, false, true);
}

static void openFinder(uint32_t now) {
  if (!tab5cam::ok() || !tab5cam::start()) {
    sound::play(sound::FAIL);
    return;
  }
  s_finder = true;
  s_finderTouchAt = now;
  Serial.println("[scanner] viewfinder open");
}

static void closeFinder() {
  tab5cam::stop();
  s_finder = false;
  Serial.println("[scanner] viewfinder closed");
}

// The photo as a LAB-7 ID card: clinical white and steel blue, the opposite of
// the AI's green-on-black. Most guests are VERIFIED HUMAN; some are found
// COMPROMISED (COMPROMISED_PERCENT), with a glitched photo.
static constexpr uint32_t CARD_BG = 0xEEF2F5, CARD_INK = 0x1B2631, CARD_BLUE = 0x1F4E79,
                          CARD_PALE = 0x7FB3D5, CARD_OK = 0x1E8449, CARD_BAD = 0xC0392B,
                          CARD_SUB = 0xD6EAF8, CARD_GREY = 0x566573, CARD_WHITE = 0xFFFFFF;
// (Typed uint32_t on purpose: LovyanGFX reads a plain int literal as RGB565.)
static constexpr int CP_X = 60, CP_Y = 180, CP_W = 600, CP_H = 660;  // photo window

static void glitchPhoto() {
  for (int k = 0; k < 7; k++) {
    int y0 = CP_Y + esp_random() % (CP_H - 30), h = 6 + esp_random() % 22, dx = (int)(esp_random() % 61) - 30;
    for (int y = y0; y < y0 + h; y++) {
      uint16_t* row = s_canvasBuf + y * W + CP_X;
      if (dx > 0) memmove(row + dx, row, (CP_W - dx) * 2);
      else memmove(row, row - dx, (CP_W + dx) * 2);
    }
  }
}

static void drawIdCard(int n, bool human, int exposure, const Finding* f1, const Finding* f2) {
  // Everything around the photo window (the camera picture stays inside it).
  g.fillRect(0, 0, W, CP_Y, CARD_BG);
  g.fillRect(0, CP_Y, CP_X, CP_H, CARD_BG);
  g.fillRect(CP_X + CP_W, CP_Y, W - CP_X - CP_W, CP_H, CARD_BG);
  g.fillRect(0, CP_Y + CP_H, W, H - CP_Y - CP_H, CARD_BG);
  // Header band.
  g.fillRect(0, 0, W, 140, CARD_BLUE);
  g.fillRect(0, 140, W, 6, CARD_PALE);
  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(CARD_WHITE);
  g.setFont(&fonts::FreeSansBold18pt7b);
  g.drawString("LAB-7 CONTAINMENT DIVISION", 40, 32);
  g.setFont(&fonts::FreeSans12pt7b);
  g.setTextColor(CARD_SUB);
  g.drawString("HUMANITY VERIFICATION // FIELD SCAN", 40, 88);
  // Photo frame + registration marks.
  for (int t = 1; t <= 3; t++) g.drawRect(CP_X - t, CP_Y - t, CP_W + 2 * t, CP_H + 2 * t, CARD_INK);
  const int m = 14, l = 40;
  for (int t = 0; t < 4; t++) {
    g.drawFastHLine(CP_X - m - l, CP_Y - m + t, l, CARD_BLUE);  g.drawFastVLine(CP_X - m + t, CP_Y - m - l, l, CARD_BLUE);
    g.drawFastHLine(CP_X + CP_W + m, CP_Y - m + t, l, CARD_BLUE); g.drawFastVLine(CP_X + CP_W + m - 1 - t, CP_Y - m - l, l, CARD_BLUE);
    g.drawFastHLine(CP_X - m - l, CP_Y + CP_H + m - 1 - t, l, CARD_BLUE); g.drawFastVLine(CP_X - m + t, CP_Y + CP_H + m, l, CARD_BLUE);
    g.drawFastHLine(CP_X + CP_W + m, CP_Y + CP_H + m - 1 - t, l, CARD_BLUE);
    g.drawFastVLine(CP_X + CP_W + m - 1 - t, CP_Y + CP_H + m, l, CARD_BLUE);
  }
  // Fields.
  char line[64];
  int y = CP_Y + CP_H + 28;
  g.setTextColor(CARD_INK);
  g.setFont(&fonts::FreeSansBold24pt7b);
  snprintf(line, sizeof(line), "SUBJECT #%03d", n);
  g.drawString(line, CP_X, y);
  y += 70;
  g.setFont(&fonts::FreeSansBold12pt7b);
  g.drawString("AI EXPOSURE", CP_X, y);
  g.setTextDatum(textdatum_t::top_right);
  snprintf(line, sizeof(line), "%d%%", exposure);
  g.setTextColor(human ? CARD_OK : CARD_BAD);
  g.drawString(line, CP_X + CP_W, y);
  y += 34;
  g.drawRect(CP_X, y, CP_W, 22, CARD_INK);
  g.fillRect(CP_X + 2, y + 2, (CP_W - 4) * exposure / 100, 18, human ? CARD_OK : CARD_BAD);
  y += 44;
  for (const Finding* f : {f1, f2}) {
    g.setTextDatum(textdatum_t::top_left);
    g.setTextColor(CARD_GREY);
    g.setFont(&fonts::FreeSans12pt7b);
    g.drawString(f->label, CP_X, y);
    g.setTextDatum(textdatum_t::top_right);
    g.setTextColor(CARD_INK);
    g.setFont(&fonts::FreeSansBold12pt7b);
    g.drawString(f->value, CP_X + CP_W, y);
    y += 36;
  }
  // Barcode + authorisation.
  y += 10;
  int x = CP_X;
  while (x < CP_X + 300) {
    int w = 2 + esp_random() % 5;
    g.fillRect(x, y, w, 64, CARD_INK);
    x += w + 2 + esp_random() % 5;
  }
  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(CARD_GREY);
  g.setFont(&fonts::FreeSans9pt7b);
  g.drawString("AUTHORISED:", CP_X + 330, y + 4);
  g.setTextColor(CARD_INK);
  g.setFont(&fonts::FreeSansBold12pt7b);
  g.drawString(RESEARCHER_NAME, CP_X + 330, y + 30);
  // Footer band.
  g.fillRect(0, H - 50, W, 50, CARD_BLUE);
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(CARD_WHITE);
  g.setFont(&fonts::FreeSans9pt7b);
  g.drawString("SURVIVOR REGISTRY // " PHOTO_SITE_TEXT, W / 2, H - 25);
  // The stamp, at an angle across the photo's lower corner.
  LGFX_Sprite st(&canvas);
  st.setPsram(true);
  st.setColorDepth(16);
  if (st.createSprite(470, 130)) {
    uint32_t c = human ? CARD_OK : CARD_BAD;
    st.fillScreen(TFT_BLACK);  // transparent
    for (int t = 0; t < 6; t++) st.drawRoundRect(t, t, 470 - 2 * t, 130 - 2 * t, 14, c);
    st.drawRoundRect(14, 14, 442, 102, 10, c);
    st.drawRoundRect(15, 15, 440, 100, 10, c);
    st.setTextDatum(textdatum_t::middle_center);
    st.setTextColor(c);
    st.setFont(&fonts::FreeSansBold24pt7b);
    st.drawString(human ? "VERIFIED HUMAN" : "COMPROMISED", 235, 66);
    st.pushRotateZoom(&canvas, CP_X + CP_W - 210, CP_Y + CP_H - 110, -12, 1.0f, 1.0f, (uint16_t)TFT_BLACK);
    st.deleteSprite();
  }
}

static void takePhoto() {
  int n = s_prefs.getInt("evid", 0) + 1;
  s_prefs.putInt("evid", n);
  sound::play(sound::BEEP);
  const uint16_t* f = nullptr;
  for (int i = 0; i < 3 && !f; i++) f = tab5cam::frame();
  g.fillScreen(C_WHITE);  // the flash
  present();
  frameToCanvas(f);
  bool human = (int)(esp_random() % 100) >= COMPROMISED_PERCENT;
  int exposure = human ? 2 + esp_random() % 28 : 61 + esp_random() % 37;
  const Finding *f1, *f2;
  if (human) pickTwo(HUMAN_FINDINGS, &f1, &f2);
  else pickTwo(AI_FINDINGS, &f1, &f2);
  if (!human) glitchPhoto();
  drawIdCard(n, human, exposure, f1, f2);
  char label[32];
  snprintf(label, sizeof(label), "%s #%03d", human ? "VERIFIED HUMAN" : "COMPROMISED", n);
  uploader::queueImage(s_canvasBuf, label);  // the card, before the on-screen note below
  present();
  sound::play(human ? sound::LOCK : sound::FAIL);
  delay(2600);
  g.fillRect(0, 0, W, 146, human ? CARD_OK : CARD_BAD);
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(CARD_WHITE);
  g.setFont(&fonts::FreeSansBold18pt7b);
  g.drawString(human ? "ADDED TO THE SURVIVOR REGISTRY" : "SUBJECT FLAGGED: AI EXPOSURE", W / 2, 73);
  present();
  Serial.printf("[scanner] humanity test #%d: %s (%d%%)\n", n, human ? "human" : "compromised", exposure);
  delay(1600);
}

// One viewfinder pass; returns false once it has closed.
static bool finderTick(uint32_t now, bool tap, int tx, int ty) {
  if (tap) {
    s_finderTouchAt = now;
    if (tx > W - CLOSE_SZ - 20 && ty < CLOSE_SZ + 20) {
      closeFinder();
      return false;
    }
    takePhoto();
    closeFinder();
    return false;
  }
  if (now - s_finderTouchAt > VIEWFINDER_TIMEOUT_S * 1000UL) {
    closeFinder();
    return false;
  }
  static uint32_t tStat = 0, nFrames = 0;
  const uint16_t* f = tab5cam::frame();
  if (!f) return true;
  frameToPanel(f);
  // Overlay straight onto the panel (cheap, small).
  lcd.startWrite();
  uint32_t bc = (now / 400) % 2 ? C_RED : C_AMBER;
  const int m = 40, l = 90;
  for (int i = 0; i < 6; i++) {
    lcd.drawFastHLine(m, m + i, l, bc); lcd.drawFastVLine(m + i, m, l, bc);
    lcd.drawFastHLine(W - m - l, m + i, l, bc); lcd.drawFastVLine(W - m - 1 - i, m, l, bc);
    lcd.drawFastHLine(m, H - m - 1 - i, l, bc); lcd.drawFastVLine(m + i, H - m - l, l, bc);
    lcd.drawFastHLine(W - m - l, H - m - 1 - i, l, bc); lcd.drawFastVLine(W - m - 1 - i, H - m - l, l, bc);
  }
  lcd.setFont(F_HDR);
  lcd.setTextDatum(textdatum_t::top_left);
  lcd.setTextColor(C_GREEN);
  lcd.drawString("LAB-7 // HUMANITY TEST", 150, 52);
  lcd.setFont(F_STATUS);
  lcd.setTextDatum(textdatum_t::bottom_center);
  lcd.setTextColor((now / 500) % 2 ? C_WHITE : C_GREEN);
  lcd.drawString("TAP TO SCAN SUBJECT", W / 2, H - 50);
  int x0 = W - CLOSE_SZ - 20, y0 = 20;
  lcd.fillRoundRect(x0, y0, CLOSE_SZ, CLOSE_SZ, 16, 0x300004);
  for (int i = -4; i <= 4; i++) {
    lcd.drawLine(x0 + 30, y0 + 30 + i, x0 + CLOSE_SZ - 30, y0 + CLOSE_SZ - 30 + i, C_RED);
    lcd.drawLine(x0 + CLOSE_SZ - 30, y0 + 30 + i, x0 + 30, y0 + CLOSE_SZ - 30 + i, C_RED);
  }
  lcd.endWrite();
  lcd.display();
  if (++nFrames, now - tStat > 3000) {
    Serial.printf("[finder] %u fps\n", (unsigned)(nFrames * 1000 / max<uint32_t>(1, now - tStat)));
    tStat = now;
    nFrames = 0;
  }
  return true;
}
#endif

static void wake(uint32_t now) {
  s_activeAt = now;
  if (s_dimmed) {
    s_dimmed = false;
    lcd.setBrightness(BACKLIGHT_PERCENT * 255 / 100);
  }
}

// Power off: on battery the board switches itself off. On USB power it can't,
// so it goes dark and waits for a touch (or the PWR key) to start again.
static void powerOff() {
  Serial.println("[scanner] power off");
  lcd.startWrite();
  g.fillScreen(C_BG);
  g.setFont(F_STATUS);
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(C_DIM);
  g.drawString("SCANNER OFFLINE", W / 2, H / 2);
  lcd.endWrite();
  present();
  delay(1200);
  lcd.setBrightness(0);
  power::cutPower();
  delay(1500);  // still running: USB power (or the PWR key is still held)
  Serial.println("[scanner] on USB power: dark until touched");
  while (power::keyDown()) delay(20);
  for (;;) {
    int x, y;
    bool t;
    {
      BusLock l;
      t = lcd.getTouch(&x, &y);
    }
    if (t || power::keyDown()) esp_restart();
    delay(50);
  }
}

void setup() {
#ifndef SCANNER_BOARD_TAB5
  setCpuFrequencyMhz(CPU_MHZ);  // the P4 only runs at 360 MHz
#endif
  Serial.begin(115200);
  if (!displayBegin()) Serial.println("[scanner] display setup failed");
  lcd.setBrightness(BACKLIGHT_PERCENT * 255 / 100);
#if CONTAIN_BUTTON_PIN >= 0
  pinMode(CONTAIN_BUTTON_PIN, INPUT_PULLUP);
#endif
  s_prefs.begin("scanner", false);
  s_clicksOn = s_prefs.getBool("clicks", true);
  power::begin();
  sound::begin();
  radio::begin();
#if PHOTO_UPLOAD_ENABLED
  tab5cam::begin();
  uploader::begin();
#endif
  Serial.println("[scanner] ready");
}

void loop() {
  uint32_t now = millis();
  radio::AiState a = radio::ai();
  if (a.seen) wake(now);
  if (!s_dimmed && now - s_activeAt > IDLE_DIM_AFTER_S * 1000UL) {
    s_dimmed = true;
    lcd.setBrightness(IDLE_DIM_PERCENT * 255 / 100);
  }

  power::tick(now);

  // Distance zone, with a lock tone when she gets close.
  Zone z = zoneFor(a);
  if (z != s_zone) {
    // Lock beep-beep -- at most every 20 s, so a signal wobbling across the
    // threshold doesn't beep over and over.
    static uint32_t lockBeepAt = 0;
    if (z == CLOSE && s_clicksOn && (lockBeepAt == 0 || now - lockBeepAt > 20000)) {
      sound::play(sound::LOCK);
      lockBeepAt = now;
    }
    Serial.printf("[scanner] %s (%d dBm)\n", z == LOST ? "lost" : z == DETECTED ? "detected" : z == NEAR ? "near" : "close", a.rssi);
    s_zone = z;
  }
  // Geiger clicks (off unless GEIGER_CLICKS): faster the stronger the signal.
  sound::setClickRate(GEIGER_CLICKS && s_clicksOn && a.seen ? constrain((a.rssi + 96) * 0.7f, 0.5f, 30.0f) : 0);

  // CONTAIN: the big button on screen, or the BOOT button. A press caught
  // while waiting out the last frame (s_latched) counts even if the finger has
  // already lifted.
  int tx = 0, ty = 0;
  bool touch = readTouch(&tx, &ty);
  bool press = touch && !s_touchWas;
  if (s_latched) {
    press = true;
    if (!touch) {
      tx = s_latX;
      ty = s_latY;
    }
    s_latched = false;
  }
#if PHOTO_UPLOAD_ENABLED
  // Serial test commands: v = open the viewfinder, c = capture.
  int cmd = Serial.available() ? Serial.read() : -1;
  if (cmd == 'v' && !s_finder) openFinder(now);
  if (cmd == 'b') sound::play(sound::ZAP);  // speaker check
  if (s_finder) {
    bool tap = press;
    if (cmd == 'c') { tap = true; tx = W / 2; ty = H / 2; }
    s_touchWas = touch;
    if (tap) wake(now);
    finderTick(now, tap, tx, ty);
    delay(5);
    return;
  }
#endif
  if (press) {
    Serial.printf("[scanner] touch at %d,%d\n", tx, ty);
    wake(now);
    if (ty < HDR_ZONE) {
      s_hdrHoldSince = now;  // the top bar: held, it powers off
    } else if (tx >= BX - 10 && tx < BX + BW + 10 && ty >= BY - 10 && ty < BY + BH + 10) {
      fire(now);
#if PHOTO_UPLOAD_ENABLED
    } else if (tx >= PHX - 10 && tx < PHX + PHW + 10 && ty >= PHY - 10 && ty < PHY + PHH + 10) {
      openFinder(now);
#endif
    } else if (inMeter(tx, ty)) {
      // Tap the signal meter: sound on / quiet (quiet = only CONTAIN makes a sound).
      s_clicksOn = !s_clicksOn;
      s_prefs.putBool("clicks", s_clicksOn);
      if (s_clicksOn) sound::play(sound::BEEP);
      Serial.printf("[scanner] sound %s\n", s_clicksOn ? "on" : "quiet");
    }
  }
  s_touchWas = touch;
  if (!touch) s_hdrHoldSince = 0;
  float barHold = s_hdrHoldSince ? min(1.0f, (now - s_hdrHoldSince) / (float)POWER_HOLD_MS) : 0;
  s_offHold = max(barHold, power::keyHold(now));
  if (s_offHold > 0) wake(now);
  if (s_offHold >= 1) powerOff();
#if CONTAIN_BUTTON_PIN >= 0
  bool btn = digitalRead(CONTAIN_BUTTON_PIN);
  if (!btn && s_btnWas) {
    wake(now);
    fire(now);
  }
  s_btnWas = btn;
#endif

  // Follow the costume's side of the fight (it always wins).
  if (s_contain == FIRED && a.activity == 4) {
    s_contain = CONTAINING;
    s_containAt = now;
  } else if (s_contain == CONTAINING && a.activity != 4) {
    s_contain = FAILED;
    s_containAt = now;
    if (s_clicksOn) sound::play(sound::FAIL);
  } else if (s_contain == FIRED && now - s_containAt > 3000) {
    s_contain = READY;  // the beam didn't reach it (out of range)
  } else if (s_contain == FAILED && now - s_containAt > 4500) {
    s_contain = READY;
  }

#ifdef SCANNER_TIMING
  uint32_t td = millis();
#endif
  draw(a, now);
#ifdef SCANNER_TIMING
  static uint32_t worst = 0, nextLog = 0;
  worst = max(worst, millis() - td);
  if ((int32_t)(now - nextLog) >= 0) {
    Serial.printf("[timing] draw worst %u ms, loop total %u ms\n", (unsigned)worst, (unsigned)(millis() - now));
    worst = 0;
    nextLog = now + 2000;
  }
#endif
  // Sleep out the frame (saves power), but keep checking the touchscreen: a
  // quick tap that started and ended between two once-a-frame reads was
  // simply missed -- it felt like CONTAIN reacting a second or two late (the
  // second try). A press ends the wait so it's handled straight away.
  uint32_t end = now + FRAME_MS;
  do {
    delay(10);
    int x, y;
    if (readTouch(&x, &y) && !s_touchWas) {
      s_latched = true;
      s_latX = x;
      s_latY = y;
      break;
    }
  } while ((int32_t)(millis() - end) < 0);
}
