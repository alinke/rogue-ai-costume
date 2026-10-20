#pragma once
// Small UI pieces for the interactive scenes: touch buttons, and grayscale
// photos drawn through a colour ramp (the scan look).
#include "gfx.h"

namespace ui {

struct Button {
  int x, y, w, h;
  const char* label;  // '\n' allowed
  bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// Outlined button; `lit` = filled (pressed / selected), text in black.
void drawButton(const Button& b, gfx::FontId f, uint32_t color, bool lit);

// Centred text (single or multi-line) in a box.
void centredText(const char* s, int x, int y, int w, int h, gfx::FontId f, uint32_t color);

// Grayscale image (stride = row length in bytes) drawn through a colour ramp,
// every third row dimmed for a CRT look. Rows [rowFrom, rowTo) only.
enum Ramp { GREEN, RED };
void grayImage(const uint8_t* src, int stride, int x, int y, int w, int rowFrom, int rowTo, Ramp ramp);

}  // namespace ui
