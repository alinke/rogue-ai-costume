#include "ui.h"

#include <math.h>

#include "term.h"

using namespace gfx;

namespace ui {

void centredText(const char* s, int x, int y, int w, int h, FontId f, uint32_t color) {
  term::Line lines[4];
  int n = term::wrap(s, max(1, w / charW(f)), lines, 4);
  int top = y + (h - n * lineH(f)) / 2;
  for (int i = 0; i < n; i++) {
    textN(lines[i].s, lines[i].len, x + (w - lines[i].len * charW(f)) / 2, top + i * lineH(f), f, color);
  }
}

void drawButton(const Button& b, FontId f, uint32_t color, bool lit) {
  clearRect(b.x, b.y, b.w, b.h);
  if (lit) {
    canvas.fillRoundRect(b.x, b.y, b.w, b.h, 10, color);
  } else {
    for (int t = 0; t < 3; t++) canvas.drawRoundRect(b.x + t, b.y + t, b.w - 2 * t, b.h - 2 * t, 10, color);
  }
  centredText(b.label, b.x, b.y, b.w, b.h, f, lit ? C_BG : color);
  dirty(b.x, b.y, b.w, b.h);
}

static uint16_t s_ramp[2][256];

static uint16_t swapped565(uint32_t rgb) {
  uint16_t c = (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
  return (uint16_t)(c >> 8 | c << 8);
}

static void buildRamps() {
  for (int i = 0; i < 256; i++) {
    // A little gamma lift so mid-tones (faces) read well on the dark panel.
    float t = powf(i / 255.0f, 0.8f);
    s_ramp[GREEN][i] = swapped565(((uint32_t)(0x28 * t) << 16) | ((uint32_t)(0xFF * t) << 8) | (uint32_t)(0x5A * t));
    s_ramp[RED][i] = swapped565(((uint32_t)(0xFF * t) << 16) | ((uint32_t)(0x1E * t) << 8) | (uint32_t)(0x28 * t));
  }
}

void grayImage(const uint8_t* src, int stride, int x, int y, int w, int rowFrom, int rowTo, Ramp ramp) {
  if (s_ramp[GREEN][255] == 0) buildRamps();
  const uint16_t* lut = s_ramp[ramp];
  auto* buf = (uint16_t*)canvas.getBuffer();
  for (int r = rowFrom; r < rowTo; r++) {
    int sy = y + r;
    if (sy < BODY_Y || sy >= H) continue;
    const uint8_t* s = src + (size_t)r * stride;
    uint16_t* d = buf + (size_t)sy * W + x;
    bool dim = sy % 3 == 2;
    for (int i = 0; i < w; i++) d[i] = lut[dim ? s[i] >> 1 : s[i]];
  }
  if (rowTo > rowFrom) dirty(x, y + rowFrom, w, rowTo - rowFrom);
}

}  // namespace ui
