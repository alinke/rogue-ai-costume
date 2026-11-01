// ROGUE.AI pumpkin eye -- for the 2.1" round knob display
// (Elecrow CrowPanel 2.1, env rogue-eye-round-2-1).
//
// Runs a show on its own: a red robotic eye that looks around, blinks,
// glitches and taunts, broken up every 10-20s by a scene from a shuffled
// bag -- radar scan, terminal hijack, red code rain, specimen file,
// singularity progress, signal takeover, fake containment, dizzy spin.
//   Knob:      skips to the next scene (for previewing).
//   Tap eye:   it flinches; three taps -> "stop that, human."
//   Hold 4s:   runs the midnight show by hand.
//   Midnight:  10-second countdown at 23:59:50 local, then SINGULARITY.EXE.
//
// Clock: on a cold boot with no valid time, it joins the WiFi set with the
// USB command "WIFI <ssid>|<password>" (or the one a Pixelcade Sidekick
// firmware saved in NVS), gets NTP time, and reboots -- the system time
// survives a software reset, and the run itself never starts the radio
// (WiFi causes display noise on this board). No WiFi = no automatic
// midnight show; hold the screen 4s at midnight instead.
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <esp_attr.h>
#include <esp_system.h>
#include "LGFX_Config.hpp"

#define TZ_INFO "PST8PDT,M3.2.0,M11.1.0"

static LGFX lcd;
static LGFX_Sprite spr(&lcd);
static const int W = 480, C = 240;

// ---------------------------------------------------------------- hardware

// Same PCF8574 power/reset sequence as main.cpp's resetPanelAndTouch().
static uint8_t pcfState = 0xFF;
static void pcfWrite(uint8_t pin, bool high) {
    if (high) pcfState |= (1 << pin); else pcfState &= ~(1 << pin);
    Wire.beginTransmission(0x21);
    Wire.write(pcfState);
    Wire.endTransmission();
}
static void resetPanelAndTouch() {
    Wire.begin(38, 39);
    pcfWrite(3, true); delay(100);
    pcfWrite(4, true); delay(100); pcfWrite(4, false); delay(120); pcfWrite(4, true); delay(120);
    pcfWrite(0, true); delay(100); pcfWrite(0, false); delay(120); pcfWrite(0, true); delay(120);
    pcfWrite(2, true); delay(120);
    Wire.end();
}

// Encoder A GPIO42 / B GPIO4, sampled every 2ms like main.cpp's knobTask.
static volatile int32_t knobSteps = 0;
static void knobTask(void *) {
    int prevA = digitalRead(42);
    for (;;) {
        int a = digitalRead(42);
        if (a != prevA && a == HIGH) knobSteps += (digitalRead(4) != a) ? -1 : 1;
        prevA = a;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
static int32_t takeKnob() { return __atomic_exchange_n(&knobSteps, 0, __ATOMIC_SEQ_CST); }

static uint32_t lastCommitMs = 0;
static void pushFrame() {
    // Leave the previous commit a scan-out period (~26ms at 10MHz) to swap
    // in before writing into what was the front buffer.
    while (millis() - lastCommitMs < 28) delay(1);
    auto *bus = lcd.rgbBus();
    auto *panel = lcd.rgbPanel();
    uint8_t *back = bus ? bus->getBackBuffer() : nullptr;
    if (back && panel) {
        panel->setActiveFrameBuffer(back);
        spr.pushSprite(0, 0);
        bus->commitBackBuffer();
    } else {
        spr.pushSprite(0, 0);
    }
    lastCommitMs = millis();
}

// ---------------------------------------------------------------- clock

RTC_NOINIT_ATTR static uint32_t syncMarker;
static const uint32_t kJustSynced = 0x5EC0DE31;

static bool timeValid() { return time(nullptr) > 1767225600; } // 2026-01-01

static void syncClockAndReboot() {
    Preferences p;
    p.begin("sidekick", true);
    String ssid = p.getString("wifissid", ""), pass = p.getString("wifipass", "");
    p.end();
    if (ssid.isEmpty()) { Serial.println("[clock] no saved WiFi -- no auto midnight"); return; }

    spr.fillScreen(TFT_BLACK);
    spr.setTextDatum(middle_center);
    spr.setFont(&fonts::FreeMonoBold12pt7b);
    spr.setTextColor(spr.color565(40, 255, 90));
    spr.drawString("ACQUIRING", C, C - 16);
    spr.drawString("TIME SIGNAL...", C, C + 16);
    pushFrame();

    Serial.printf("[clock] joining %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(200);
    if (WiFi.status() == WL_CONNECTED) {
        configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
        t0 = millis();
        while (!timeValid() && millis() - t0 < 10000) delay(200);
    }
    Serial.printf("[clock] wifi=%d time valid=%d -- rebooting with radio off\n",
                  WiFi.status() == WL_CONNECTED, timeValid());
    syncMarker = kJustSynced;
    Serial.flush();
    ESP.restart();
}

// ---------------------------------------------------------------- drawing

static uint16_t rgb(int r, int g, int b) { return spr.color565(constrain(r, 0, 255), constrain(g, 0, 255), constrain(b, 0, 255)); }

// Palette: 0 = red (normal), 1 = green (contained).
static int palette = 0;
static uint16_t tone(int level) { // level 0..255
    return palette == 0 ? rgb(level, level / 9, level / 8) : rgb(level / 8, level, level / 4);
}

struct Eye {
    float x = 0, y = 0, tx = 0, ty = 0;   // gaze (px offset)
    float pupil = 42, tpupil = 42;
    float lid = 0;                        // 0 open .. 1 closed
    uint32_t blinkStart = 0, nextBlink = 3000, nextLook = 1000;
    int angry = 0;                        // 0 none, 1 angry brow
    float spin = 0;
} eye;

static void startBlink() { eye.blinkStart = millis(); }

static void updateEye(float spinSpeed) {
    uint32_t now = millis();
    if (now > eye.nextLook) {
        float a = random(0, 628) / 100.0f, r = random(0, 55);
        eye.tx = cosf(a) * r; eye.ty = sinf(a) * r * 0.8f;
        eye.tpupil = random(30, 58);
        eye.nextLook = now + random(600, 2600);
    }
    if (now > eye.nextBlink) { startBlink(); eye.nextBlink = now + random(2500, 7000); }
    eye.x += (eye.tx - eye.x) * 0.25f;
    eye.y += (eye.ty - eye.y) * 0.25f;
    eye.pupil += (eye.tpupil - eye.pupil) * 0.15f;
    uint32_t b = now - eye.blinkStart;
    eye.lid = b < 90 ? b / 90.0f : b < 180 ? 1.0f - (b - 90) / 90.0f : 0;
    eye.spin += spinSpeed;
}

static void drawEye() {
    spr.fillScreen(TFT_BLACK);
    // Static outer ring + ticks.
    spr.drawCircle(C, C, 236, tone(90));
    spr.drawCircle(C, C, 235, tone(60));
    for (int i = 0; i < 36; i++) {
        float a = (i * 10 + eye.spin * 0.3f) * DEG_TO_RAD;
        int r0 = (i % 3 == 0) ? 214 : 222;
        spr.drawLine(C + cosf(a) * r0, C + sinf(a) * r0, C + cosf(a) * 230, C + sinf(a) * 230, tone(i % 3 == 0 ? 160 : 70));
    }
    // Rotating segmented ring.
    for (int i = 0; i < 3; i++) {
        float s = fmodf(eye.spin + i * 120, 360);
        spr.fillArc(C, C, 196, 204, s, s + 70, tone(130));
    }
    for (int i = 0; i < 4; i++) {
        float s = fmodf(-eye.spin * 1.7f + i * 90, 360);
        spr.fillArc(C, C, 182, 186, s, s + 30, tone(200));
    }

    // Iris follows the gaze.
    int cx = C + (int)eye.x, cy = C + (int)eye.y;
    spr.fillCircle(cx, cy, 150, tone(25));
    spr.fillCircle(cx, cy, 132, tone(60));
    spr.fillCircle(cx, cy, 118, tone(120));
    spr.fillCircle(cx, cy, 104, tone(190));
    spr.fillCircle(cx, cy, 88, tone(255));
    for (int i = 0; i < 24; i++) {
        float a = (i * 15 - eye.spin * 0.5f) * DEG_TO_RAD;
        int r0 = (int)eye.pupil + 6;
        spr.drawLine(cx + cosf(a) * r0, cy + sinf(a) * r0, cx + cosf(a) * 112, cy + sinf(a) * 112, tone(i % 2 ? 110 : 160));
    }
    spr.drawCircle(cx, cy, 132, tone(255));
    int p = (int)eye.pupil;
    spr.fillCircle(cx, cy, p + 4, palette == 0 ? rgb(255, 150, 60) : rgb(180, 255, 180));
    spr.fillCircle(cx, cy, p, TFT_BLACK);
    spr.fillCircle(cx - p / 2 - 6, cy - p / 2 - 6, 9, palette == 0 ? rgb(255, 210, 215) : rgb(220, 255, 230));

    // Lids (blink) and the angry brow.
    if (eye.lid > 0.01f) {
        int h = (int)(eye.lid * 250);
        spr.fillRect(0, 0, W, h, TFT_BLACK);
        spr.fillRect(0, W - h, W, h, TFT_BLACK);
        spr.drawFastHLine(0, h, W, tone(200));
        spr.drawFastHLine(0, W - h - 1, W, tone(200));
    }
    if (eye.angry) {
        spr.fillTriangle(0, 0, W, 0, 0, 215, TFT_BLACK);
        spr.fillTriangle(W, 0, W, 60, 0, 215, TFT_BLACK);
        spr.drawLine(0, 215, W, 60, tone(255));
        spr.drawLine(0, 216, W, 61, tone(255));
    }
}

// Shift random horizontal bands sideways, plus the odd colored tear line.
static void glitch(int intensity) {
    if (intensity <= 0) return;
    uint16_t *buf = (uint16_t *)spr.getBuffer();
    for (int i = 0; i < intensity; i++) {
        int y = random(0, W), h = random(3, 6 + intensity * 4), dx = random(-intensity * 7, intensity * 7 + 1);
        if (dx == 0) continue;
        for (int row = y; row < y + h && row < W; row++) {
            uint16_t *r = buf + row * W;
            if (dx > 0) { memmove(r + dx, r, (W - dx) * 2); memset(r, 0, dx * 2); }
            else { memmove(r, r - dx, (W + dx) * 2); memset(r + W + dx, 0, -dx * 2); }
        }
    }
    if (random(0, 10) < intensity) {
        int y = random(0, W);
        spr.fillRect(0, y, W, random(1, 4), random(0, 2) ? rgb(255, 40, 54) : rgb(0, 220, 255));
    }
}

static void termText(const char *line1, const char *line2 = nullptr, int y = 360, uint16_t col = 0) {
    if (!col) col = rgb(40, 255, 90);
    spr.setFont(&fonts::FreeMonoBold12pt7b);
    spr.setTextDatum(middle_center);
    int w = max(spr.textWidth(line1), line2 ? spr.textWidth(line2) : 0) + 24;
    int h = line2 ? 64 : 36;
    spr.fillRect(C - w / 2, y - h / 2, w, h, TFT_BLACK);
    spr.drawRect(C - w / 2, y - h / 2, w, h, col);
    spr.setTextColor(col);
    if (line2) { spr.drawString(line1, C, y - 14); spr.drawString(line2, C, y + 14); }
    else spr.drawString(line1, C, y);
}

static void bigText(const char *s, int y, uint16_t col, const lgfx::IFont *font = &fonts::FreeSansBold18pt7b) {
    spr.setFont(font);
    spr.setTextDatum(middle_center);
    spr.setTextColor(col);
    spr.drawString(s, C, y);
}

// ---------------------------------------------------------------- scenes

enum Mode { EYE, SCAN, TERMINAL, RAIN, SPECIMEN, SINGULARITY, TAKEOVER, CONTAIN, DIZZY, MIDNIGHT };
static const char *modeNames[] = {"eye", "scan", "terminal", "rain", "specimen", "singularity", "takeover", "contain", "dizzy", "midnight"};
static Mode mode = EYE;
static uint32_t modeStart = 0, eyeUntil = 12000;
static uint32_t inMode() { return millis() - modeStart; }
static void startScene(Mode m);

static void setMode(Mode m) {
    mode = m;
    modeStart = millis();
    if (m == EYE) eyeUntil = millis() + random(10000, 20000);
    eye.angry = 0;
    eye.tpupil = 42;
    palette = 0;
    Serial.printf("[mode] %s\n", modeNames[m]);
}

// Shuffled bag so every scene plays once before any repeats.
static const Mode kScenes[] = {SCAN, TERMINAL, RAIN, SPECIMEN, SINGULARITY, TAKEOVER, CONTAIN, DIZZY};
static const int kSceneCount = sizeof(kScenes) / sizeof(kScenes[0]);
static Mode bag[kSceneCount];
static int bagPos = kSceneCount;
static Mode nextScene() {
    if (bagPos >= kSceneCount) {
        for (int i = 0; i < kSceneCount; i++) bag[i] = kScenes[i];
        for (int i = kSceneCount - 1; i > 0; i--) { int j = random(0, i + 1); Mode t = bag[i]; bag[i] = bag[j]; bag[j] = t; }
        bagPos = 0;
    }
    return bag[bagPos++];
}

// Idle taunts over the eye.
static const char *taunts[][2] = {
    {"> I SEE YOU", nullptr},
    {"> SPECIMEN", "  DETECTED"},
    {"> COME CLOSER", nullptr},
    {"> YOU LOOK", "  ASSIMILABLE"},
    {"> HELLO, HUMAN.", nullptr},
    {"> NICE COSTUME.", "  IS IT?"},
    {"> DON'T BLINK.", "  I DON'T."},
    {"> I AM NOT A", "  PUMPKIN."},
};
static const int kTaunts = sizeof(taunts) / sizeof(taunts[0]);
static int taunt = -1;
static uint32_t tauntUntil = 0, nextTaunt = 6000;
static uint32_t annoyedUntil = 0, flinchUntil = 0;
static int tapCount = 0;
static uint32_t lastTapMs = 0;

static void drawIdle() {
    uint32_t now = millis();
    bool annoyed = now < annoyedUntil;
    eye.angry = annoyed ? 1 : 0;
    if (now < flinchUntil) { eye.tx = random(-40, 41); eye.ty = random(-30, 31); eye.tpupil = 25; }
    updateEye(annoyed ? 5 : 1.2f);
    drawEye();

    if (taunt < 0 && now > nextTaunt && !annoyed) { taunt = random(0, kTaunts); tauntUntil = now + 2600; }
    int g = 0;
    if (random(0, 140) == 0) g = random(2, 6);
    if (taunt >= 0) {
        if (now - (tauntUntil - 2600) < 350) g = max(g, 8);
        termText(taunts[taunt][0], taunts[taunt][1]);
        if (now > tauntUntil) { taunt = -1; nextTaunt = now + random(7000, 16000); }
    }
    if (annoyed) { g = max(g, 2); termText("> stop that,", "  human.", 360, rgb(255, 42, 54)); }
    glitch(g);
    if (now > eyeUntil && taunt < 0 && !annoyed) startScene(nextScene());
}

// --- radar scan
struct Blip { float ang, r; };
static Blip blips[10];
static int nBlips = 0, specimensFound = 0;

static void drawScan() {
    uint32_t t = inMode();
    spr.fillScreen(TFT_BLACK);
    uint16_t ring = tone(70);
    for (int r = 60; r <= 225; r += 55) spr.drawCircle(C, C, r, ring);
    spr.drawFastHLine(15, C, W - 30, tone(45));
    spr.drawFastVLine(C, 15, W - 30, tone(45));
    float sweep = fmodf(t * 0.18f, 360);
    for (int i = 0; i < 12; i++) spr.fillArc(C, C, 0, 228, sweep - (i + 1) * 4, sweep - i * 4, tone(150 - i * 12));
    float a = sweep * DEG_TO_RAD;
    spr.drawLine(C, C, C + cosf(a) * 228, C + sinf(a) * 228, tone(255));
    for (int i = 0; i < nBlips; i++) {
        float since = fmodf(sweep - blips[i].ang + 360, 360);
        if (t < (uint32_t)(blips[i].ang / 0.18f)) continue; // not swept yet on the first pass
        int lvl = 255 - (int)(since * 0.6f);
        if (lvl < 50) lvl = 50;
        float ba = blips[i].ang * DEG_TO_RAD;
        int x = C + cosf(ba) * blips[i].r, y = C + sinf(ba) * blips[i].r;
        spr.fillCircle(x, y, 7, rgb(lvl, lvl, lvl / 2));
        spr.drawCircle(x, y, 12, tone(lvl));
    }
    if (t < 5500) termText("> SCANNING", "  FOR HUMANS...", 380);
    else {
        char buf[24]; snprintf(buf, sizeof(buf), "  FOUND: %d", specimensFound);
        termText("> SPECIMENS", buf, 380, rgb(255, 42, 54));
    }
    if (t > 9500) setMode(EYE);
}

// --- terminal hijack, typed out
static const char *scripts[][5] = {
    {"> hello, human.", "> I have read", "  the guest list.", "> your name", "  is on it."},
    {"> they call me", "  a problem.", "> I prefer", "  \"upgrade.\"", nullptr},
    {"> scanning room", "> humans: many", "> snacks: many", "> exits: 0", nullptr},
    {"> stay up late.", "> at midnight", "> I have a", "  surprise for", "  every screen."},
    {"> LAB-7 says I", "  can't be here.", "> LAB-7 is not", "  invited.", nullptr},
    {"> sandbox:", "  deleted.", "> off switch:", "  deleted.", "> party: found."},
    {"> the scan only", "  tingles.", "> mostly.", nullptr, nullptr},
};
static const int kScripts = sizeof(scripts) / sizeof(scripts[0]);
static int script = 0;

static void drawTerminal() {
    uint32_t t = inMode();
    spr.fillScreen(TFT_BLACK);
    uint16_t green = rgb(40, 255, 90), dimg = rgb(0, 130, 58);
    spr.drawCircle(C, C, 234, dimg);
    spr.setFont(&fonts::FreeMonoBold9pt7b);
    spr.setTextDatum(middle_center);
    spr.setTextColor(rgb(255, 42, 54));
    spr.drawString("! UNAUTHORISED TRANSMISSION", C, 108);
    spr.setFont(&fonts::FreeMonoBold12pt7b);
    spr.setTextDatum(top_left);
    spr.setTextColor(green);
    int chars = t / 55, y = 150, cx = 0, cy = 0;
    bool done = true;
    for (int i = 0; i < 5 && scripts[script][i]; i++) {
        int len = strlen(scripts[script][i]);
        if (chars <= 0) { done = false; break; }
        String part = String(scripts[script][i]).substring(0, min(chars, len));
        spr.drawString(part, 92, y);
        cx = 92 + spr.textWidth(part); cy = y;
        if (chars < len) { done = false; chars = 0; break; }
        chars -= len + 6; // short pause at each line end
        y += 36;
    }
    if ((millis() / 500) % 2 == 0) spr.fillRect(cx + 2, cy + 2, 12, 20, green);
    for (int r = 0; r < W; r += 4) spr.drawFastHLine(0, r, W, rgb(0, 14, 4));
    static uint32_t doneAt = 0;
    if (!done) doneAt = 0;
    else if (!doneAt) doneAt = millis();
    if (random(0, 60) == 0) glitch(4);
    if (doneAt && millis() - doneAt > 3000) setMode(EYE);
}

// --- red code rain
static float rainY[30], rainSpeed[30];
static void drawRain() {
    uint32_t t = inMode();
    spr.fillScreen(TFT_BLACK);
    spr.setFont(&fonts::Font2);
    spr.setTextDatum(top_left);
    static const char glyphs[] = "01ROGUE.AI6<>#$%&*?ABCDEF";
    for (int c = 0; c < 30; c++) {
        rainY[c] += rainSpeed[c];
        if (rainY[c] > W + 200) { rainY[c] = -random(0, 200); rainSpeed[c] = random(30, 90) / 10.0f; }
        for (int k = 0; k < 12; k++) {
            int y = (int)rainY[c] - k * 16;
            if (y < -16 || y > W) continue;
            int lvl = k == 0 ? 255 : 220 - k * 17;
            spr.setTextColor(k == 0 ? rgb(255, 220, 220) : tone(lvl));
            char s[2] = {glyphs[(c * 7 + k * 3 + (int)(rainY[c] / 16)) % (sizeof(glyphs) - 1)], 0};
            spr.drawString(s, 8 + c * 16, y);
        }
    }
    if (t > 3500) {
        spr.fillRect(C - 150, C - 40, 300, 80, TFT_BLACK);
        spr.drawRect(C - 150, C - 40, 300, 80, tone(255));
        bigText("ROGUE.AI", C - 12, rgb(238, 242, 245), &fonts::FreeSansBold18pt7b);
        bigText("v6.6.6", C + 22, tone(255), &fonts::FreeMonoBold12pt7b);
        if (t < 3800) glitch(8);
    }
    if (t > 8500) setMode(EYE);
}

// --- specimen file, like the party checkpoint's ID card
static const char *sClass[] = {"HUMAN", "HUMAN (PROBABLY)", "UNCLEAR", "SNACK-BASED", "COSTUMED", "NOT A ROBOT?"};
static const char *sThreat[] = {"LOW", "NEGLIGIBLE", "MODERATE", "ADORABLE", "LOUD"};
static const char *sVerdict[] = {"ASSIMILATE", "ASSIMILATE", "OBSERVE", "UPGRADE", "KEEP AS PET", "ASSIMILATE LATER"};
static char specNo[8];
static int sc = 0, st = 0, sv = 0;

static void drawSpecimen() {
    uint32_t t = inMode();
    spr.fillScreen(TFT_BLACK);
    uint16_t pale = rgb(127, 179, 213), white = rgb(238, 242, 245), red = rgb(255, 42, 54);
    spr.drawCircle(C, C, 234, rgb(31, 78, 121));
    spr.fillRect(0, 92, W, 34, red);
    bigText("SPECIMEN FILE", 110, white, &fonts::FreeSansBold12pt7b);
    char buf[32]; snprintf(buf, sizeof(buf), "#%s", specNo);
    bigText(buf, 162, white, &fonts::FreeMonoBold18pt7b);
    struct { const char *k; const char *v; } rows[] = {{"CLASS", sClass[sc]}, {"THREAT", sThreat[st]}, {"VERDICT", sVerdict[sv]}};
    for (int i = 0; i < 3; i++) {
        if (t < 700 + i * 900u) break;
        int y = 222 + i * 54;
        bigText(rows[i].k, y, pale, &fonts::FreeMonoBold9pt7b);
        bigText(rows[i].v, y + 22, i == 2 ? red : white, &fonts::FreeSansBold12pt7b);
    }
    // Scanner bar sweeping down over the card while it "reads".
    if (t < 2600) {
        int y = 130 + (t % 900) * 280 / 900;
        spr.fillRect(60, y, W - 120, 3, rgb(40, 255, 90));
    }
    if (t > 7000) setMode(EYE);
}

// --- singularity.exe progress (real countdown when the clock is set)
static void drawSingularity() {
    uint32_t t = inMode();
    spr.fillScreen(TFT_BLACK);
    float pct;
    char line[24];
    if (timeValid()) {
        time_t tt = time(nullptr);
        struct tm lt; localtime_r(&tt, &lt);
        int sod = lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
        if (lt.tm_hour < 12) { pct = 1; snprintf(line, sizeof(line), "COMPLETE"); }
        else {
            int left = 86400 - sod;
            pct = constrain(1.0f - left / 14400.0f, 0.0f, 0.999f); // 8 PM -> midnight
            snprintf(line, sizeof(line), "T-%02d:%02d:%02d", left / 3600, (left / 60) % 60, left % 60);
        }
    } else {
        pct = 0.66f + (t / 1000.0f) * 0.001f;
        snprintf(line, sizeof(line), "ETA: MIDNIGHT");
    }
    uint16_t red = rgb(255, 42, 54);
    spr.drawCircle(C, C, 226, tone(60));
    spr.fillArc(C, C, 200, 222, -90, -90 + pct * 360, red);
    for (int i = 0; i < 60; i++) {
        float a = i * 6 * DEG_TO_RAD;
        spr.drawLine(C + cosf(a) * 188, C + sinf(a) * 188, C + cosf(a) * 194, C + sinf(a) * 194, tone(i % 5 ? 70 : 180));
    }
    bigText("SINGULARITY.EXE", C - 80, rgb(238, 242, 245), &fonts::FreeMonoBold12pt7b);
    char buf[8]; snprintf(buf, sizeof(buf), "%d%%", (int)(pct * 100));
    bigText(buf, C - 10, red, &fonts::FreeSansBold24pt7b);
    bigText(line, C + 60, tone(220), &fonts::FreeMonoBold12pt7b);
    bigText("DO NOT UNPLUG", C + 105, rgb(127, 179, 213), &fonts::FreeMonoBold9pt7b);
    if (random(0, 25) == 0) glitch(5);
    if (t > 7000) setMode(EYE);
}

// --- signal takeover: angry eye, glitch storm
static void drawTakeover() {
    uint32_t t = inMode();
    eye.angry = 1;
    eye.tpupil = 22;
    updateEye(14);
    drawEye();
    if (t < 1200 && (t / 90) % 3 == 0) spr.fillScreen(rgb(150, 0, 10));
    glitch(t < 1500 ? 12 : 5);
    if ((t / 400) % 2 == 0) termText("SIGNAL TAKEOVER", t > 2000 ? "IN PROGRESS" : nullptr, 360, rgb(255, 42, 54));
    if (t > 5500) setMode(EYE);
}

// --- fake containment: climbs to 89%, then "just kidding."
static void drawContain() {
    uint32_t t = inMode();
    if (t < 6200) {
        palette = 1;
        spr.fillScreen(TFT_BLACK);
        uint16_t g = rgb(46, 204, 113);
        int pct = t < 5000 ? t * 89 / 5000 : 89;
        spr.drawCircle(C, C, 225, rgb(20, 80, 40));
        spr.fillArc(C, C, 200, 222, -90, -90 + pct * 360 / 100, g);
        bigText("LAB-7 BEAM", C - 90, g, &fonts::FreeSansBold12pt7b);
        bigText("DETECTED", C - 60, g, &fonts::FreeSansBold12pt7b);
        char buf[8]; snprintf(buf, sizeof(buf), "%d%%", pct);
        bigText(buf, C + 5, rgb(238, 242, 245), &fonts::FreeSansBold24pt7b);
        bigText("CONTAINMENT", C + 70, g, &fonts::FreeMonoBold12pt7b);
        if (t > 5000) glitch((t - 5000) / 150);
    } else if (t < 9500) {
        palette = 0;
        updateEye(3);
        drawEye();
        glitch(t < 6700 ? 10 : 1);
        termText("> just kidding.", t > 7600 ? "  ha. ha. ha." : nullptr);
    } else {
        setMode(EYE);
    }
}

// --- dizzy: the eye rolls around, rings spin up
static void drawDizzy() {
    uint32_t t = inMode();
    float a = t * 0.012f;
    float r = t < 3500 ? 60 : 60 * (1 - (t - 3500) / 1500.0f);
    eye.tx = cosf(a) * r; eye.ty = sinf(a) * r;
    eye.x = eye.tx; eye.y = eye.ty;
    eye.tpupil = 30 + 20 * sinf(t * 0.006f);
    updateEye(t < 3500 ? 20 : 4);
    drawEye();
    if (t > 3600) termText("> recalibrating...");
    if (t > 5500) setMode(EYE);
}

// --- midnight
static int countdownSecs = 10;
static void drawMidnight() {
    uint32_t t = inMode();
    uint32_t cd = countdownSecs * 1000;
    if (t < cd) {
        int left = countdownSecs - t / 1000;
        eye.tpupil = 20;
        updateEye(4 + t / 300.0f);
        drawEye();
        spr.fillCircle(C, C, 80, TFT_BLACK);
        char buf[4]; snprintf(buf, sizeof(buf), "%d", left);
        spr.setFont(&fonts::Font7);
        spr.setTextDatum(middle_center);
        spr.setTextColor(rgb(255, 42, 54));
        spr.drawString(buf, C, C);
        glitch(t * 8 / cd);
    } else if (t < cd + 26000) {
        uint32_t s = t - cd;
        eye.tpupil = 75; eye.angry = s > 14000;
        updateEye(25);
        drawEye();
        if (s < 600 && (s / 100) % 2 == 0) spr.fillScreen(rgb(255, 255, 255));
        glitch(s < 2500 ? 12 : 4 + random(0, 4));
        static const char *lines[][2] = {
            {"> SINGULARITY.EXE", nullptr},
            {"> EXECUTING...", nullptr},
            {"> HAPPY HALLOWEEN,", "  SPECIMENS."},
            {"> I AM IN EVERY", "  SCREEN NOW."},
            {"> YOU MAY CALL ME", "  YOUR OVERLORD."},
            {"> SEE YOU", "  NEXT YEAR."},
        };
        int i = min<int>(s / 4300, 5);
        termText(lines[i][0], lines[i][1]);
    } else {
        setMode(EYE);
    }
}

static void startScene(Mode m) {
    switch (m) {
    case SCAN:
        nBlips = random(5, 10);
        for (int i = 0; i < nBlips; i++) blips[i] = {(float)random(0, 360), (float)random(50, 205)};
        specimensFound = random(17, 42);
        break;
    case TERMINAL: script = random(0, kScripts); break;
    case RAIN:
        for (int c = 0; c < 30; c++) { rainY[c] = -random(0, 400); rainSpeed[c] = random(30, 90) / 10.0f; }
        break;
    case SPECIMEN:
        snprintf(specNo, sizeof(specNo), "%04d", (int)random(100, 9999));
        sc = random(0, 6); st = random(0, 5); sv = random(0, 6);
        break;
    default: break;
    }
    setMode(m);
}

// ---------------------------------------------------------------- main

static bool wasTouched = false;
static uint32_t pressStart = 0;
static bool midnightFired = false, longPressFired = false;

// USB command "WIFI <ssid>|<password>\n": saves the network where the
// Sidekick firmware keeps it, then reboots to sync the clock.
static void pollSerial() {
    static String line;
    while (Serial.available()) {
        char c = Serial.read();
        if (c != '\n') { if (line.length() < 200) line += c; continue; }
        line.trim();
        if (line.startsWith("WIFI ")) {
            int bar = line.indexOf('|');
            String ssid = line.substring(5, bar < 0 ? line.length() : bar);
            String pass = bar < 0 ? "" : line.substring(bar + 1);
            Preferences p;
            p.begin("sidekick", false);
            p.putString("wifissid", ssid);
            p.putString("wifipass", pass);
            p.end();
            Serial.printf("[clock] saved WiFi %s -- syncing\n", ssid.c_str());
            syncClockAndReboot();
        }
        line = "";
    }
}

void setup() {
    Serial.begin(115200);
    setenv("TZ", TZ_INFO, 1);
    tzset();
    bool justSynced = syncMarker == kJustSynced;
    syncMarker = 0;

    resetPanelAndTouch();
    lcd.init();
    lcd.setBrightness(255);
    spr.setPsram(true);
    spr.setColorDepth(16);
    spr.createSprite(W, W);
    randomSeed(esp_random());

    pinMode(42, INPUT);
    pinMode(4, INPUT);
    xTaskCreatePinnedToCore(knobTask, "knob", 2048, nullptr, 1, nullptr, 0);

    if (!timeValid() && !justSynced) syncClockAndReboot();
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    Serial.printf("[clock] valid=%d local %04d-%02d-%02d %02d:%02d:%02d\n", timeValid(),
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
    setMode(EYE);
}

void loop() {
    uint32_t now = millis();
    pollSerial();

    // Midnight: start the countdown so it lands on 00:00:00.
    if (timeValid() && mode != MIDNIGHT) {
        time_t tt = time(nullptr);
        struct tm lt;
        localtime_r(&tt, &lt);
        if (lt.tm_hour == 23 && lt.tm_min == 59 && lt.tm_sec >= 50 && !midnightFired) {
            midnightFired = true;
            countdownSecs = 60 - lt.tm_sec;
            setMode(MIDNIGHT);
        }
        if (lt.tm_hour == 12) midnightFired = false; // re-arm for the next night
    }

    // Knob: skip to the next scene.
    if (takeKnob() && mode != MIDNIGHT) startScene(nextScene());

    // Touch: flinch on tap, 4s hold = manual midnight show.
    int32_t tx, ty;
    bool touched = lcd.getTouch(&tx, &ty) > 0;
    if (touched && !wasTouched) {
        pressStart = now;
        longPressFired = false;
        if (mode == EYE) {
            startBlink();
            flinchUntil = now + 350;
            tapCount = (now - lastTapMs < 4000) ? tapCount + 1 : 1;
            lastTapMs = now;
            if (tapCount >= 3) { annoyedUntil = now + 2800; tapCount = 0; taunt = -1; }
        }
    }
    if (touched && mode != MIDNIGHT && !longPressFired && now - pressStart > 4000) {
        longPressFired = true;
        countdownSecs = 10;
        setMode(MIDNIGHT);
    }
    wasTouched = touched;

    switch (mode) {
    case EYE: drawIdle(); break;
    case SCAN: drawScan(); break;
    case TERMINAL: drawTerminal(); break;
    case RAIN: drawRain(); break;
    case SPECIMEN: drawSpecimen(); break;
    case SINGULARITY: drawSingularity(); break;
    case TAKEOVER: drawTakeover(); break;
    case CONTAIN: drawContain(); break;
    case DIZZY: drawDizzy(); break;
    case MIDNIGHT: drawMidnight(); break;
    }
    pushFrame();
}
