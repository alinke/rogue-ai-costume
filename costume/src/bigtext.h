#pragma once
// Huge centred text with chromatic aberration (headlines, "I'M OUT.",
// "DO NOT TOUCH ME."). Picks the largest font that fits the area without
// re-breaking the author's lines; '\n' in the text forces a break.
#include "gfx.h"
#include "term.h"

namespace bigtext {

constexpr int MAX_LINES = 6;

struct Layout {
  gfx::FontId font;
  int n;
  term::Line lines[MAX_LINES];
  int x[MAX_LINES], y[MAX_LINES];
  int left, top, right, bottom;  // bounding box of the main copy
};

// Allocates the alpha mask (boot only). There is one mask: only the most
// recently laid-out text can be drawn.
bool begin();

// Lays out s centred in the area (x range defaults to the full width).
void layout(const char* s, int areaY, int areaH, Layout* out, int areaX = 0, int areaW = gfx::W);

// Clears the previous draw's area (pad covers offsets + jitter), then draws:
// red copy offset left, cyan right, `color` on top, all shifted by (jx, jy).
void draw(const Layout& L, uint32_t color, int offset, int jx, int jy, int pad);

// A headline that lives on screen: irregular flicker, chromatic jitter,
// occasional hard glitches (wide split + sheared slices) and brief blackouts.
struct Animator {
  void start(const char* s, int areaY, int areaH, uint32_t color, uint32_t now, int areaX = 0,
             int areaW = gfx::W);
  void tick(uint32_t now);
  Layout L;
  uint32_t color = 0, nextAt = 0;
};

}  // namespace bigtext
