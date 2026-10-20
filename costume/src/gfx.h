#pragma once
// Drawing layer: one full-screen PSRAM canvas that every scene draws into,
// pushed to the panel as dirty rectangles. All buffers are allocated once at
// boot; nothing here allocates while running.
#include <Arduino.h>
#include <LovyanGFX.hpp>

namespace gfx {

constexpr int W = 1280;
constexpr int H = 800;
constexpr int HEADER_H = 44;           // persistent header bar
constexpr int BODY_Y = HEADER_H;
constexpr int BODY_H = H - HEADER_H;
constexpr int MARGIN_X = 40;           // left/right text margin

// Palette (RGB888 -- LovyanGFX converts uint32_t colours from RGB888).
constexpr uint32_t C_BG       = 0x000000;
constexpr uint32_t C_GREEN    = 0x28FF5A;
constexpr uint32_t C_DIM      = 0x00823A;
constexpr uint32_t C_RED      = 0xFF1E28;
constexpr uint32_t C_AMBER    = 0xFFAA00;
constexpr uint32_t C_CYAN     = 0x00DCFF;
constexpr uint32_t C_WHITE    = 0xEBFFF0;
constexpr uint32_t C_HEADER   = 0x00240A;
constexpr uint32_t C_SCANLINE = 0x001204;
constexpr int SCANLINE_STEP = 4;

enum FontId : uint8_t { F_SMALL, F_BODY, F_LARGE, F_HEAD, F_HUGE, F_COUNT };

extern LGFX_Sprite canvas;

bool begin();                           // panel, canvas, fonts; false = fatal
void setBacklightPercent(int pct);
void powerDown();                       // backlight off, panel asleep, DSI power rail off
bool getTouch(int* x, int* y);          // screen coordinates (calibrated); false = no touch
// Touch correction applied by getTouch(): x' = ax*x + bx, y' = ay*y + by.
void setTouchCal(float ax, float bx, float ay, float by);
void getTouchCal(float* ax, float* bx, float* ay, float* by);

// Fonts (JetBrains Mono, monospace): every glyph has the same advance.
void font(FontId f);                    // select on the canvas
int charW(FontId f);                    // advance per character, px
int lineH(FontId f);                    // font line height, px
const lgfx::IFont* fontObj(FontId f);   // for drawing into other sprites

// Background: black with a faint scanline every SCANLINE_STEP rows (body only).
void clearRect(int x, int y, int w, int h);
void clearBody();

// Text, drawn transparently over whatever is there (anti-aliased).
void text(const char* s, int x, int y, FontId f, uint32_t color);
void textN(const char* s, int n, int x, int y, FontId f, uint32_t color);

// Chromatic-aberration text: red copy offset left, cyan copy right, main colour
// on top. (x, y) is the top-left of the main copy. Marks its area dirty.
void chromaText(const char* s, int n, int x, int y, FontId f, uint32_t color, int offset);

// Dirty-rectangle tracking and push.
void dirty(int x, int y, int w, int h);
void dirtyAll();
void present();                         // push dirty areas to the panel

// Glitch effects, applied to the canvas (marks the whole screen dirty).
// intensity 0..1; tint is the dominant bar colour (e.g. C_RED for alarms).
void glitchFrame(float intensity, uint32_t tint);
// Shift a band of rows horizontally, within columns [x0, x1).
void sliceShift(int y, int h, int dx, int x0 = 0, int x1 = W);

// Filled ring (annulus) between radii r0 < r1, drawn as two spans per row.
void fillRing(int cx, int cy, int r0, int r1, uint32_t color);

// Small helpers
uint32_t rnd(uint32_t lo, uint32_t hi); // inclusive
inline uint32_t rnd(uint32_t n) { return rnd(0, n - 1); }

}  // namespace gfx
