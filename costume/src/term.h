#pragma once
// Terminal helpers shared by the typed scenes: word wrap, a character-by-
// character typer with human-ish timing, a block cursor, and a shuffle bag
// for picking lines without back-to-back repeats. No heap use.
#include <Arduino.h>
#include "gfx.h"

namespace term {

struct Line { const char* s; int len; };

// Word-wraps s into at most maxLines lines of at most maxCols characters.
// '\n' forces a break. Returns the number of lines.
int wrap(const char* s, int maxCols, Line* out, int maxLines);

// Columns of font f that fit between the side margins.
int bodyCols(gfx::FontId f);

// Types text one character at a time at a varied, natural speed.
struct Typer {
  // baseMs: average delay per character. 0 = draw everything at once.
  void start(const char* s, int len, int x, int y, gfx::FontId f, uint32_t color,
             int baseMs, uint32_t now);
  bool tick(uint32_t now);  // true once every character is on screen
  bool done() const { return pos >= len; }
  int cursorX() const { return x; }

  const char* s = nullptr;
  int len = 0, pos = 0;
  int x = 0, y = 0;
  gfx::FontId f = gfx::F_BODY;
  uint32_t color = gfx::C_GREEN;
  int baseMs = 40;
  uint32_t nextAt = 0;
  int burst = 0;

 private:
  int delayAfter(char c);
};

// Block cursor (one per screen). Blinks when idle, solid while typing.
void cursorPlace(int x, int y, gfx::FontId f, uint32_t color, uint32_t now);
void cursorHide();                 // erase it and stop drawing it
void cursorTick(uint32_t now);     // blink
void cursorForget();               // screen was cleared: drop it without erasing

// Shuffle bag: hands out 0..n-1 in random order, never the same index twice
// in a row (also across refills).
struct Bag {
  void init(int n);
  int next();
  uint8_t order[64];
  int n = 0, pos = 0, last = -1;
};

}  // namespace term
