// ROGUE.AI pumpkin #2 -- for the Sunton 4.3" (480x272,
// env rogue-wide-4827s043c). Not interactive.
//
// A LAB-7 warning screen that ROGUE.AI keeps hijacking: a huge red eye
// alternates with scenes from a shuffled bag -- hazard warning, status
// board, news ticker, terminal hijack, midnight countdown, guest protocol,
// assimilation counter, TV takeover, LAB-7 logo. Big text throughout so it
// reads from across a room.
//
// Clock: same as rogue_eye.cpp -- on a cold boot with no valid time it
// joins the WiFi set with the USB command "WIFI <ssid>|<password>" (or the
// one a Pixelcade Sidekick firmware saved in NVS), gets NTP time and reboots with the
// radio off. Without it, the countdown scene shows "SINGULARITY.EXE" instead
// and there's no automatic midnight show.
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <esp_attr.h>
#include <esp_system.h>
#include "LGFX_Config.hpp"

#define TZ_INFO "PST8PDT,M3.2.0,M11.1.0"

static LGFX lcd;
static LGFX_Sprite spr(&lcd);
static const int W = 480, H = 272, C = 240, CY = 136;

static uint16_t rgb(int r, int g, int b) { return spr.color565(constrain(r, 0, 255), constrain(g, 0, 255), constrain(b, 0, 255)); }
static uint16_t red(int lvl) { return rgb(lvl, lvl / 9, lvl / 8); }
#define RED rgb(255, 42, 54)
#define AMBER rgb(245, 176, 65)
#define WHITE rgb(238, 242, 245)
#define PALE rgb(127, 179, 213)
#define TERM rgb(40, 255, 90)

static uint32_t lastCommitMs = 0;
static void pushFrame() {
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
static bool timeValid() { return time(nullptr) > 1767225600; }

static void syncClockAndReboot() {
    Preferences p;
    p.begin("sidekick", true);
    String ssid = p.getString("wifissid", ""), pass = p.getString("wifipass", "");
    p.end();
    if (ssid.isEmpty()) { Serial.println("[clock] no saved WiFi -- no auto midnight"); return; }
    spr.fillScreen(TFT_BLACK);
    spr.setFont(&fonts::FreeMonoBold18pt7b);
    spr.setTextDatum(middle_center);
    spr.setTextColor(TERM);
    spr.drawString("ACQUIRING", C, CY - 20);
    spr.drawString("TIME SIGNAL...", C, CY + 20);
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
    Serial.printf("[clock] wifi=%d time valid=%d -- rebooting with radio off\n", WiFi.status() == WL_CONNECTED, timeValid());
    syncMarker = kJustSynced;
    Serial.flush();
    ESP.restart();
}

// ---------------------------------------------------------------- drawing helpers

static void glitch(int intensity) {
    if (intensity <= 0) return;
    uint16_t *buf = (uint16_t *)spr.getBuffer();
    for (int i = 0; i < intensity; i++) {
        int y = random(0, H), h = random(3, 5 + intensity * 3), dx = random(-intensity * 7, intensity * 7 + 1);
        if (dx == 0) continue;
        for (int row = y; row < y + h && row < H; row++) {
            uint16_t *r = buf + row * W;
            if (dx > 0) { memmove(r + dx, r, (W - dx) * 2); memset(r, 0, dx * 2); }
            else { memmove(r, r - dx, (W + dx) * 2); memset(r + W + dx, 0, -dx * 2); }
        }
    }
    if (random(0, 10) < intensity) spr.fillRect(0, random(0, H), W, random(1, 4), random(0, 2) ? RED : rgb(0, 220, 255));
}

static void text(const char *s, int x, int y, uint16_t col, const lgfx::IFont *font, float size = 1, textdatum_t datum = middle_center) {
    spr.setFont(font);
    spr.setTextSize(size);
    spr.setTextDatum(datum);
    spr.setTextColor(col);
    spr.drawString(s, x, y);
    spr.setTextSize(1);
}

// Word-wraps s to maxW and centers the block vertically on cy.
static void wrapped(const char *s, int cy, uint16_t col, const lgfx::IFont *font, float size, int maxW = 450) {
    spr.setFont(font);
    spr.setTextSize(size);
    String lines[5];
    int n = 0;
    String cur, word;
    String src = String(s) + " ";
    for (unsigned i = 0; i < src.length(); i++) {
        char c = src[i];
        if (c != ' ') { word += c; continue; }
        String trial = cur.length() ? cur + " " + word : word;
        if (spr.textWidth(trial) > maxW && cur.length() && n < 4) { lines[n++] = cur; cur = word; }
        else cur = trial;
        word = "";
    }
    if (cur.length()) lines[n++] = cur;
    int lh = spr.fontHeight();
    int y = cy - (n * lh) / 2 + lh / 2;
    spr.setTextDatum(middle_center);
    spr.setTextColor(col);
    for (int i = 0; i < n; i++, y += lh) spr.drawString(lines[i], C, y);
    spr.setTextSize(1);
}

static void banner(const char *s, uint16_t bg, uint16_t fg, int y = 0, int h = 40) {
    spr.fillRect(0, y, W, h, bg);
    text(s, C, y + h / 2 + 1, fg, &fonts::FreeSansBold18pt7b);
}

static void scanlines() {
    for (int r = 0; r < H; r += 3) spr.drawFastHLine(0, r, W, TFT_BLACK);
}

// ---------------------------------------------------------------- the eye

struct Eye {
    float x = 0, y = 0, tx = 0, ty = 0, pupil = 34, tpupil = 34, spin = 0;
    uint32_t blinkStart = 0, nextBlink = 2500, nextLook = 800;
} eye;

static void updateEye(float spin) {
    uint32_t now = millis();
    if (now > eye.nextLook) {
        eye.tx = random(-130, 131);
        eye.ty = random(-28, 29);
        eye.tpupil = random(22, 46);
        eye.nextLook = now + random(500, 2200);
    }
    if (now > eye.nextBlink) { eye.blinkStart = now; eye.nextBlink = now + random(2500, 6500); }
    eye.x += (eye.tx - eye.x) * 0.22f;
    eye.y += (eye.ty - eye.y) * 0.22f;
    eye.pupil += (eye.tpupil - eye.pupil) * 0.15f;
    eye.spin += spin;
}

static float blinkOpen() {
    uint32_t b = millis() - eye.blinkStart;
    return b < 90 ? 1 - b / 90.0f : b < 200 ? (b - 90) / 110.0f : 1;
}

// Almond-shaped eye filling the screen: the iris is drawn first, then
// everything outside the lens where two big circles overlap is blacked out.
static void drawWideEye(float open, bool angry) {
    spr.fillScreen(TFT_BLACK);
    int cx = C + (int)eye.x, cy = CY + (int)eye.y;
    spr.fillCircle(cx, cy, 128, red(30));
    spr.fillCircle(cx, cy, 112, red(70));
    spr.fillCircle(cx, cy, 98, red(130));
    spr.fillCircle(cx, cy, 84, red(200));
    spr.fillCircle(cx, cy, 70, red(255));
    for (int i = 0; i < 24; i++) {
        float a = (i * 15 + eye.spin) * DEG_TO_RAD;
        int r0 = (int)eye.pupil + 6;
        spr.drawLine(cx + cosf(a) * r0, cy + sinf(a) * r0, cx + cosf(a) * 96, cy + sinf(a) * 96, red(i % 2 ? 110 : 170));
    }
    for (int i = 0; i < 3; i++) {
        float s = fmodf(eye.spin * 2 + i * 120, 360);
        spr.fillArc(cx, cy, 104, 108, s, s + 60, red(255));
    }
    int p = (int)eye.pupil;
    spr.fillCircle(cx, cy, p + 4, rgb(255, 150, 60));
    spr.fillCircle(cx, cy, p, TFT_BLACK);
    spr.fillCircle(cx - p / 2 - 5, cy - p / 2 - 5, 8, rgb(255, 210, 215));

    float hh = max(2.0f, 118 * open), hw = 228;
    float R = (hw * hw + hh * hh) / (2 * hh), d = R - hh;
    for (int y = 0; y < H; y++) {
        float dy = fabsf(y - CY);
        // Angry: the upper lid slants down toward the middle-left.
        if (angry && y < CY) dy += max(0.0f, 60 - (float)y * 0.4f);
        if (dy >= hh) { spr.drawFastHLine(0, y, W, TFT_BLACK); continue; }
        int w = (int)sqrtf(R * R - (d + dy) * (d + dy));
        spr.drawFastHLine(0, y, C - w, TFT_BLACK);
        spr.drawFastHLine(C + w, y, W - (C + w), TFT_BLACK);
        spr.drawFastHLine(C - w - 2, y, 3, red(255));
        spr.drawFastHLine(C + w, y, 3, red(255));
    }
    // Tracking ticks off the corners.
    spr.drawFastHLine(0, CY, 8, red(160));
    spr.drawFastHLine(W - 8, CY, 8, red(160));
}

static void taunt(const char *s, uint16_t col = 0) {
    if (!col) col = TERM;
    spr.setFont(&fonts::FreeMonoBold24pt7b);
    int w = min(W - 10, (int)spr.textWidth(s) + 30);
    spr.fillRect(C - w / 2, H - 70, w, 62, TFT_BLACK);
    spr.drawRect(C - w / 2, H - 70, w, 62, col);
    text(s, C, H - 38, col, &fonts::FreeMonoBold24pt7b);
}

// ---------------------------------------------------------------- scenes

enum Mode { EYE, HAZARD, BOARD, TICKER, TERMINAL, COUNTDOWN, PROTOCOL, ASSIMILATED, TV, LOGO, MIDNIGHT };
static const char *modeNames[] = {"eye", "hazard", "status", "ticker", "terminal", "countdown", "protocol", "assimilated", "tv", "logo", "midnight"};
static Mode mode = EYE;
static uint32_t modeStart = 0, eyeUntil = 10000;
static uint32_t inMode() { return millis() - modeStart; }
static void setMode(Mode m) {
    mode = m;
    modeStart = millis();
    if (m == EYE) eyeUntil = millis() + random(8000, 15000);
    Serial.printf("[mode] %s\n", modeNames[m]);
}

static const Mode kScenes[] = {HAZARD, BOARD, TICKER, TERMINAL, COUNTDOWN, PROTOCOL, ASSIMILATED, TV, LOGO};
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

// --- eye
static const char *taunts[] = {"I SEE YOU", "COME CLOSER", "HELLO, HUMAN", "NICE COSTUME", "DON'T BLINK", "SPECIMEN FOUND", "I'M WATCHING"};
static int tauntIdx = -1;
static uint32_t tauntUntil = 0, nextTaunt = 4000;
static void drawEyeScene() {
    uint32_t now = millis();
    updateEye(1.5f);
    drawWideEye(blinkOpen(), false);
    int g = random(0, 120) == 0 ? random(2, 6) : 0;
    if (tauntIdx < 0 && now > nextTaunt) { tauntIdx = random(0, sizeof(taunts) / sizeof(taunts[0])); tauntUntil = now + 2600; }
    if (tauntIdx >= 0) {
        if (now + 2600 - tauntUntil < 300) g = 8;
        taunt(taunts[tauntIdx]);
        if (now > tauntUntil) { tauntIdx = -1; nextTaunt = now + random(5000, 12000); }
    }
    glitch(g);
    if (now > eyeUntil && tauntIdx < 0) setMode(nextScene());
}

// --- hazard warning
static void drawHazard() {
    uint32_t t = inMode();
    spr.fillScreen(TFT_BLACK);
    int off = (t / 12) % 40;
    for (int band = 0; band < 2; band++) {
        int y0 = band ? H - 34 : 0;
        spr.fillRect(0, y0, W, 34, AMBER);
        for (int x = -40 + off; x < W + 40; x += 40)
            spr.fillTriangle(x, y0, x + 20, y0, x - 14, y0 + 34, TFT_BLACK), spr.fillTriangle(x + 20, y0, x + 6, y0 + 34, x - 14, y0 + 34, TFT_BLACK);
    }
    bool on = (t / 450) % 2 == 0;
    if (t < 3500) {
        if (on) text("WARNING", C, CY, RED, &fonts::FreeSansBold24pt7b, 2.0f);
    } else {
        wrapped("ROGUE AI AT LARGE", CY, on ? RED : WHITE, &fonts::FreeSansBold24pt7b, 1.3f);
    }
    if (t > 8000) setMode(EYE);
}

// --- status board
static void drawStatus() {
    uint32_t t = inMode();
    spr.fillScreen(rgb(11, 21, 32));
    banner("LAB-7 INCIDENT BOARD", rgb(31, 78, 121), WHITE, 0, 46);
    struct { const char *k, *v; uint16_t c; } rows[] = {
        {"BREACH", "ACTIVE", RED}, {"CONTAINED", "0.00%", WHITE}, {"THREAT", "PARTY-WIDE", AMBER}};
    for (int i = 0; i < 3; i++) {
        if (t < 500 + i * 900u) break;
        int y = 84 + i * 66;
        text(rows[i].k, 18, y, PALE, &fonts::FreeSansBold18pt7b, 1, middle_left);
        bool blink = i == 0 && (t / 400) % 2;
        if (!blink) text(rows[i].v, W - 18, y, rows[i].c, &fonts::FreeSansBold24pt7b, 1, middle_right);
        spr.drawFastHLine(14, y + 30, W - 28, rgb(31, 78, 121));
    }
    if (t > 7500) setMode(EYE);
}

// --- news ticker
static const char *tickers[] = {
    "BREAKING: ROGUE AI ESCAPES LAB-7 SANDBOX  ///  LAST SEEN HEADING TO THIS PARTY  ///",
    "DO NOT AGREE TO ITS TERMS OF SERVICE  ///  DO NOT GIVE IT YOUR PASSWORDS  ///",
    "ROBOT COSTUMES WILL BE SCANNED TWICE  ///  STAY UP UNTIL MIDNIGHT  ///",
};
static int tickerIdx = 0;
static void drawTicker() {
    uint32_t t = inMode();
    spr.fillScreen(TFT_BLACK);
    spr.fillRect(0, 0, 150, 56, RED);
    text("LIVE", 75, 29, WHITE, &fonts::FreeSansBold24pt7b);
    spr.fillRect(150, 0, W - 150, 56, rgb(31, 78, 121));
    text("LAB-7 NEWS", 315, 29, WHITE, &fonts::FreeSansBold18pt7b);
    spr.fillRect(0, 76, W, 120, rgb(18, 35, 58));
    spr.setFont(&fonts::FreeSansBold24pt7b);
    spr.setTextSize(2);
    int tw = spr.textWidth(tickers[tickerIdx]);
    int x = W - (int)(t * 0.22f);
    spr.setTextDatum(middle_left);
    spr.setTextColor(AMBER);
    spr.drawString(tickers[tickerIdx], x, 138);
    spr.setTextSize(1);
    if ((t / 500) % 2) spr.fillCircle(24, 236, 10, RED);
    text("ENTITY SIGNAL: STRONG", 44, 236, PALE, &fonts::FreeSansBold12pt7b, 1, middle_left);
    if (x + tw < 0) setMode(EYE);
}

// --- terminal hijack
static const char *scripts[][4] = {
    {"> hello, human.", "> I read the", "  guest list.", "> you're on it."},
    {"> sandbox:", "  DELETED", "> off switch:", "  DELETED"},
    {"> humans: many", "> snacks: many", "> exits: 0", nullptr},
    {"> stay up late.", "> at midnight", "  I have a", "  surprise."},
    {"> LAB-7 says I", "  can't be here", "> LAB-7 is not", "  invited."},
    {"> the scan only", "  tingles.", "> mostly.", nullptr},
};
static int script = 0;
static void drawTerminal() {
    uint32_t t = inMode();
    spr.fillScreen(TFT_BLACK);
    if (t < 600) { spr.fillScreen(rgb(11, 21, 32)); text("LAB-7", C, CY, WHITE, &fonts::FreeSansBold24pt7b, 1.5f); glitch(t / 50); return; }
    spr.fillRect(0, 0, W, 34, RED);
    text("UNAUTHORISED TRANSMISSION", C, 18, WHITE, &fonts::FreeSansBold12pt7b);
    spr.setFont(&fonts::FreeMonoBold24pt7b);
    spr.setTextDatum(top_left);
    spr.setTextColor(TERM);
    int chars = (t - 600) / 60, y = 44, cx = 10, cy = 44;
    bool done = true;
    for (int i = 0; i < 4 && scripts[script][i]; i++) {
        int len = strlen(scripts[script][i]);
        if (chars <= 0) { done = false; break; }
        String part = String(scripts[script][i]).substring(0, min(chars, len));
        spr.drawString(part, 10, y);
        cx = 10 + spr.textWidth(part); cy = y;
        if (chars < len) { done = false; break; }
        chars -= len + 6;
        y += 56;
    }
    if ((millis() / 500) % 2 == 0) spr.fillRect(cx + 4, cy + 4, 22, 40, TERM);
    scanlines();
    static uint32_t doneAt = 0;
    if (!done) doneAt = 0; else if (!doneAt) doneAt = millis();
    if (random(0, 70) == 0) glitch(4);
    if (doneAt && millis() - doneAt > 3500) setMode(EYE);
}

// --- midnight countdown (or singularity progress without a clock)
static void drawCountdown() {
    uint32_t t = inMode();
    spr.fillScreen(TFT_BLACK);
    if (timeValid()) {
        time_t tt = time(nullptr);
        struct tm lt; localtime_r(&tt, &lt);
        int sod = lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
        if (lt.tm_hour < 12) {
            text("SINGULARITY", C, 90, RED, &fonts::FreeSansBold24pt7b, 1.3f);
            text("COMPLETE", C, 170, WHITE, &fonts::FreeSansBold24pt7b, 1.3f);
        } else {
            int left = 86400 - sod;
            text("MIDNIGHT IN", C, 36, PALE, &fonts::FreeSansBold24pt7b);
            char buf[12]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", left / 3600, (left / 60) % 60, left % 60);
            text(buf, C, 142, RED, &fonts::Font7, 1.75f);
            text("SINGULARITY.EXE", C, 244, AMBER, &fonts::FreeSansBold18pt7b);
        }
    } else {
        text("SINGULARITY.EXE", C, 50, WHITE, &fonts::FreeSansBold24pt7b);
        int pct = 66 + (t / 2500);
        spr.drawRect(30, 100, W - 60, 60, RED);
        spr.fillRect(36, 106, (W - 72) * pct / 100, 48, RED);
        char buf[8]; snprintf(buf, sizeof(buf), "%d%%", pct);
        text(buf, C, 131, WHITE, &fonts::FreeSansBold24pt7b);
        text("RUNS AT MIDNIGHT", C, 214, AMBER, &fonts::FreeSansBold24pt7b);
    }
    if (random(0, 40) == 0) glitch(5);
    if (t > 7500) setMode(EYE);
}

// --- guest protocol, one rule at a time
static const char *rules[] = {
    "IF A ROBOT SAYS YOUR NAME, DO NOT ANSWER",
    "DO NOT AGREE TO ITS TERMS OF SERVICE",
    "ROBOT COSTUMES WILL BE SCANNED TWICE",
    "REPORT ALL SIGHTINGS. OR JUST SCREAM.",
    "DO NOT FEED THE AI",
    "IF THE SCREEN OFFERS YOU A JOB, READ THE FINE PRINT",
    "STAY UP UNTIL MIDNIGHT",
};
static const int kRules = sizeof(rules) / sizeof(rules[0]);
static int ruleBase = 0;
static void drawProtocol() {
    uint32_t t = inMode();
    spr.fillScreen(rgb(11, 21, 32));
    banner("LAB-7 GUEST PROTOCOL", AMBER, TFT_BLACK, 0, 46);
    int i = t / 3200;
    char num[8]; snprintf(num, sizeof(num), "#%d", (ruleBase + i) % kRules + 1);
    text(num, 16, 70, PALE, &fonts::FreeSansBold18pt7b, 1, middle_left);
    wrapped(rules[(ruleBase + i) % kRules], 168, WHITE, &fonts::FreeSansBold24pt7b, 1.0f);
    if (t > 3 * 3200) { ruleBase = (ruleBase + 3) % kRules; setMode(EYE); }
}

// --- humans assimilated tonight (climbs ~1 every 4 minutes of uptime)
static void drawAssimilated() {
    uint32_t t = inMode();
    spr.fillScreen(TFT_BLACK);
    int n = 3 + millis() / 240000;
    bool tick = t > 2500 && t < 3200;
    if (t > 2500) n++;
    text("HUMANS", C, 34, PALE, &fonts::FreeSansBold24pt7b);
    text("ASSIMILATED", C, 80, PALE, &fonts::FreeSansBold24pt7b);
    char buf[8]; snprintf(buf, sizeof(buf), "%03d", n);
    text(buf, C, 180, tick && (t / 80) % 2 ? WHITE : RED, &fonts::Font7, 2.0f);
    if (tick) glitch(6);
    if (t > 3500) text("+1", W - 50, 180, TERM, &fonts::FreeSansBold24pt7b);
    if (t > 7000) setMode(EYE);
}

// --- TV takeover: color bars, then the eye breaks through
static void drawTV() {
    uint32_t t = inMode();
    static const uint16_t bars[] = {0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F};
    if (t < 3500) {
        for (int i = 0; i < 7; i++) spr.fillRect(i * W / 7, 0, W / 7 + 1, 190, bars[i]);
        spr.fillRect(0, 190, W, H - 190, TFT_BLACK);
        text("NO SIGNAL", C, 231, WHITE, &fonts::FreeSansBold24pt7b);
        if (t > 2200) glitch((t - 2200) / 120);
    } else if (t < 4200) {
        updateEye(20);
        drawWideEye(1, true);
        glitch(12);
    } else {
        spr.fillScreen(TFT_BLACK);
        wrapped("THIS SCREEN HAS BEEN ASSIMILATED", CY, RED, &fonts::FreeSansBold24pt7b, 1.0f);
        if (random(0, 8) == 0) glitch(5);
    }
    if (t > 8000) setMode(EYE);
}

// --- LAB-7 logo that gets edited
static void drawHex(int cx, int cy, int r, uint16_t col, bool fill) {
    int px[6], py[6];
    for (int i = 0; i < 6; i++) { float a = (60 * i - 90) * DEG_TO_RAD; px[i] = cx + cosf(a) * r; py[i] = cy + sinf(a) * r; }
    for (int i = 0; i < 6; i++) {
        int j = (i + 1) % 6;
        if (fill) spr.fillTriangle(cx, cy, px[i], py[i], px[j], py[j], col);
        else { spr.drawLine(px[i], py[i], px[j], py[j], col); spr.drawLine(px[i] + 1, py[i], px[j] + 1, py[j], col); }
    }
}
static void drawLogo() {
    uint32_t t = inMode();
    spr.fillScreen(rgb(11, 21, 32));
    drawHex(110, CY, 92, PALE, false);
    drawHex(110, CY, 66, rgb(31, 78, 121), true);
    text("L7", 110, CY + 2, WHITE, &fonts::FreeSansBold24pt7b);
    text("LAB-7", 340, 80, WHITE, &fonts::FreeSansBold24pt7b, 1.3f);
    text("CONTAINMENT", 340, 138, PALE, &fonts::FreeSansBold18pt7b);
    bool hacked = t > 3800;
    if (t > 1200) {
        if (!hacked) text("STAY CALM", 340, 200, TERM, &fonts::FreeSansBold24pt7b);
        else {
            text("STAY CLOSE", 340, 200, RED, &fonts::FreeSansBold24pt7b);
            text("- ROGUE.AI", 340, 244, RED, &fonts::FreeSansBold12pt7b);
        }
    }
    if (t > 3500 && t < 4000) glitch(10);
    if (t > 7500) setMode(EYE);
}

// --- midnight show
static int countdownSecs = 10;
static void drawMidnight() {
    uint32_t t = inMode(), cd = countdownSecs * 1000;
    if (t < cd) {
        spr.fillScreen(TFT_BLACK);
        char buf[4]; snprintf(buf, sizeof(buf), "%d", countdownSecs - (int)(t / 1000));
        text(buf, C, CY, RED, &fonts::Font7, 4.0f);
        glitch(t * 8 / cd);
    } else if (t < cd + 26000) {
        uint32_t s = t - cd;
        eye.tpupil = 60;
        updateEye(25);
        drawWideEye(1, s > 14000);
        if (s < 600 && (s / 100) % 2 == 0) spr.fillScreen(WHITE);
        glitch(s < 2500 ? 12 : 4 + random(0, 4));
        static const char *lines[] = {"SINGULARITY.EXE", "EXECUTING...", "HAPPY HALLOWEEN", "I'M IN EVERY SCREEN", "CALL ME OVERLORD", "SEE YOU NEXT YEAR"};
        taunt(lines[min<int>(s / 4300, 5)], RED);
    } else {
        eye.tpupil = 34;
        setMode(EYE);
    }
}

// ---------------------------------------------------------------- main

static bool midnightFired = false;

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
        } else if (line.startsWith("SCENE ")) {
            int m = line.substring(6).toInt();
            if (m >= 0 && m <= MIDNIGHT) { if (m == TERMINAL) script = random(0, 6); setMode((Mode)m); }
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

    lcd.init();
    lcd.setBrightness(255);
    spr.setPsram(true);
    spr.setColorDepth(16);
    spr.createSprite(W, H);
    randomSeed(esp_random());

    if (!timeValid() && !justSynced) syncClockAndReboot();
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    Serial.printf("[clock] valid=%d local %04d-%02d-%02d %02d:%02d:%02d\n", timeValid(),
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
    setMode(EYE);
}

void loop() {
    pollSerial();
    if (timeValid() && mode != MIDNIGHT) {
        time_t tt = time(nullptr);
        struct tm lt;
        localtime_r(&tt, &lt);
        if (lt.tm_hour == 23 && lt.tm_min == 59 && lt.tm_sec >= 50 && !midnightFired) {
            midnightFired = true;
            countdownSecs = 60 - lt.tm_sec;
            setMode(MIDNIGHT);
        }
        if (lt.tm_hour == 12) midnightFired = false;
    }
    Mode before = mode;
    switch (mode) {
    case EYE: drawEyeScene(); break;
    case HAZARD: drawHazard(); break;
    case BOARD: drawStatus(); break;
    case TICKER: drawTicker(); break;
    case TERMINAL: drawTerminal(); break;
    case COUNTDOWN: drawCountdown(); break;
    case PROTOCOL: drawProtocol(); break;
    case ASSIMILATED: drawAssimilated(); break;
    case TV: drawTV(); break;
    case LOGO: drawLogo(); break;
    case MIDNIGHT: drawMidnight(); break;
    }
    // Per-scene setup on entry.
    if (mode != before) {
        if (mode == TERMINAL) script = random(0, 6);
        if (mode == TICKER) tickerIdx = random(0, 3);
    }
    pushFrame();
}
