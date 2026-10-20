#include "term.h"

using namespace gfx;

namespace term {

int bodyCols(FontId f) { return (W - 2 * MARGIN_X) / charW(f); }

int wrap(const char* s, int maxCols, Line* out, int maxLines) {
  int count = 0;
  const char* p = s;
  while (*p && count < maxLines) {
    // Take up to maxCols characters, stopping at '\n'.
    int n = 0;
    while (p[n] && p[n] != '\n' && n < maxCols) n++;
    int take = n;
    if (p[n] && p[n] != '\n') {
      // Line is full mid-text: break at the last space if there is one.
      int sp = n;
      while (sp > 0 && p[sp] != ' ') sp--;
      if (sp > 0) take = sp;
    }
    out[count++] = {p, take};
    p += take;
    if (*p == '\n' || *p == ' ') p++;  // drop the break character
  }
  return count;
}

// ---- Typer ------------------------------------------------------------------------
void Typer::start(const char* s_, int len_, int x_, int y_, FontId f_, uint32_t color_,
                  int baseMs_, uint32_t now) {
  s = s_;
  len = len_;
  pos = 0;
  x = x_;
  y = y_;
  f = f_;
  color = color_;
  baseMs = baseMs_;
  nextAt = now;
  burst = 0;
  cursorPlace(x, y, f, C_GREEN, now);
}

int Typer::delayAfter(char c) {
  if (baseMs == 0) return 0;
  int d = baseMs * (int)rnd(50, 160) / 100;
  if (burst > 0) {
    d = baseMs / 3;
    burst--;
  } else if (rnd(100) < 4) {
    burst = rnd(3, 8);  // a quick flurry of keys
  }
  switch (c) {
    case '.': case '!': case '?': d += rnd(250, 450); break;
    case ',': case ';': d += rnd(100, 200); break;
    case ':': d += 150; break;
    case ' ': if (rnd(100) < 5) d += rnd(150, 450); break;  // "thinking"
    default: break;
  }
  return d;
}

bool Typer::tick(uint32_t now) {
  if (pos >= len) return true;
  int cw = charW(f);
  while (pos < len && (int32_t)(now - nextAt) >= 0) {
    char c = s[pos++];
    cursorHide();  // the cursor cell is where this character goes
    if (c != ' ') textN(&c, 1, x, y, f, color);
    dirty(x - 2, y, cw + 4, lineH(f));
    x += cw;
    nextAt += delayAfter(c);
  }
  cursorPlace(x, y, f, C_GREEN, now);
  return pos >= len;
}

// ---- Cursor ------------------------------------------------------------------------
static struct {
  bool active = false;  // supposed to be on screen (blinking)
  bool drawn = false;   // currently painted
  int x, y;
  FontId f;
  uint32_t color;
  uint32_t toggleAt;
} s_cur;

static void cursorRect(int* x, int* y, int* w, int* h) {
  int lh = lineH(s_cur.f);
  *x = s_cur.x + 1;
  *y = s_cur.y + lh / 8;
  *w = charW(s_cur.f) - 2;
  *h = lh - lh / 4;
}

static void cursorDraw(bool on) {
  int x, y, w, h;
  cursorRect(&x, &y, &w, &h);
  if (on) {
    canvas.fillRect(x, y, w, h, s_cur.color);
    dirty(x, y, w, h);
  } else {
    clearRect(x, y, w, h);
  }
  s_cur.drawn = on;
}

void cursorPlace(int x, int y, FontId f, uint32_t color, uint32_t now) {
  if (s_cur.active && s_cur.drawn && (s_cur.x != x || s_cur.y != y)) cursorDraw(false);
  s_cur.active = true;
  s_cur.x = x;
  s_cur.y = y;
  s_cur.f = f;
  s_cur.color = color;
  if (!s_cur.drawn) cursorDraw(true);
  s_cur.toggleAt = now + 530;  // solid while typing, blinks once idle
}

void cursorHide() {
  if (s_cur.active && s_cur.drawn) cursorDraw(false);
  s_cur.active = false;
}

void cursorForget() {
  s_cur.active = false;
  s_cur.drawn = false;
}

void cursorTick(uint32_t now) {
  if (!s_cur.active || (int32_t)(now - s_cur.toggleAt) < 0) return;
  cursorDraw(!s_cur.drawn);
  s_cur.toggleAt = now + 530;
}

// ---- Bag ---------------------------------------------------------------------------
void Bag::init(int n_) {
  n = min(n_, (int)sizeof(order));
  pos = n;  // forces a shuffle on first next()
}

int Bag::next() {
  if (n <= 0) return 0;
  if (n == 1) return 0;
  if (pos >= n) {
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = n - 1; i > 0; i--) {
      int j = rnd(i + 1);
      uint8_t t = order[i];
      order[i] = order[j];
      order[j] = t;
    }
    if (order[0] == last) {  // no repeat across the refill boundary
      uint8_t t = order[0];
      order[0] = order[n - 1];
      order[n - 1] = t;
    }
    pos = 0;
  }
  last = order[pos++];
  return last;
}

}  // namespace term
