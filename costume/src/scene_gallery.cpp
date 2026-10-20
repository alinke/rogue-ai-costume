// "ASSIMILATED HUMANS": the night's scan photos as a wall of green mugshots,
// revealed one by one, with specimen numbers and verdicts. Only comes up once
// two or more people have been scanned.
#include "gallery.h"
#include "messages.h"
#include "scenes.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int COLS = 4, TW = gallery::THUMB_W, TH = 250;  // thumbnails cropped to 250 rows
constexpr int CROP = (gallery::THUMB_H - TH) / 2;
constexpr int GAPX = 20;
constexpr int X0 = (W - (COLS * TW + (COLS - 1) * GAPX)) / 2;
constexpr int Y0 = BODY_Y + 120;
constexpr int PITCH_Y = TH + 46;
constexpr uint32_t REVEAL_MS = 380, HOLD_MS = 4500;

int first, shown, revealed, revealRows;
uint32_t revealStart, endAt;

void cell(int k, int* x, int* y) {
  *x = X0 + (k % COLS) * (TW + GAPX);
  *y = Y0 + (k / COLS) * PITCH_Y;
}

void label(int k) {
  gallery::Entry e = gallery::entry(first + k);
  int x, y;
  cell(k, &x, &y);
  char buf[48];
  snprintf(buf, sizeof(buf), "#%03d %s", e.number, e.verdict ? e.verdict : "");
  int maxChars = TW / charW(F_SMALL);
  textN(buf, min((int)strlen(buf), maxChars), x, y + TH + 6, F_SMALL,
        first + k == gallery::count() - 1 ? C_RED : C_GREEN);
  dirty(x, y + TH, TW, 46);
}

void enter(uint32_t now) {
  shown = min(gallery::count(), 2 * COLS);
  first = gallery::count() - shown;  // the newest ones
  revealed = 0;
  revealRows = 0;
  revealStart = now;
  char title[48];
  snprintf(title, sizeof(title), "%s: %d", GALLERY_TITLE, gallery::total());
  text(title, X0, BODY_Y + 24, F_LARGE, C_AMBER);
  dirty(0, BODY_Y, W, 120);
  endAt = 0;
}

bool tick(uint32_t now) {
  if (revealed < shown) {
    // Raster-reveal the current thumbnail top to bottom.
    gallery::Entry e = gallery::entry(first + revealed);
    int x, y;
    cell(revealed, &x, &y);
    int rows = min(TH, (int)((now - revealStart) * TH / REVEAL_MS));
    if (rows > revealRows) {
      ui::grayImage(e.pixels + CROP * TW, TW, x, y, TW, revealRows, rows, ui::GREEN);
      revealRows = rows;
    }
    if (rows >= TH) {
      if (first + revealed == gallery::count() - 1) {
        for (int t = 0; t < 3; t++) canvas.drawRect(x - 4 + t, y - 4 + t, TW + 8 - 2 * t, TH + 8 - 2 * t, C_RED);
        dirty(x - 4, y - 4, TW + 8, TH + 8);
      }
      label(revealed);
      revealed++;
      revealRows = 0;
      revealStart = now;
      if (revealed == shown) endAt = now + HOLD_MS;
    }
    return true;
  }
  return (int32_t)(now - endAt) < 0;
}

bool available() { return gallery::count() >= 2; }

}  // namespace

const Scene SCENE_GALLERY = {"gallery", enter, tick, nullptr, available};
