#include "bigtext.h"

using namespace gfx;

namespace bigtext {

static constexpr int SIDE = 48;  // horizontal margin

// The laid-out text is rendered once into an 8-bit alpha mask; each frame then
// blends the red, cyan and main copies from it straight into the canvas.
// (Drawing a 170 px anti-aliased font three times through LovyanGFX took
// ~200 ms a frame; the mask blend is a few ms.)
static LGFX_Sprite s_mask;
static int s_maskW = 0, s_maskH = 0;

bool begin() {
  s_mask.setColorDepth(lgfx::grayscale_8bit);
  s_mask.setPsram(true);
  if (!s_mask.createSprite(W, BODY_H)) return false;
  s_mask.setTextDatum(lgfx::textdatum_t::top_left);
  s_mask.setTextWrap(false);
  return true;
}

static void renderMask(const Layout& L) {
  s_maskW = min(L.right - L.left, W);
  s_maskH = min(L.bottom - L.top, BODY_H);
  s_mask.fillRect(0, 0, s_maskW, s_maskH, (uint8_t)0);
  s_mask.setFont(fontObj(L.font));
  s_mask.setTextColor(0xFFFFFFu);
  char buf[64];
  for (int i = 0; i < L.n; i++) {
    int n = min(L.lines[i].len, (int)sizeof(buf) - 1);
    memcpy(buf, L.lines[i].s, n);
    buf[n] = 0;
    s_mask.drawString(buf, L.x[i] - L.left, L.y[i] - L.top);
  }
}

// Blends `rgb` into the canvas through the mask, shifted by (ox, oy).
static void blendMask(const Layout& L, uint32_t rgb, int ox, int oy) {
  const int cr = (rgb >> 19) & 31, cg = (rgb >> 10) & 63, cb = (rgb >> 3) & 31;
  const uint16_t c = (uint16_t)(cr << 11 | cg << 5 | cb);
  const uint16_t cs = (uint16_t)(c >> 8 | c << 8);  // canvas stores byte-swapped RGB565
  auto* dst = (uint16_t*)canvas.getBuffer();
  auto* mask = (const uint8_t*)s_mask.getBuffer();
  const int x0 = L.left + ox, y0 = L.top + oy;
  const int mxs = max(0, -x0), mxe = min(s_maskW, W - x0);
  for (int my = 0; my < s_maskH; my++) {
    int y = y0 + my;
    if (y < BODY_Y || y >= H) continue;
    const uint8_t* m = mask + (size_t)my * W;
    uint16_t* d = dst + (size_t)y * W + x0;
    for (int mx = mxs; mx < mxe; mx++) {
      uint32_t a = m[mx];
      if (!a) continue;
      if (a >= 250) {
        d[mx] = cs;
        continue;
      }
      uint16_t v = (uint16_t)(d[mx] >> 8 | d[mx] << 8);
      int r = v >> 11, g = (v >> 5) & 63, b = v & 31;
      r += ((cr - r) * (int)a) >> 8;
      g += ((cg - g) * (int)a) >> 8;
      b += ((cb - b) * (int)a) >> 8;
      v = (uint16_t)(r << 11 | g << 5 | b);
      d[mx] = (uint16_t)(v >> 8 | v << 8);
    }
  }
}

// Line pitch: the big fonts are caps-only in practice, so lines can sit
// closer than the font's full ascent+descent.
static int pitchOf(FontId f) { return f >= F_HEAD ? lineH(f) * 78 / 100 : lineH(f); }

void layout(const char* s, int areaY, int areaH, Layout* L, int areaX, int areaW) {
  static const FontId ORDER[] = {F_HUGE, F_HEAD, F_LARGE, F_BODY};
  // First pass: largest font where every authored line fits as-is.
  L->font = F_BODY;
  bool found = false;
  for (FontId f : ORDER) {
    int cols = (areaW - 2 * SIDE) / charW(f);
    int n = term::wrap(s, 999, L->lines, MAX_LINES);
    int longest = 0;
    for (int i = 0; i < n; i++) longest = max(longest, L->lines[i].len);
    if (longest <= cols && n * pitchOf(f) <= areaH) {
      L->font = f;
      L->n = n;
      found = true;
      break;
    }
  }
  if (!found) {
    // Nothing fits as authored: word-wrap in the body font.
    L->n = term::wrap(s, (areaW - 2 * SIDE) / charW(F_BODY), L->lines, MAX_LINES);
  }

  int pitch = pitchOf(L->font);
  int cw = charW(L->font);
  int top = areaY + (areaH - L->n * pitch) / 2;
  if (L->font >= F_HEAD) top -= lineH(L->font) / 10;  // visually centre the caps
  L->left = W;
  L->right = 0;
  for (int i = 0; i < L->n; i++) {
    int w = L->lines[i].len * cw;
    L->x[i] = areaX + (areaW - w) / 2;
    L->y[i] = top + i * pitch;
    L->left = min(L->left, L->x[i]);
    L->right = max(L->right, L->x[i] + w);
  }
  L->top = top;
  L->bottom = top + (L->n - 1) * pitch + lineH(L->font);
#ifdef BIGTEXT_TIMING
  uint32_t t0 = micros();
#endif
  renderMask(*L);
#ifdef BIGTEXT_TIMING
  Serial.printf("[D] mask render %u us\n", (unsigned)(micros() - t0));
#endif
}

void draw(const Layout& L, uint32_t color, int offset, int jx, int jy, int pad) {
#ifdef BIGTEXT_TIMING
  uint32_t t0 = micros();
#endif
  clearRect(L.left - pad, L.top - pad, L.right - L.left + 2 * pad, L.bottom - L.top + 2 * pad);
  blendMask(L, C_RED, jx - offset, jy);
  blendMask(L, C_CYAN, jx + offset, jy);
  blendMask(L, color, jx, jy);
#ifdef BIGTEXT_TIMING
  static int n = 0;
  if (++n % 10 == 0) Serial.printf("[D] draw %u us\n", (unsigned)(micros() - t0));
#endif
}

static constexpr int ANIM_PAD = 40;

void Animator::start(const char* s, int areaY, int areaH, uint32_t c, uint32_t now, int areaX,
                     int areaW) {
  layout(s, areaY, areaH, &L, areaX, areaW);
  color = c;
  nextAt = now;
}

void Animator::tick(uint32_t now) {
  if ((int32_t)(now - nextAt) < 0) return;
  uint32_t r = rnd(100);
  if (r < 4) {
    // Blackout: the text drops out for a moment.
    clearRect(L.left - ANIM_PAD, L.top - ANIM_PAD, L.right - L.left + 2 * ANIM_PAD,
              L.bottom - L.top + 2 * ANIM_PAD);
    nextAt = now + rnd(40, 90);
  } else if (r < 14) {
    // Hard glitch: wide split, big jitter, a couple of sheared slices.
    draw(L, color, rnd(16, 28), (int)rnd(0, 16) - 8, (int)rnd(0, 8) - 4, ANIM_PAD);
    for (int i = rnd(1, 2); i > 0; i--) {
      sliceShift(rnd(L.top, L.bottom - 10), rnd(6, 30), (int)rnd(12, 40) * (rnd(2) ? 1 : -1),
                 L.left - ANIM_PAD, L.right + ANIM_PAD);
    }
    nextAt = now + rnd(50, 90);
  } else {
    uint32_t c = rnd(100) < 6 ? C_WHITE : color;
    draw(L, c, rnd(5, 9), (int)rnd(0, 4) - 2, (int)rnd(0, 2) - 1, ANIM_PAD);
    nextAt = now + rnd(90, 240);
  }
}

}  // namespace bigtext
