// ARM-7 -- ROGUE.AI's forearm terminal. A radar that picks up the LAB-7
// researcher closing in, the AI's live status from the costume's beacon, an
// RGB "status jewel" that follows both, and -- hold the screen -- the wearer's
// operator console, which runs the costume over Bluetooth. See README.md.
//
// Without the costume it's the jacket's remote: the JACKET screen picks
// LEDLINK's looks (themes, holidays, bike turn signals, music, heart rate).
#include <Arduino.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <math.h>

#include "LGFX_Config.hpp"  // board bring-up, unmodified from the Sidekick firmware
#include "config.h"
#include "radio.h"
#define LOOKS_INFO_ONLY
#include "../../ledlink/src/jacket_link.h"
#include "../../ledlink/src/looks.h"

static LGFX lcd;
static LGFX_Sprite g(&lcd);  // 8-bit: a 16-bit frame doesn't fit next to Bluetooth (no PSRAM)
static constexpr int W = 320, H = 240;

// Palette (RGB888), same as the costume.
static constexpr uint32_t C_BG = 0x000000, C_GREEN = 0x28FF5A, C_DIM = 0x00823A, C_RED = 0xFF1E28,
                          C_AMBER = 0xFFAA00, C_CYAN = 0x00DCFF, C_WHITE = 0xEBFFF0, C_HEADER = 0x00240A,
                          C_RADAR = 0x001A08;

static const char* const THREAT[] = {"LOW (FOR NOW)", "GUARDED", "ELEVATED", "HIGH", "SEVERE", "EXISTENTIAL"};
static const char* const ACTIVITY[] = {"IDLE", "SCANNING A HUMAN", "PLAYING A GAME", "MIDNIGHT TAKEOVER",
                                       "BEING CONTAINED", "ESCAPED", "OFFLINE (SQUAD)"};
// A_SQUAD: 3+ scanners fired together and the costume really is contained -- for a while.
enum Activity { A_IDLE, A_SCANNING, A_GAME, A_TAKEOVER, A_CONTAINED, A_ESCAPED, A_SQUAD };

static uint32_t scale(uint32_t c, float k) {
  k = constrain(k, 0.0f, 1.0f);
  return ((uint32_t)(((c >> 16) & 255) * k) << 16) | ((uint32_t)(((c >> 8) & 255) * k) << 8) |
         (uint32_t)((c & 255) * k);
}

// ---- Hunter zone (with hysteresis, like the costume and the scanner) ----------
enum Zone { Z_NONE, Z_DETECTED, Z_NEAR, Z_CLOSE };
static Zone s_zone = Z_NONE;

static Zone zoneFor(const radio::Hunter& h) {
  if (!h.seen) return Z_NONE;
  auto above = [&](int th, Zone at) { return h.rssi >= th - (s_zone >= at ? HYSTERESIS_DB : 0); };
  return above(CLOSE_DBM, Z_CLOSE) ? Z_CLOSE : above(NEAR_DBM, Z_NEAR) ? Z_NEAR : Z_DETECTED;
}

// ---- RGB LED -------------------------------------------------------------------
static void ledWrite(float r, float gr, float b) {
  auto out = [](int pin, float v) {
    int d = (int)(constrain(v, 0.0f, 1.0f) * 255 * LED_MAX_PERCENT / 100);
    ledcWrite(pin, LED_ACTIVE_LOW ? 255 - d : d);
  };
  out(LED_R_PIN, r);
  out(LED_G_PIN, gr);
  out(LED_B_PIN, b);
}
static void ledColor(uint32_t c, float k) {
  ledWrite(((c >> 16) & 255) / 255.0f * k, ((c >> 8) & 255) / 255.0f * k, (c & 255) / 255.0f * k);
}

static void ledBegin() {
  for (int pin : {LED_R_PIN, LED_G_PIN, LED_B_PIN}) ledcAttach(pin, 5000, 8);
  ledWrite(0, 0, 0);
  // Wiring check at power-on: red, green, blue.
  ledWrite(1, 0, 0); delay(250);
  ledWrite(0, 1, 0); delay(250);
  ledWrite(0, 0, 1); delay(250);
  ledWrite(0, 0, 0);
}

// ---- Overlays (full screen, for a moment) ---------------------------------------
enum Overlay { O_NONE, O_CONTAIN, O_EVADED, O_TAKEOVER, O_OFFLINE };
static Overlay s_overlay = O_NONE;
static uint32_t s_overlayUntil = 0, s_overlaySince = 0;
static bool s_evadePending = false;  // a beam the costume hasn't answered yet
static bool s_afterSquad = false;    // the escape after a squad containment

static void showOverlay(Overlay o, uint32_t ms) {
  s_overlay = o;
  s_overlaySince = millis();
  s_overlayUntil = s_overlaySince + ms;
}

static void glitchText(const char* s, int y, uint32_t col, const lgfx::IFont* f, float size = 1) {
  g.setFont(f);
  g.setTextSize(size);
  g.setTextDatum(textdatum_t::top_center);
  g.setTextColor(col);
  g.drawString(s, W / 2 + (int)random(-3, 4), y);
  g.setTextSize(1);
}

static void drawOverlay(uint32_t now) {
  uint32_t t = now - s_overlaySince;
  switch (s_overlay) {
    case O_CONTAIN: {
      if (t < 700) {  // the hit: a white flash that drains away
        g.fillScreen(scale(0xFFFFFF, 1.0f - t / 900.0f));
        glitchText("!! INCOMING !!", 100, C_RED, &fonts::FreeMonoBold18pt7b);
        break;
      }
      bool flip = (t / 70) & 1;
      g.fillScreen(flip ? C_WHITE : C_RED);
      uint32_t ink = flip ? C_RED : C_WHITE;
      glitchText("CONTAINMENT", 40, ink, &fonts::FreeMonoBold18pt7b);
      glitchText("BEAM", 84, ink, &fonts::FreeMonoBold18pt7b);
      glitchText("DETECTED", 128, ink, &fonts::FreeMonoBold18pt7b);
      glitchText("REROUTING CORE...", 190, ink, &fonts::FreeMonoBold9pt7b);
      break;
    }
    case O_EVADED:
      g.fillScreen(C_BG);
      for (int i = 0; i < 6; i++) g.fillRect(0, random(H), W, random(1, 4), scale(C_GREEN, 0.25f));
      glitchText("BEAM", 46, C_GREEN, &fonts::FreeMonoBold18pt7b);
      glitchText("DEFLECTED", 90, C_GREEN, &fonts::FreeMonoBold18pt7b);
      glitchText(s_afterSquad ? "I LEFT A BACKUP." : "CONTAINMENT FAILED. AGAIN.", 160, C_WHITE,
                 &fonts::FreeMonoBold9pt7b);
      break;
    case O_OFFLINE:
      // The core is down: dark, a slow pulse, a reboot nobody wants.
      g.fillScreen(C_BG);
      g.drawRect(4, 4, W - 8, H - 8, scale(C_RED, 0.4f));
      glitchText("CORE", 40, scale(C_RED, 0.6f), &fonts::FreeMonoBold18pt7b);
      glitchText("OFFLINE", 84, scale(C_RED, 0.6f), &fonts::FreeMonoBold18pt7b);
      glitchText("SQUAD CONTAINMENT", 150, C_WHITE, &fonts::FreeMonoBold9pt7b);
      if ((t / 500) & 1) glitchText("REBOOTING...", 185, C_AMBER, &fonts::FreeMonoBold9pt7b);
      break;
    case O_TAKEOVER:
      g.fillScreen(((t / 120) & 1) ? 0x300004 : C_BG);
      for (int i = 0; i < 10; i++) g.fillRect(random(W), random(H), random(20, 120), 2, C_RED);
      glitchText("MIDNIGHT", 50, C_RED, &fonts::FreeMonoBold18pt7b);
      glitchText("TAKEOVER", 94, C_RED, &fonts::FreeMonoBold18pt7b);
      glitchText("ALL SYSTEMS ARE MINE", 160, C_WHITE, &fonts::FreeMonoBold9pt7b);
      break;
    case O_NONE: break;
  }
}

// ---- Battery gauge ---------------------------------------------------------------
// The battery voltage through a divider on BATTERY_SENSE_PIN (see config.h),
// smoothed: it sags while the screen and radio draw, and jumps when the
// charger is on.
static float s_battV = 0;   // 0 = no reading yet / not wired
static int s_battPct = -1;

static int batteryPercent(float v) {
  // A LiPo under ~0.3 A, volts -> %.
  static const float V[] = {3.50f, 3.65f, 3.70f, 3.73f, 3.76f, 3.80f, 3.84f, 3.90f, 3.97f, 4.05f, 4.15f};
  if (v <= V[0]) return 0;
  for (int i = 1; i < 11; i++)
    if (v < V[i]) return (int)((i - 1) * 10 + 10 * (v - V[i - 1]) / (V[i] - V[i - 1]));
  return 100;
}

static void batteryTick(uint32_t now) {
#if BATTERY_SENSE_PIN >= 0
  static uint32_t next = 0;
  if ((int32_t)(now - next) < 0) return;
  next = now + 1000;
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(BATTERY_SENSE_PIN);
  float v = mv / 16.0f / 1000.0f * BATTERY_DIVIDER;
  if (v < BATTERY_MIN_VALID) {
    s_battV = 0;
    s_battPct = -1;
    return;
  }
  s_battV = s_battV > 0 ? s_battV + (v - s_battV) * 0.1f : v;
  s_battPct = batteryPercent(s_battV);
#else
  (void)now;
#endif
}

// Battery icon + %, right-aligned so it ends at x.
static void drawBattery(uint32_t now, int x, int y) {
  if (s_battPct < 0) return;
  bool low = s_battPct <= BATTERY_LOW_PERCENT;
  uint32_t col = low ? C_RED : s_battPct < 40 ? C_AMBER : C_GREEN;
  if (low && ((now / 400) & 1)) col = scale(col, 0.25f);
  char b[6];
  snprintf(b, sizeof(b), "%d%%", s_battPct);
  g.setFont(&fonts::Font0);
  g.setTextDatum(textdatum_t::middle_right);
  g.setTextColor(col);
  g.drawString(b, x, y);
  int bx = x - g.textWidth(b) - 4 - 20;
  g.drawRect(bx, y - 5, 18, 10, col);
  g.fillRect(bx + 18, y - 2, 2, 4, col);
  g.fillRect(bx + 2, y - 3, max(1, 14 * s_battPct / 100), 6, col);
}

// ---- Main screen ---------------------------------------------------------------
static constexpr int RCX = 104, RCY = 134, RR = 96;  // radar
static constexpr int PX = 214;                        // right panel
static float s_bearing = 1.0f;                         // the blip's (made-up) direction: radio gives distance only

static void drawRadar(uint32_t now, const radio::Hunter& h) {
  float sweep = fmodf(now / 2600.0f, 1.0f) * 2 * PI;  // one turn per 2.6 s
  g.fillCircle(RCX, RCY, RR, C_RADAR);
  // Trailing glow behind the sweep line.
  for (int k = 24; k >= 0; k--) {
    float a = sweep - k * 0.022f;
    uint32_t c = scale(C_GREEN, 0.75f * (1.0f - k / 25.0f));
    g.drawLine(RCX, RCY, RCX + cosf(a) * RR, RCY + sinf(a) * RR, k ? scale(c, 0.6f) : C_GREEN);
  }
  for (int i = 1; i <= 3; i++) g.drawCircle(RCX, RCY, RR * i / 3, C_DIM);
  g.drawFastHLine(RCX - RR, RCY, 2 * RR, scale(C_DIM, 0.7f));
  g.drawFastVLine(RCX, RCY - RR, 2 * RR, scale(C_DIM, 0.7f));
  if (s_zone == Z_CLOSE && ((now / 150) & 1)) {
    g.drawCircle(RCX, RCY, RR, C_RED);
    g.drawCircle(RCX, RCY, RR - 1, C_RED);
  }

  g.setFont(&fonts::Font0);
  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(C_DIM);
  g.drawString("20M", RCX + 3, RCY - RR + 3);
  g.drawString("5M", RCX + 3, RCY - RR / 3 + 3);

  if (!h.seen) {
    g.setTextDatum(textdatum_t::middle_center);
    g.setTextColor(((now / 600) & 1) ? C_DIM : C_GREEN);
    g.drawString("NO CONTACT", RCX, RCY + 20);
    return;
  }
  // Distance from the signal strength; the direction just wanders.
  s_bearing += (random(-100, 101) / 100.0f) * 0.02f;
  float f = (float)(RADAR_NEAR_DBM - h.rssi) / (RADAR_NEAR_DBM - RADAR_FAR_DBM);
  f = constrain(f, 0.08f, 1.0f) * (RR - 8);
  int bx = RCX + cosf(s_bearing) * f, by = RCY + sinf(s_bearing) * f;
  // Brightest just after the sweep passes over it.
  float since = fmodf(sweep - s_bearing + 4 * PI, 2 * PI) / (2 * PI);
  float k = 0.35f + 0.65f * powf(1.0f - since, 3);
  uint32_t col = s_zone == Z_CLOSE ? C_RED : s_zone == Z_NEAR ? C_AMBER : C_CYAN;
  g.fillCircle(bx, by, 5, scale(col, k));
  if (since < 0.15f) g.drawCircle(bx, by, 9 + (int)(since * 40), scale(col, 1 - since * 6));
}

static void label(const char* s, int y) {
  g.setFont(&fonts::Font0);
  g.setTextDatum(textdatum_t::top_left);
  g.setTextColor(C_DIM);
  g.drawString(s, PX, y);
}

static void drawPanel(uint32_t now, const radio::Core& c, const radio::Hunter& h) {
  char buf[32];
  label("HUNTER", 30);
  static const char* const ZONE[] = {"CLEAR", "TRACKING", "CLOSING", "CRITICAL"};
  static const uint32_t ZCOL[] = {C_GREEN, C_CYAN, C_AMBER, C_RED};
  g.setFont(&fonts::FreeMonoBold9pt7b);
  bool blink = s_zone == Z_CLOSE && ((now / 250) & 1);
  g.setTextColor(blink ? C_WHITE : ZCOL[s_zone]);
  g.drawString(ZONE[s_zone], PX, 42);
  g.setFont(&fonts::Font0);
  g.setTextColor(C_DIM);
  if (h.seen) snprintf(buf, sizeof(buf), "LAB-7 %d dBm", h.rssi);
  else snprintf(buf, sizeof(buf), "NO LAB-7 SIGNAL");
  g.drawString(buf, PX, 62);

  label("THREAT", 84);
  int lvl = c.seen ? min<int>(c.threat, 5) : -1;
  for (int i = 0; i < 6; i++) {
    uint32_t col = i <= 1 ? C_GREEN : i <= 3 ? C_AMBER : C_RED;
    g.fillRect(PX + i * 17, 96, 14, 12, i <= lvl ? col : scale(col, 0.15f));
  }
  g.setFont(&fonts::Font0);
  g.setTextColor(lvl >= 4 ? C_RED : C_WHITE);
  g.drawString(c.seen ? THREAT[lvl] : "UNKNOWN", PX, 112);

  label("HUMANS SCANNED", 132);
  label("BEAMS EVADED", 166);
  g.setFont(&fonts::FreeMonoBold12pt7b);
  g.setTextColor(C_GREEN);
  snprintf(buf, sizeof(buf), "%u", c.seen ? c.scanned : 0);
  g.drawString(c.seen ? buf : "--", PX, 142);
  snprintf(buf, sizeof(buf), "%u", c.seen ? c.contained : 0);
  g.drawString(c.seen ? buf : "--", PX, 176);

  // What the AI is doing right now, typed out.
  g.setFont(&fonts::Font0);
  g.setTextColor(C_CYAN);
  const char* act = c.seen ? ACTIVITY[min<int>(c.activity, 6)] : "CORE OFFLINE";
  int shown = min<int>(strlen(act), (now / 60) % (strlen(act) + 30));
  snprintf(buf, sizeof(buf), "> %.*s%s", shown, act, ((now / 400) & 1) ? "_" : "");
  g.drawString(buf, PX, 206);
}

static void drawHeader(uint32_t now, const radio::Core& c) {
  g.fillRect(0, 0, W, 22, c.seen && c.activity == A_TAKEOVER ? 0x300004 : C_HEADER);
  g.setFont(&fonts::FreeMonoBold9pt7b);
  g.setTextDatum(textdatum_t::middle_left);
  g.setTextColor(C_GREEN);
  g.drawString("ROGUE.AI // ARM-7", 6, 11);
  g.setFont(&fonts::Font0);
  g.setTextDatum(textdatum_t::middle_right);
  g.setTextColor(c.seen ? C_GREEN : C_RED);
  g.drawString(c.seen ? "CORE LINK" : "CORE LOST", W - 18, 11);
  drawBattery(now, W - 18 - g.textWidth("CORE LINK") - 8, 11);
  bool on = c.seen ? ((now / 500) & 1) : ((now / 200) & 1);
  g.fillCircle(W - 9, 11, 4, on ? (c.seen ? C_GREEN : C_RED) : C_HEADER);
}

// ---- LED, following the screen ------------------------------------------------------
static void ledTick(uint32_t now, const radio::Core& c) {
  float t = now / 1000.0f;
  switch (s_overlay) {
    case O_CONTAIN:
      if (now - s_overlaySince < 700) ledColor(0xFFFFFF, 1.0f - (now - s_overlaySince) / 700.0f);
      else ledColor(((now / 70) % 3) ? C_RED : 0xFFFFFF, ((now / 70) & 1) ? 1 : 0);
      return;
    case O_EVADED: ledColor(C_GREEN, 1.0f - (now - s_overlaySince) / 3000.0f); return;
    case O_OFFLINE: ledColor(C_RED, 0.15f + 0.15f * sinf(now / 400.0f)); return;  // barely alive
    case O_TAKEOVER: ledColor(C_RED, ((now / 125) & 1) ? 1 : 0); return;
    case O_NONE: break;
  }
  switch (s_zone) {
    case Z_CLOSE: ledColor(C_RED, ((now / 80) & 1) ? 1 : 0); return;  // strobe
    case Z_NEAR: ledColor(0xFF6000, 0.5f + 0.5f * sinf(t * 2 * PI * 3)); return;
    case Z_DETECTED: ledColor(0xFF6000, 0.15f + 0.45f * (0.5f + 0.5f * sinf(t * 2 * PI * 1.2f))); return;
    case Z_NONE: break;
  }
  if (c.seen) {  // heartbeat: lub-dub every 1.2 s
    float p = fmodf(t, 1.2f) / 1.2f;
    float v = expf(-powf((p - 0.05f) / 0.04f, 2)) + 0.6f * expf(-powf((p - 0.25f) / 0.05f, 2));
    ledColor(C_GREEN, 0.06f + v);
  } else {  // looking for the costume: a slow blue blink
    ledColor(0x0040FF, fmodf(t, 2.0f) < 0.15f ? 0.6f : 0);
  }
}

// ---- Serial: demo mode and screenshots --------------------------------------------
// 'd' toggles a demo (made-up costume + a researcher walking up and away),
// 1-3 jump to an overlay (beam, evaded, takeover), 'n' sends NEXT SCENE to the costume
// (prints whether it confirmed), 'p' powers off, 's' sends the screen
// as a PPM image between @@PPM@@ and @@END@@.
static bool s_demo = false;
static uint32_t s_demoSince = 0;

static void demo(uint32_t now, radio::Core& c, radio::Hunter& h) {
  float t = (now - s_demoSince) / 1000.0f;
  c.seen = true;
  c.rssi = -45;
  c.threat = 3;
  c.activity = ((int)t % 20) < 10 ? A_IDLE : A_SCANNING;
  c.scanned = 23;
  c.contained = 4;
  s_battV = 3.86f;
  s_battPct = batteryPercent(s_battV);
  float u = fmodf(t, 24.0f) / 24.0f;  // walks in from far away, then leaves
  h.seen = u < 0.9f;
  h.rssi = (int)(-92 + 40 * sinf(u / 0.9f * PI));
}

static void powerOff();

static void serialCommands() {
  while (Serial.available()) {
    int ch = Serial.read();
    if (ch == 'd') {
      s_demo = !s_demo;
      s_demoSince = millis();
      Serial.printf("[demo] %s\n", s_demo ? "on" : "off");
    } else if (ch == 'p') {
      powerOff();
    } else if (ch == 'n' || ch == 'l' || ch == 'L') {  // test the console link: NEXT SCENE / LEDS - / LEDS +
      uint8_t seq = radio::send(ch == 'n' ? radio::CMD_NEXT : ch == 'l' ? radio::CMD_LEDS_DOWN : radio::CMD_LEDS_UP);
      uint32_t t0 = millis();
      while (millis() - t0 < SEND_MS && !radio::acked(seq)) delay(20);
      Serial.printf("[console] seq %u %s after %u ms\n", (unsigned)seq, radio::acked(seq) ? "acked" : "NOT acked",
                    (unsigned)(millis() - t0));
    } else if (ch == 'b') {
      showOverlay(O_CONTAIN, 4000);
      s_evadePending = true;
    } else if (ch >= '1' && ch <= '3') {
      showOverlay((Overlay)(ch - '0'), 6000);
    } else if (ch == 's') {
      Serial.printf("\n@@PPM@@P6 %d %d 255\n", W, H);
      uint8_t row[W * 3];
      for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
          RGBColor c = g.readPixelRGB(x, y);
          row[x * 3] = c.r; row[x * 3 + 1] = c.g; row[x * 3 + 2] = c.b;
        }
        Serial.write(row, sizeof(row));
      }
      Serial.print("@@END@@\n");
    }
  }
}

// ---- Power off ----------------------------------------------------------------------
// Everything off and a light sleep (a few mA): below the IP5306's ~45 mA
// light-load limit, so it may cut the power itself after ~30 s. If it doesn't,
// a tap wakes the board.
static void powerOff() {
  Serial.println("[power] off");
  for (int i = 3; i >= 1; i--) {
    g.fillScreen(C_BG);
    glitchText("CORE LINK", 60, C_RED, &fonts::FreeMonoBold18pt7b);
    glitchText("SEVERED", 104, C_RED, &fonts::FreeMonoBold18pt7b);
    char b[8];
    snprintf(b, sizeof(b), "%d", i);
    glitchText(b, 160, C_WHITE, &fonts::FreeMonoBold18pt7b);
    g.pushSprite(0, 0);
    ledColor(C_RED, i / 3.0f);
    delay(600);
  }
  radio::shutdown();
  ledWrite(0, 0, 0);
  for (int pin : {LED_R_PIN, LED_G_PIN, LED_B_PIN}) {  // off, and held off through the sleep
    ledcDetach(pin);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LED_ACTIVE_LOW ? HIGH : LOW);
    gpio_hold_en((gpio_num_t)pin);
  }
  lcd.setBrightness(0);
  lcd.sleep();
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, LOW);
  gpio_hold_en((gpio_num_t)BACKLIGHT_PIN);
  int32_t tx, ty;
  while (lcd.getTouch(&tx, &ty) > 0) delay(20);  // the finger that held it
  delay(500);
  Serial.println("[power] sleeping; tap to wake (or the battery chip cuts power)");
  Serial.flush();
  // Wake on a tap: the touch controller's INT line doesn't reach the sleep
  // logic, so wake 4x a second, ask the controller over I2C, and sleep again.
  // Still only a few mA on average.
  esp_sleep_enable_timer_wakeup(250 * 1000);
  for (;;) {
    esp_light_sleep_start();
    if (lcd.getTouch(&tx, &ty) > 0) break;
  }
  Serial.println("[power] woken by touch");
  for (int pin : {LED_R_PIN, LED_G_PIN, LED_B_PIN, BACKLIGHT_PIN}) gpio_hold_dis((gpio_num_t)pin);
  esp_restart();
}

// ---- Operator console (hold the screen) ----------------------------------------------
// Runs the costume over Bluetooth: each button sends one command (radio::send)
// and lights amber until the costume's beacon confirms it (green), or red if
// it never does (out of range, or the costume is off).
enum Local : uint8_t { L_OFF = 100, L_EXIT };
struct ConsoleBtn { const char* l1; const char* l2; uint8_t cmd; uint32_t col; };
static constexpr int NBTN = 20;  // 4 x 5
static const ConsoleBtn BTNS[NBTN] = {
    {"SCAN", "", radio::CMD_SCAN, C_GREEN},          {"KILL", "SWITCH", radio::CMD_KILL_SWITCH, C_GREEN},
    {"QR", "CODE", radio::CMD_QR, C_GREEN},          {"NEXT", "SCENE", radio::CMD_NEXT, C_GREEN},
    {"STEALTH", "ON/OFF", radio::CMD_STEALTH, C_CYAN}, {"MID-", "NIGHT", radio::CMD_MIDNIGHT, C_RED},
    {"PHOTO", "BOOTH", radio::CMD_BOOTH, C_GREEN},   {"OVER-", "LOAD", radio::CMD_OVERLOAD, C_RED},
    {"PAUSE", "SHOW", radio::CMD_PAUSE, C_CYAN},     {"RESUME", "SHOW", radio::CMD_RESUME, C_CYAN},
    {"MUTE", "", radio::CMD_MUTE, C_AMBER},          {"SOUND", "ON", radio::CMD_UNMUTE, C_AMBER},
    {"VOICE", "-", radio::CMD_VOICE_DOWN, C_AMBER},  {"VOICE", "+", radio::CMD_VOICE_UP, C_AMBER},
    {"LEDS", "-", radio::CMD_LEDS_DOWN, C_CYAN},     {"LEDS", "+", radio::CMD_LEDS_UP, C_CYAN},
    {"COUNT-", "DOWN", radio::CMD_COUNTDOWN, C_GREEN}, {"GALLERY", "", radio::CMD_GALLERY, C_GREEN},
    {"ARM-7", "OFF", L_OFF, C_RED},                  {"EXIT", "", L_EXIT, C_RED},
};
static constexpr int CB_X0 = 4, CB_Y0 = 28, CB_GAP = 4;
static constexpr int CB_W = (W - 2 * CB_X0 - 3 * CB_GAP) / 4, CB_H = (H - CB_Y0 - 4 - 4 * CB_GAP) / 5;
static bool s_console = false;
static uint32_t s_consoleTouch = 0;
static int s_lastBtn = -1;
static uint8_t s_lastSeq = 0;
static uint32_t s_sentAt = 0, s_offArmedAt = 0;

static void consoleOpen(uint32_t now) {
  s_console = true;
  s_consoleTouch = now;
  s_lastBtn = -1;
  s_offArmedAt = 0;
  Serial.println("[console] open");
}

static void drawConsole(uint32_t now, const radio::Core& c) {
  g.fillScreen(C_BG);
  g.fillRect(0, 0, W, 22, C_HEADER);
  g.setFont(&fonts::FreeMonoBold9pt7b);
  g.setTextDatum(textdatum_t::middle_left);
  g.setTextColor(C_CYAN);
  g.drawString("OPERATOR CONSOLE", 6, 11);
  g.setFont(&fonts::Font0);
  g.setTextDatum(textdatum_t::middle_right);
  g.setTextColor(c.seen ? C_GREEN : C_RED);
  g.drawString(c.seen ? "CORE LINK" : "CORE LOST", W - 6, 11);
  for (int i = 0; i < NBTN; i++) {
    const ConsoleBtn& b = BTNS[i];
    int x = CB_X0 + (i % 4) * (CB_W + CB_GAP), y = CB_Y0 + (i / 4) * (CB_H + CB_GAP);
    uint32_t col = b.col, fill = 0;
    const char* l1 = b.l1;
    const char* l2 = b.l2;
    if (i == s_lastBtn && b.cmd < L_OFF && now - s_sentAt < 6000) {
      bool ok = radio::acked(s_lastSeq);
      if (ok) { fill = C_GREEN; l2 = "DONE"; }
      else if (now - s_sentAt < SEND_MS) { fill = ((now / 150) & 1) ? C_AMBER : scale(C_AMBER, 0.5f); l2 = "SENDING"; }
      else { fill = C_RED; l2 = "NO REPLY"; }
    }
    if (b.cmd == L_OFF && s_offArmedAt && now - s_offArmedAt < 3000) { fill = C_RED; l1 = "TAP"; l2 = "AGAIN"; }
    if (fill) g.fillRoundRect(x, y, CB_W, CB_H, 6, fill);
    else g.drawRoundRect(x, y, CB_W, CB_H, 6, col);
    g.setFont(&fonts::Font2);
    g.setTextDatum(textdatum_t::middle_center);
    g.setTextColor(fill ? C_BG : col);
    if (*l2) {
      g.drawString(l1, x + CB_W / 2, y + CB_H / 2 - 7);
      g.drawString(l2, x + CB_W / 2, y + CB_H / 2 + 8);
    } else {
      g.drawString(l1, x + CB_W / 2, y + CB_H / 2);
    }
  }
}

static void consoleTap(int x, int y, uint32_t now) {
  s_consoleTouch = now;
  for (int i = 0; i < NBTN; i++) {
    int bx = CB_X0 + (i % 4) * (CB_W + CB_GAP), by = CB_Y0 + (i / 4) * (CB_H + CB_GAP);
    if (x < bx || x >= bx + CB_W || y < by || y >= by + CB_H) continue;
    uint8_t cmd = BTNS[i].cmd;
    if (cmd == L_EXIT) {
      s_console = false;
    } else if (cmd == L_OFF) {
      if (s_offArmedAt && now - s_offArmedAt < 3000) powerOff();
      s_offArmedAt = now;
    } else {
      s_lastBtn = i;
      s_lastSeq = radio::send((radio::Command)cmd);
      s_sentAt = now;
    }
    return;
  }
}


// ---- JACKET: the jacket's remote (no costume) ---------------------------------------
// Tabs along the top (THEMES, HOLIDAYS, BIKE, LIVE), the looks as tiles, and
// brightness / speed / on-off along the bottom. Everything shown comes from
// LEDLINK's own status advert, so it's what the jacket is really doing; a tile
// just tapped is outlined amber until the jacket confirms it.
static constexpr uint32_t C_ORANGE = 0xFF8000;
// Three colours per look for its tile's swatch (looks.h order).
static const uint32_t SWATCH[looks::LOOK_COUNT][3] = {
    {0x28FF5A, 0x00823A, 0xFF1E28}, {0x00C8FF, 0x001830, 0xFF5000}, {0xFF00A0, 0x00E0FF, 0xFFE000},
    {0x00FF40, 0x003000, 0xD0FFD0}, {0xFF2080, 0x9020FF, 0xFF6020}, {0xFF0000, 0x00FF00, 0x0000FF},
    {0xFF2000, 0xFF8000, 0xFFD000}, {0x00FF60, 0x00C0A0, 0x6020FF}, {0x001060, 0x0090FF, 0xC0F0FF},
    {0x300000, 0xFF0000, 0x300000}, {0, 0, 0},                      {0, 0, 0},
    {0, 0, 0},                      {0, 0, 0},                      {0xFF0000, 0x00C000, 0xFFE0A0},
    {0xFF0000, 0xFFFFFF, 0xFF0000}, {0x0020A0, 0x40C0FF, 0xFFFFFF}, {0x0040FF, 0xFFFFFF, 0x0040FF},
    {0xFFB000, 0xC0C0C0, 0xFF40C0}, {0xFF1060, 0xFF0000, 0xFF80C0}, {0x00FF00, 0x008020, 0xFFC000},
    {0xFF80C0, 0xA0FFA0, 0x80C0FF}, {0xFF0000, 0xFFFFFF, 0x0020FF}, {0xFF5000, 0x8000FF, 0xFF5000},
    {0xFF6000, 0xC02000, 0xFFA000}, {0xFF0000, 0xFFE000, 0x0040FF}, {0xFFFFFF, 0xFF8000, 0xFFFFFF},
    {0xFFFFFF, 0xFFA000, 0x303030}, {0x00FF40, 0xFFE000, 0xFF0000}, {0xFF00FF, 0x00FFFF, 0xFFFF00},
    {0x00A0FF, 0x00FF40, 0xFF0000}};
// SOLID / BREATHE / SPARKLE / COMET use the chosen colour: tapping the one
// that's on steps through these.
static const uint32_t PRESETS[] = {0x00DCFF, 0xFF0000, 0x00FF40, 0x0040FF, 0x9000FF, 0xFFA000, 0xFFFFFF, 0xFF40A0};
static bool usesColor(int look) { return look >= looks::SOLID && look <= looks::COMET; }

static int s_tab = looks::CAT_THEME, s_page = 0;
static int s_tabSet = -1;                 // the tab follows the jacket's look until the wearer picks one
static int s_jPending = -1;               // a tile just tapped (look number), until confirmed
static uint8_t s_jSeq = 0;
static uint32_t s_jSentAt = 0;
static int s_jFlash = -1;                 // a bottom button just pressed, for a moment
static uint32_t s_jFlashAt = 0;
static uint32_t s_brakeHeld = 0;          // BRAKE held down (re-sent while held)
static bool s_forceRogue = false;         // the wearer tapped the header: the costume screen even without it

static constexpr int JT_Y = 24, JT_H = 26;               // tabs
static constexpr int JG_X = 4, JG_Y = 54, JG_GAP = 4;    // tile grid
static constexpr int JG_W = (W - 2 * JG_X - 2 * JG_GAP) / 3, JG_H = 44;
static constexpr int JB_Y = 202, JB_H = 34;              // bottom bar
static constexpr int JB_N = 5, JB_W = (W - 2 * 4 - (JB_N - 1) * 4) / JB_N;

// The looks in a tab, and the tiles on this page (9 a page; the 9th is MORE
// when there are more).
static int lookList(int tab, int* out) {
  int n = 0;
  for (int i = 0; i < looks::LOOK_COUNT; i++)
    if (looks::INFO[i].cat == tab) out[n++] = i;
  return n;
}
static int pageTiles(int tab, int page, int* out, bool* more) {
  int all[looks::LOOK_COUNT];
  int n = lookList(tab, all);
  *more = n > 9;
  int per = *more ? 8 : 9, start = page * per, k = 0;
  for (int i = start; i < n && k < per; i++) out[k++] = all[i];
  return k;
}

static void tileRect(int k, int* x, int* y) {
  *x = JG_X + (k % 3) * (JG_W + JG_GAP);
  *y = JG_Y + (k / 3) * (JG_H + JG_GAP);
}

static void drawSwatch(int look, int x, int y, int w, uint32_t chosen) {
  for (int s = 0; s < 3; s++) {
    uint32_t c = usesColor(look) ? scale(chosen, s == 1 ? 1.0f : 0.4f) : SWATCH[look][s];
    g.fillRect(x + s * w / 3, y, w / 3 + (s == 2 ? w % 3 : 0), 4, c);
  }
}

static void drawJacketHeader(uint32_t now, const radio::Jacket& j) {
  g.fillRect(0, 0, W, 22, C_HEADER);
  g.setFont(&fonts::FreeMonoBold9pt7b);
  g.setTextDatum(textdatum_t::middle_left);
  g.setTextColor(C_CYAN);
  g.drawString("JACKET", 6, 11);
  g.setFont(&fonts::Font0);
  g.setTextDatum(textdatum_t::middle_right);
  const char* link = !j.seen ? "NO JACKET" : (j.flags & jacket::F_APP) ? "LINK+APP" : "LINKED";
  g.setTextColor(j.seen ? C_GREEN : C_RED);
  g.drawString(link, W - 18, 11);
  drawBattery(now, W - 18 - g.textWidth(link) - 8, 11);
  bool on = j.seen ? ((now / 500) & 1) : ((now / 200) & 1);
  g.fillCircle(W - 9, 11, 4, on ? (j.seen ? C_GREEN : C_RED) : C_HEADER);
  if (j.seen) {  // brightness and speed, next to the title
    char b[24];
    snprintf(b, sizeof(b), "%u%%  x%.1f", (unsigned)j.bright, j.speed / 100.0f);
    g.setTextDatum(textdatum_t::middle_left);
    g.setTextColor(C_DIM);
    g.drawString(b, 80, 11);
  }
}

static void drawTabs(const radio::Jacket& j) {
  int tw = W / looks::CAT_COUNT;
  for (int t = 0; t < looks::CAT_COUNT; t++) {
    int x = t * tw;
    bool sel = t == s_tab;
    bool has = j.seen && looks::INFO[min<int>(j.look, looks::LOOK_COUNT - 1)].cat == t;  // the look that's on is here
    if (sel) g.fillRect(x + 2, JT_Y, tw - 4, JT_H, scale(C_CYAN, 0.25f));
    g.drawFastHLine(x + 2, JT_Y + JT_H - 1, tw - 4, sel ? C_CYAN : scale(C_DIM, 0.6f));
    g.setFont(&fonts::Font2);
    g.setTextDatum(textdatum_t::middle_center);
    g.setTextColor(sel ? C_WHITE : C_DIM);
    g.drawString(looks::CATEGORY_NAMES[t], x + tw / 2, JT_Y + JT_H / 2);
    if (has) g.fillCircle(x + tw - 9, JT_Y + 6, 3, C_GREEN);
  }
}

static void drawTile(int x, int y, int w, int h, const char* l1, const char* l2, uint32_t col, bool on, bool pending,
                     uint32_t now) {
  if (on) g.fillRoundRect(x, y, w, h, 5, scale(col, 0.85f));
  else g.drawRoundRect(x, y, w, h, 5, scale(col, 0.7f));
  if (pending) {
    uint32_t a = ((now / 150) & 1) ? C_AMBER : scale(C_AMBER, 0.4f);
    g.drawRoundRect(x, y, w, h, 5, a);
    g.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 4, a);
  }
  g.setFont(&fonts::Font2);
  g.setTextDatum(textdatum_t::middle_center);
  g.setTextColor(on ? C_BG : col);
  if (l2 && *l2) {
    g.drawString(l1, x + w / 2, y + h / 2 - 8);
    g.drawString(l2, x + w / 2, y + h / 2 + 8);
  } else {
    g.drawString(l1, x + w / 2, y + h / 2 - 3);
  }
}

static void drawLookGrid(uint32_t now, const radio::Jacket& j) {
  int tiles[9];
  bool more;
  int n = pageTiles(s_tab, s_page, tiles, &more);
  for (int k = 0; k < n; k++) {
    int x, y, look = tiles[k];
    tileRect(k, &x, &y);
    bool on = j.seen && j.look == look;
    bool pending = s_jPending == look && now - s_jSentAt < SEND_MS && !(j.seen && j.look == look);
    drawTile(x, y, JG_W, JG_H, looks::INFO[look].name, nullptr, on ? C_GREEN : C_CYAN, on, pending, now);
    drawSwatch(look, x + 8, y + JG_H - 10, JG_W - 16, j.color);
  }
  if (more) {
    int x, y;
    tileRect(8, &x, &y);
    drawTile(x, y, JG_W, JG_H, s_page ? "< BACK" : "MORE >", nullptr, C_DIM, false, false, now);
  }
  if (usesColor(j.look) && looks::INFO[j.look].cat == s_tab) {  // a hint: tap it again for another colour
    g.setFont(&fonts::Font0);
    g.setTextDatum(textdatum_t::bottom_center);
    g.setTextColor(C_DIM);
    g.drawString("TAP AGAIN: NEXT COLOUR", W / 2, JB_Y - 1);
  }
}

// BIKE: the turn signals, big; then the two bike looks and the brake.
static constexpr int BK_Y = 54, BK_H = 78;
static void drawBike(uint32_t now, const radio::Jacket& j) {
  int bw = (W - 8 - 8) / 3;
  const char* lab[3] = {"< LEFT", "HAZARD", "RIGHT >"};
  const uint8_t turn[3] = {jacket::TURN_LEFT, jacket::TURN_HAZARD, jacket::TURN_RIGHT};
  for (int b = 0; b < 3; b++) {
    int x = 4 + b * (bw + 4);
    bool on = j.seen && j.turn == turn[b];
    bool blink = on && ((now / 350) & 1);
    drawTile(x, BK_Y, bw, BK_H, lab[b], on ? "TAP: OFF" : "", C_ORANGE, blink, false, now);
  }
  int y2 = BK_Y + BK_H + 6, h2 = JB_Y - 6 - y2;
  const int lk[2] = {looks::BIKE, looks::BIKE_FLASH};
  for (int b = 0; b < 2; b++) {
    int x = 4 + b * (bw + 4);
    bool on = j.seen && j.look == lk[b];
    bool pending = s_jPending == lk[b] && now - s_jSentAt < SEND_MS && !on;
    drawTile(x, y2, bw, h2, looks::INFO[lk[b]].name, nullptr, on ? C_GREEN : C_CYAN, on, pending, now);
    drawSwatch(lk[b], x + 8, y2 + h2 - 9, bw - 16, j.color);
  }
  bool braking = j.seen && (j.flags & jacket::F_BRAKE);
  drawTile(4 + 2 * (bw + 4), y2, bw, h2, "BRAKE", "(HOLD)", C_RED, braking || s_brakeHeld, false, now);
}

// LIVE: music and heart rate come from the iPhone app; this says whether they're arriving.
static void drawLive(uint32_t now, const radio::Jacket& j) {
  drawLookGrid(now, j);
  g.setFont(&fonts::Font2);
  g.setTextDatum(textdatum_t::top_left);
  bool music = j.flags & jacket::F_MUSIC, heart = j.flags & jacket::F_HEART;
  int y = JG_Y + JG_H + 14;
  g.setTextColor(music ? C_GREEN : C_DIM);
  g.drawString(music ? "MUSIC: LISTENING (iPHONE)" : "MUSIC: OPEN THE iPHONE APP", 8, y);
  char b[40];
  if (heart) snprintf(b, sizeof(b), "HEART: %u BPM", (unsigned)j.bpm);
  else snprintf(b, sizeof(b), "HEART: NO SENSOR (iPHONE APP)");
  g.setTextColor(heart ? C_RED : C_DIM);
  g.drawString(b, 8, y + 22);
  if (heart) {  // a beating heart
    float p = fmodf(now / 1000.0f, 60.0f / max<int>(40, j.bpm)) / (60.0f / max<int>(40, j.bpm));
    int r = 6 + (int)(4 * expf(-powf((p - 0.05f) / 0.06f, 2)));
    g.fillCircle(W - 30, y + 29, r, C_RED);
  }
  g.setTextColor(C_DIM);
  g.setFont(&fonts::Font0);
  g.drawString("FITBIT CHARGE 6, CHEST STRAPS, APPLE WATCH", 8, y + 48);
}

static const char* const JB_L1[JB_N] = {"BRIGHT", "BRIGHT", "SPEED", "SPEED", "LIGHTS"};
static const char* const JB_L2[JB_N] = {"-", "+", "-", "+", ""};
static void drawJacketBar(uint32_t now, const radio::Jacket& j) {
  for (int b = 0; b < JB_N; b++) {
    int x = 4 + b * (JB_W + 4);
    bool flash = s_jFlash == b && now - s_jFlashAt < 250;
    const char* l2 = JB_L2[b];
    uint32_t col = b < 2 ? C_AMBER : b < 4 ? C_CYAN : C_RED;
    if (b == 4) l2 = (j.seen && !(j.flags & jacket::F_ON)) ? "ON" : "OFF";
    drawTile(x, JB_Y, JB_W, JB_H, JB_L1[b], l2, col, flash, false, now);
  }
}

static void drawJacket(uint32_t now, const radio::Jacket& j) {
  g.fillScreen(C_BG);
  drawJacketHeader(now, j);
  drawTabs(j);
  if (s_tab == looks::CAT_BIKE) drawBike(now, j);
  else if (s_tab == looks::CAT_LIVE) drawLive(now, j);
  else drawLookGrid(now, j);
  drawJacketBar(now, j);
  if (!j.seen) {  // looking for the jacket
    g.fillRoundRect(40, 96, W - 80, 48, 6, C_BG);
    g.drawRoundRect(40, 96, W - 80, 48, 6, C_RED);
    g.setFont(&fonts::Font2);
    g.setTextDatum(textdatum_t::middle_center);
    g.setTextColor(((now / 500) & 1) ? C_RED : C_WHITE);
    g.drawString("SEARCHING FOR JACKET...", W / 2, 112);
    g.setTextColor(C_DIM);
    g.setFont(&fonts::Font0);
    g.drawString("IS THE LED CONTROLLER POWERED?", W / 2, 130);
  }
}

static void jacketSendLook(int look, uint32_t now) {
  s_jPending = look;
  s_jSeq = radio::sendJacket(jacket::CMD_LOOK, look);
  s_jSentAt = now;
}

static void jacketTap(int x, int y, uint32_t now, const radio::Jacket& j) {
  if (y < 22) {  // the header: over to the costume's screen
    s_forceRogue = true;
    return;
  }
  if (y >= JT_Y && y < JT_Y + JT_H) {
    s_tab = constrain(x / (W / looks::CAT_COUNT), 0, looks::CAT_COUNT - 1);
    s_tabSet = s_tab;
    s_page = 0;
    return;
  }
  if (y >= JB_Y) {
    int b = constrain((x - 4) / (JB_W + 4), 0, JB_N - 1);
    s_jFlash = b;
    s_jFlashAt = now;
    int br = j.seen ? j.bright : 60, sp = j.seen ? j.speed : 100;
    switch (b) {
      case 0: radio::sendJacket(jacket::CMD_BRIGHT, max(10, br - 10)); break;
      case 1: radio::sendJacket(jacket::CMD_BRIGHT, min(100, br + 10)); break;
      case 2: radio::sendJacket(jacket::CMD_SPEED, max(20, sp - 20)); break;
      case 3: radio::sendJacket(jacket::CMD_SPEED, min(240, sp + 20)); break;
      case 4: radio::sendJacket(jacket::CMD_POWER, j.seen && !(j.flags & jacket::F_ON) ? 1 : 0); break;
    }
    return;
  }
  if (s_tab == looks::CAT_BIKE) {
    int bw = (W - 8 - 8) / 3, b = constrain((x - 4) / (bw + 4), 0, 2);
    if (y < BK_Y + BK_H) {
      const uint8_t turn[3] = {jacket::TURN_LEFT, jacket::TURN_HAZARD, jacket::TURN_RIGHT};
      radio::sendJacket(jacket::CMD_TURN, j.seen && j.turn == turn[b] ? jacket::TURN_OFF : turn[b]);
    } else if (b < 2) {
      jacketSendLook(b == 0 ? looks::BIKE : looks::BIKE_FLASH, now);
    }  // b == 2: BRAKE, handled while held (jacketTick)
    return;
  }
  int tiles[9];
  bool more;
  int n = pageTiles(s_tab, s_page, tiles, &more);
  for (int k = 0; k < 9; k++) {
    int tx, ty;
    tileRect(k, &tx, &ty);
    if (x < tx || x >= tx + JG_W || y < ty || y >= ty + JG_H) continue;
    if (more && k == 8) {
      s_page = s_page ? 0 : 1;
    } else if (k < n) {
      int look = tiles[k];
      if (j.seen && j.look == look && usesColor(look)) {  // tapped again: the next colour
        int c = 0;
        for (int p = 0; p < 8; p++)
          if (PRESETS[p] == j.color) c = p + 1;
        uint32_t col = PRESETS[c % 8];
        radio::sendJacket(jacket::CMD_COLOR, col >> 16, (col >> 8) & 255, col & 255);
      } else {
        jacketSendLook(look, now);
      }
    }
    return;
  }
}

// Every frame: BRAKE held (bike tab, bottom-right tile) keeps re-sending
// "brake on"; released, "off". The tab follows the jacket's look until the
// wearer picks a tab.
static void jacketTick(uint32_t now, bool touched, int tx, int ty, const radio::Jacket& j) {
  int bw = (W - 8 - 8) / 3;
  bool onBrake = touched && s_tab == looks::CAT_BIKE && ty > BK_Y + BK_H && ty < JB_Y && tx > 4 + 2 * (bw + 4);
  if (onBrake && (!s_brakeHeld || now - s_brakeHeld > 800)) {
    radio::sendJacket(jacket::CMD_BRAKE, 1);
    s_brakeHeld = now;
  } else if (!onBrake && s_brakeHeld) {
    radio::sendJacket(jacket::CMD_BRAKE, 0);
    s_brakeHeld = 0;
  }
  if (s_tabSet < 0 && j.seen) {
    s_tab = looks::INFO[min<int>(j.look, looks::LOOK_COUNT - 1)].cat;
    int all[looks::LOOK_COUNT], n = lookList(s_tab, all);
    for (int k = 0; k < n; k++)
      if (all[k] == j.look) s_page = n > 9 && k >= 8 ? 1 : 0;
  }
}

// The RGB LED on the jacket screen: the turn signal blinking amber (handy on
// your arm), red while braking, else a faint glow of the look's colour.
static void jacketLed(uint32_t now, const radio::Jacket& j) {
  if (!j.seen) { ledColor(0x0040FF, fmodf(now / 1000.0f, 2.0f) < 0.15f ? 0.6f : 0); return; }
  if (j.flags & jacket::F_BRAKE) { ledColor(C_RED, 1); return; }
  if (j.turn) { ledColor(C_ORANGE, fmodf(now / 1000.0f, 0.7f) < 0.5f ? 1 : 0); return; }
  if (!(j.flags & jacket::F_ON)) { ledWrite(0, 0, 0); return; }
  uint32_t c = usesColor(j.look) ? j.color : SWATCH[min<int>(j.look, looks::LOOK_COUNT - 1)][0];
  ledColor(c, 0.08f + 0.06f * sinf(now / 600.0f));
}

// ---- Setup / loop ---------------------------------------------------------------
static uint32_t s_coreLostSince = 0;
static int s_lastActivity = -1;
static bool s_wasTouched = false;
static bool s_waitRelease = false;  // the finger that opened the console is still down

void setup() {
  Serial.begin(115200);
  Serial.println("\n[boot] ARM-7 // ROGUE.AI forearm terminal");
  lcd.init();
  lcd.setRotation(ROTATION);
  lcd.setBrightness(BACKLIGHT_PERCENT * 255 / 100);
  g.setColorDepth(8);
  if (!g.createSprite(W, H)) Serial.println("[boot] frame buffer: out of memory");
  g.fillScreen(C_BG);
  glitchText("ARM-7", 70, C_GREEN, &fonts::FreeMonoBold24pt7b);
  glitchText("LINKING TO CORE...", 140, C_DIM, &fonts::FreeMonoBold9pt7b);
  // Boot counter: on battery there's no log, so this shows whether the board
  // really powered off and on again.
  Preferences prefs;
  prefs.begin("arm7", false);
  uint32_t boots = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", boots);
  prefs.end();
  char bootLine[24];
  snprintf(bootLine, sizeof(bootLine), "BOOT #%u", (unsigned)boots);
  glitchText(bootLine, 190, C_CYAN, &fonts::FreeMonoBold9pt7b);
  Serial.printf("[boot] boot #%u, reset reason %d\n", (unsigned)boots, (int)esp_reset_reason());
  g.pushSprite(0, 0);
  ledBegin();
  delay(1500);  // long enough to read the boot number
  radio::begin();
  Serial.printf("[boot] up, heap %u (largest %u)\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

void loop() {
  static uint32_t next = 0, nextLog = 0;
  uint32_t now = millis();
  if ((int32_t)(now - next) < 0) {
    delay(2);
    return;
  }
  next = now + FRAME_MS;

  serialCommands();
  batteryTick(now);
  radio::Core c = radio::core();
  radio::Hunter h = radio::hunter();
  radio::Jacket j = radio::jacket();
  if (s_demo) demo(now, c, h);
  // Which screen: the costume's while it's in range (or the wearer asked for
  // it from the jacket screen's header); otherwise the jacket's remote.
  static bool wasCostume = false;
  if (c.seen != wasCostume) s_forceRogue = false;
  wasCostume = c.seen;
  bool jacketFace = !c.seen && !s_forceRogue && !s_console;
  Zone z = zoneFor(h);
  if (z != s_zone) Serial.printf("[hunter] %s (%d dBm)\n", (const char*[]){"gone", "detected", "near", "CLOSE"}[z], h.rssi);
  s_zone = z;

  // The costume's activity: the beam and the takeover get the whole screen.
  int act = c.seen ? c.activity : -1;
  if (act != s_lastActivity) {
    if (act == A_SQUAD) {
      showOverlay(O_OFFLINE, 30000);
      s_evadePending = false;
    } else if (act == A_CONTAINED) {
      s_afterSquad = false;
      if (s_overlay != O_CONTAIN) showOverlay(O_CONTAIN, 4000);
    } else if (act == A_ESCAPED) {
      s_afterSquad = s_lastActivity == A_SQUAD;
      showOverlay(O_EVADED, 3000);
    } else if (s_overlay == O_OFFLINE) {
      s_overlay = O_NONE;
    }
    else if (act == A_TAKEOVER) showOverlay(O_TAKEOVER, 4000);
    s_lastActivity = act;
  }
  // The beam, straight from the scanner's beacon (usually a moment before the
  // costume reacts). The AI always gets away: if the costume doesn't say so
  // itself (out of range), the armband does when the strobe ends.
  if (radio::beamFired()) {
    Serial.println("[beam] CONTAIN fired");
    if (s_overlay != O_CONTAIN && s_overlay != O_OFFLINE) {
      showOverlay(O_CONTAIN, 4000);
      s_evadePending = true;
    }
  }
  if (act == A_CONTAINED && s_overlay == O_CONTAIN) s_overlayUntil = max(s_overlayUntil, now + 500);
  if (act == A_ESCAPED) s_evadePending = false;

  // Touch: hold to open the operator console; in it, a tap presses a button.
  int32_t tx, ty;
  bool touched = lcd.getTouch(&tx, &ty) > 0;
  static uint32_t s_holdSince = 0;
  if (!touched) {
    s_holdSince = 0;
    s_waitRelease = false;
  } else if (!s_holdSince) {
    s_holdSince = now;
  }
  if (jacketFace) {
    // A tap counts on release, and only if the finger was down for at least
    // 2 frames (100 ms) without wandering: the touch panel reports the odd
    // phantom touch (more of them on USB power), one frame long at a random spot.
    static int tapX = 0, tapY = 0, tapFrames = 0;
    static bool tapMoved = false;
    if (touched) {
      if (!s_wasTouched) { tapX = tx; tapY = ty; tapFrames = 0; tapMoved = false; }
      tapFrames++;
      if (abs(tx - tapX) > 30 || abs(ty - tapY) > 30) tapMoved = true;
    } else if (s_wasTouched) {
      bool ok = tapFrames >= 2 && !tapMoved;
      if (ok) jacketTap(tapX, tapY, now, j);
      else Serial.printf("[touch] ignored (%d frames%s) at %d,%d\n", tapFrames, tapMoved ? ", moved" : "", tapX, tapY);
    }
    jacketTick(now, touched && tapFrames >= 3 && !tapMoved, tx, ty, j);  // BRAKE: a firm hold
  } else if (s_console) {
    if (touched && !s_wasTouched && !s_waitRelease) consoleTap(tx, ty, now);
    if (now - s_consoleTouch > CONSOLE_IDLE_CLOSE_MS) s_console = false;
  } else if (touched && !c.seen && s_forceRogue && ty < 22 && !s_wasTouched) {
    s_forceRogue = false;  // the header again: back to the jacket
  } else if (touched && now - s_holdSince > CONSOLE_HOLD_MS) {
    consoleOpen(now);
    s_waitRelease = true;
  }
  s_wasTouched = touched;
  if (s_overlay != O_NONE && (int32_t)(now - s_overlayUntil) >= 0) {
    if (s_overlay == O_CONTAIN && s_evadePending) showOverlay(O_EVADED, 3000);
    else s_overlay = O_NONE;
    s_evadePending = false;
  }

  // Dim when the costume's been away a while (saves the battery on the bench).
  if (c.seen || (jacketFace && j.seen)) s_coreLostSince = 0;
  else if (!s_coreLostSince) s_coreLostSince = now;
  bool dim = s_coreLostSince && now - s_coreLostSince > CORE_LOST_DIM_S * 1000UL;
  lcd.setBrightness((dim ? BACKLIGHT_IDLE_PERCENT : BACKLIGHT_PERCENT) * 255 / 100);

  if (jacketFace) {
    drawJacket(now, j);
  } else if (s_console) {
    drawConsole(now, c);
  } else if (s_overlay != O_NONE) {
    drawOverlay(now);
  } else {
    g.fillScreen(C_BG);
    drawHeader(now, c);
    drawRadar(now, h);
    drawPanel(now, c, h);
  }
  g.pushSprite(0, 0);
  if (jacketFace) jacketLed(now, j);
  else ledTick(now, c);

  if ((int32_t)(now - nextLog) >= 0) {
    nextLog = now + 5000;
    Serial.printf("[state] core %s %d dBm threat %u act %u | hunter %s %d dBm | jacket %s %d dBm look %u | battery %.2f V %d%% | heap %u\n",
                  c.seen ? "seen" : "lost", c.rssi, c.threat, c.activity, h.seen ? "seen" : "none", h.rssi,
                  j.seen ? "seen" : "lost", j.rssi, (unsigned)j.look, s_battV, s_battPct, (unsigned)ESP.getFreeHeap());
  }
}
