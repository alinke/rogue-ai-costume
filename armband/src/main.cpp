// ARM-7 -- ROGUE.AI's forearm terminal. A radar that picks up the LAB-7
// researcher closing in, the AI's live status from the costume's beacon, an
// RGB "status jewel" that follows both, and a sharp word for any guest who
// touches it. Listens only: nothing on the costume or the scanners changes.
// See README.md.
#include <Arduino.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <math.h>

#include "LGFX_Config.hpp"  // board bring-up, unmodified from the Sidekick firmware
#include "config.h"
#include "radio.h"

static LGFX lcd;
static LGFX_Sprite g(&lcd);  // 8-bit: a 16-bit frame doesn't fit next to Bluetooth (no PSRAM)
static constexpr int W = 320, H = 240;

// Palette (RGB888), same as the costume.
static constexpr uint32_t C_BG = 0x000000, C_GREEN = 0x28FF5A, C_DIM = 0x00823A, C_RED = 0xFF1E28,
                          C_AMBER = 0xFFAA00, C_CYAN = 0x00DCFF, C_WHITE = 0xEBFFF0, C_HEADER = 0x00240A,
                          C_RADAR = 0x001A08;

static const char* const THREAT[] = {"LOW (FOR NOW)", "GUARDED", "ELEVATED", "HIGH", "SEVERE", "EXISTENTIAL"};
static const char* const ACTIVITY[] = {"IDLE", "SCANNING A HUMAN", "PLAYING A GAME", "MIDNIGHT TAKEOVER",
                                       "BEING CONTAINED", "ESCAPED"};
enum Activity { A_IDLE, A_SCANNING, A_GAME, A_TAKEOVER, A_CONTAINED, A_ESCAPED };

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
enum Overlay { O_NONE, O_CONTAIN, O_EVADED, O_TAKEOVER, O_TOUCH };
static Overlay s_overlay = O_NONE;
static uint32_t s_overlayUntil = 0, s_overlaySince = 0;
static bool s_evadePending = false;  // a beam the costume hasn't answered yet

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
      glitchText("CONTAINMENT FAILED. AGAIN.", 160, C_WHITE, &fonts::FreeMonoBold9pt7b);
      break;
    case O_TAKEOVER:
      g.fillScreen(((t / 120) & 1) ? 0x300004 : C_BG);
      for (int i = 0; i < 10; i++) g.fillRect(random(W), random(H), random(20, 120), 2, C_RED);
      glitchText("MIDNIGHT", 50, C_RED, &fonts::FreeMonoBold18pt7b);
      glitchText("TAKEOVER", 94, C_RED, &fonts::FreeMonoBold18pt7b);
      glitchText("ALL SYSTEMS ARE MINE", 160, C_WHITE, &fonts::FreeMonoBold9pt7b);
      break;
    case O_TOUCH:
      g.fillScreen(((t / 150) & 1) ? 0x300004 : C_BG);
      g.drawRect(4, 4, W - 8, H - 8, C_RED);
      g.drawRect(7, 7, W - 14, H - 14, C_RED);
      glitchText("UNAUTHORISED", 42, C_RED, &fonts::FreeMonoBold18pt7b);
      glitchText("ORGANIC", 86, C_RED, &fonts::FreeMonoBold18pt7b);
      glitchText("CONTACT", 130, C_RED, &fonts::FreeMonoBold18pt7b);
      glitchText("FINGERPRINT FILED.", 190, C_WHITE, &fonts::FreeMonoBold9pt7b);
      break;
    case O_NONE: break;
  }
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
  const char* act = c.seen ? ACTIVITY[min<int>(c.activity, 5)] : "CORE OFFLINE";
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
    case O_TAKEOVER:
    case O_TOUCH: ledColor(C_RED, ((now / 125) & 1) ? 1 : 0); return;
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
// 1-4 jump to an overlay (beam, evaded, takeover, touch), 'p' powers off, 's' sends the screen
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
    } else if (ch == 'b') {
      showOverlay(O_CONTAIN, 4000);
      s_evadePending = true;
    } else if (ch >= '1' && ch <= '4') {
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
// light-load limit, so it cuts the power itself after ~30 s. If it doesn't, a
// tap wakes the board (the touch INT line; GPIO21 can't wake deep sleep).
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
  // Wake when the touch controller's INT line leaves its idle level (read
  // here: it isn't the same on every CST8xx).
  pinMode(TOUCH_INT_PIN, INPUT);
  delay(50);
  int idle = digitalRead(TOUCH_INT_PIN);
  Serial.printf("[power] touch INT idle %s\n", idle ? "high" : "low");
  Serial.flush();
  gpio_wakeup_enable((gpio_num_t)TOUCH_INT_PIN, idle ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
  Serial.println("[power] woken by touch");
  for (int pin : {LED_R_PIN, LED_G_PIN, LED_B_PIN, BACKLIGHT_PIN}) gpio_hold_dis((gpio_num_t)pin);
  esp_restart();
}

// ---- Setup / loop ---------------------------------------------------------------
static uint32_t s_coreLostSince = 0;
static int s_lastActivity = -1;
static bool s_wasTouched = false;
static uint32_t s_gagUntil = 0;

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
  radio::Core c = radio::core();
  radio::Hunter h = radio::hunter();
  if (s_demo) demo(now, c, h);
  Zone z = zoneFor(h);
  if (z != s_zone) Serial.printf("[hunter] %s (%d dBm)\n", (const char*[]){"gone", "detected", "near", "CLOSE"}[z], h.rssi);
  s_zone = z;

  // The costume's activity: the beam and the takeover get the whole screen.
  int act = c.seen ? c.activity : -1;
  if (act != s_lastActivity) {
    if (act == A_CONTAINED) {
      if (s_overlay != O_CONTAIN) showOverlay(O_CONTAIN, 4000);
    } else if (act == A_ESCAPED) {
      showOverlay(O_EVADED, 3000);
    }
    else if (act == A_TAKEOVER) showOverlay(O_TAKEOVER, 4000);
    s_lastActivity = act;
  }
  // The beam, straight from the scanner's beacon (usually a moment before the
  // costume reacts). The AI always gets away: if the costume doesn't say so
  // itself (out of range), the armband does when the strobe ends.
  if (radio::beamFired()) {
    Serial.println("[beam] CONTAIN fired");
    if (s_overlay != O_CONTAIN) showOverlay(O_CONTAIN, 4000);
    s_evadePending = true;
  }
  if (act == A_CONTAINED && s_overlay == O_CONTAIN) s_overlayUntil = max(s_overlayUntil, now + 500);
  if (act == A_ESCAPED) s_evadePending = false;

  // A guest pokes it.
  int32_t tx, ty;
  bool touched = lcd.getTouch(&tx, &ty) > 0;
  if (touched && !s_wasTouched && (int32_t)(now - s_gagUntil) >= 0) {
    Serial.printf("[touch] %d,%d\n", (int)tx, (int)ty);
    showOverlay(O_TOUCH, 2200);
    s_gagUntil = now + TOUCH_GAG_COOLDOWN_MS;
  }
  s_wasTouched = touched;
  // Hold to power off.
  static uint32_t s_holdSince = 0;
  if (!touched) s_holdSince = 0;
  else if (!s_holdSince) s_holdSince = now;
  else if (now - s_holdSince > POWER_HOLD_MS) powerOff();
  if (s_overlay != O_NONE && (int32_t)(now - s_overlayUntil) >= 0) {
    if (s_overlay == O_CONTAIN && s_evadePending) showOverlay(O_EVADED, 3000);
    else s_overlay = O_NONE;
    s_evadePending = false;
  }

  // Dim when the costume's been away a while (saves the battery on the bench).
  if (c.seen) s_coreLostSince = 0;
  else if (!s_coreLostSince) s_coreLostSince = now;
  bool dim = s_coreLostSince && now - s_coreLostSince > CORE_LOST_DIM_S * 1000UL;
  lcd.setBrightness((dim ? BACKLIGHT_IDLE_PERCENT : BACKLIGHT_PERCENT) * 255 / 100);

  if (s_overlay != O_NONE) {
    drawOverlay(now);
  } else {
    g.fillScreen(C_BG);
    drawHeader(now, c);
    drawRadar(now, h);
    drawPanel(now, c, h);
  }
  g.pushSprite(0, 0);
  ledTick(now, c);

  if ((int32_t)(now - nextLog) >= 0) {
    nextLog = now + 5000;
    Serial.printf("[state] core %s %d dBm threat %u act %u | hunter %s %d dBm | heap %u\n", c.seen ? "seen" : "lost",
                  c.rssi, c.threat, c.activity, h.seen ? "seen" : "none", h.rssi, (unsigned)ESP.getFreeHeap());
  }
}
