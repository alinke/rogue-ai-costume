#include "gfx.h"

#include <esp_random.h>
#include <esp_heap_caps.h>
#if CONFIG_IDF_TARGET_ESP32P4
#include <esp_cache.h>
#include <driver/ppa.h>
#endif

#include "LGFX_Config.hpp"  // board bring-up, unmodified from the Sidekick firmware
#include "config.h"
#include "fonts/font_body.h"
#include "fonts/font_head.h"
#include "fonts/font_huge.h"
#include "fonts/font_large.h"
#include "fonts/font_small.h"

namespace gfx {

static LGFX lcd;
LGFX_Sprite canvas(&lcd);

#if CONFIG_IDF_TARGET_ESP32P4
// ---- Push to the panel ---------------------------------------------------------
// The panel's frame buffer is native portrait (800x1280, little-endian RGB565);
// the canvas is landscape (1280x800, LovyanGFX's byte-swapped RGB565).
// LovyanGFX's own rotated pushSprite converts pixel by pixel in software:
// ~900 ms for a full screen. The P4's PPA (pixel processing accelerator)
// rotates and byte-swaps by DMA instead, straight into the frame buffer.
static ppa_client_handle_t s_ppa = nullptr;
static uint16_t* s_canvasBuf = nullptr;
static void* s_fb = nullptr;
static size_t s_fbBytes = 0;

#ifdef BOARD_ELECROW_P4_10
// Elecrow 10.1": a 1024x600 landscape panel. The canvas goes out at 3/4 scale
// (960x600) between 32-pixel black bars, so nothing is stretched. Blocks are
// widened to multiples of 4 pixels so 3/4 of each edge is a whole pixel.
static constexpr int PW = 1024, PH = 600, PX0 = (PW - W * 3 / 4) / 2;

static bool ppaPush(int x, int y, int w, int h, int) {
  int x0 = x & ~3, y0 = y & ~3;
  int x1 = min(W, (x + w + 3) & ~3), y1 = min(H, (y + h + 3) & ~3);
  ppa_srm_oper_config_t op = {};
  op.in.buffer = s_canvasBuf;
  op.in.pic_w = W;
  op.in.pic_h = H;
  op.in.block_w = x1 - x0;
  op.in.block_h = y1 - y0;
  op.in.block_offset_x = x0;
  op.in.block_offset_y = y0;
  op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.out.buffer = s_fb;
  op.out.buffer_size = s_fbBytes;
  op.out.pic_w = PW;
  op.out.pic_h = PH;
  op.out.block_offset_x = PX0 + x0 * 3 / 4;
  op.out.block_offset_y = y0 * 3 / 4;
  op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  op.scale_x = 0.75f;
  op.scale_y = 0.75f;
  op.byte_swap = true;
  op.mode = PPA_TRANS_MODE_BLOCKING;
  return ppa_do_scale_rotate_mirror(s_ppa, &op) == ESP_OK;
}
#else
static bool ppaPush(int x, int y, int w, int h, int rotation) {
  ppa_srm_oper_config_t op = {};
  op.in.buffer = s_canvasBuf;
  op.in.pic_w = W;
  op.in.pic_h = H;
  op.in.block_w = w;
  op.in.block_h = h;
  op.in.block_offset_x = x;
  op.in.block_offset_y = y;
  op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.out.buffer = s_fb;
  op.out.buffer_size = s_fbBytes;
  op.out.pic_w = H;  // portrait
  op.out.pic_h = W;
  op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  if (rotation == 1) {
    // LovyanGFX rotation 1: landscape (x, y) -> panel (799 - y, x) = 90 deg clockwise.
    op.rotation_angle = PPA_SRM_ROTATION_ANGLE_270;  // PPA angles are counter-clockwise
    op.out.block_offset_x = H - y - h;
    op.out.block_offset_y = x;
  } else {
    // Rotation 3 (mounted upside down): landscape (x, y) -> panel (y, 1279 - x).
    op.rotation_angle = PPA_SRM_ROTATION_ANGLE_90;
    op.out.block_offset_x = y;
    op.out.block_offset_y = W - x - w;
  }
  op.scale_x = 1.0f;
  op.scale_y = 1.0f;
  op.byte_swap = true;
  op.mode = PPA_TRANS_MODE_BLOCKING;
  return ppa_do_scale_rotate_mirror(s_ppa, &op) == ESP_OK;
}
#endif

#ifdef GFX_PPA_SELFTEST
// Boot-time check that ppaPush() writes exactly what LovyanGFX's own (slow,
// known-correct) path writes. Development only.
static void ppaSelfTest() {
  const int x = 100, y = 60, w = 300, h = 200;
  for (int i = 0; i < w * h; i++) canvas.drawPixel(x + i % w, y + i / w, (uint16_t)esp_random());
  auto* ref = (uint8_t*)heap_caps_malloc(s_fbBytes, MALLOC_CAP_SPIRAM);
  memset(s_fb, 0, s_fbBytes);
  esp_cache_msync(s_fb, s_fbBytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  lcd.setClipRect(x, y, w, h);
  canvas.pushSprite(&lcd, 0, 0);
  lcd.clearClipRect();
  lcd.display();
  memcpy(ref, s_fb, s_fbBytes);
  for (int rot : {1, 3}) {
    memset(s_fb, 0, s_fbBytes);
    esp_cache_msync(s_fb, s_fbBytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    uint32_t t0 = micros();
    bool ok = ppaPush(x, y, w, h, rot);
    uint32_t us = micros() - t0;
    size_t diff = 0;
    for (size_t i = 0; i < s_fbBytes; i++) diff += ((uint8_t*)s_fb)[i] != ref[i];
    Serial.printf("[I][selftest] PPA rot %d: %s, %u bytes differ from LovyanGFX, %u us\n", rot,
                  ok ? "ok" : "FAILED", (unsigned)diff, (unsigned)us);
  }
  heap_caps_free(ref);
  canvas.fillScreen(C_BG);
}
#endif

#else
// ---- Push to the panel (Elecrow 7", ESP32-S3) --------------------------------------
// The scenes all draw at 1280x800; this panel is 800x480. Each dirty rect is
// scaled down (5/8 x 3/5), every output pixel the average of a 2x2 block, so
// 1-pixel lines and the scanlines don't vanish or shimmer. Writes go to the
// panel's live frame buffer directly (LovyanGFX's per-pixel path was ~1 us a
// pixel: 400 ms for a full screen).
static uint16_t* s_canvasBuf = nullptr;
static constexpr int PW = 800, PH = 480;  // the panel

static uint16_t s_sx[PW], s_sy[PH];       // panel x / y -> canvas x / y
static uint16_t* s_fb = nullptr;          // the panel's live frame buffer
static bool s_fbSwapped = false;          // frame buffer holds byte-swapped RGB565 too

// Average of four RGB565 pixels: green moved to the upper half-word so all three
// channels add up in one 32-bit register without carrying into each other.
static inline uint32_t spread(uint16_t v) { return (v | ((uint32_t)v << 16)) & 0x07E0F81Fu; }
static inline uint16_t avg4(uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
  uint32_t t = (spread(a) + spread(b) + spread(c) + spread(d)) >> 2;
  t &= 0x07E0F81Fu;
  return (uint16_t)(t | (t >> 16));
}

// fast: one source pixel per panel pixel (2.6x quicker -- the PSRAM the panel is
// scanned out of leaves little bandwidth), for big areas while they animate.
static void scalePush(int x, int y, int w, int h, bool fast) {
  int tx0 = x * PW / W, tx1 = min(PW, ((x + w) * PW + W - 1) / W);
  int ty0 = y * PH / H, ty1 = min(PH, ((y + h) * PH + H - 1) / H);
  if (tx1 <= tx0 || ty1 <= ty0) return;
  for (int ty = ty0; ty < ty1; ty++) {
    int sy = s_sy[ty], sy2 = min(H - 1, sy + 1);
    const uint16_t* a = s_canvasBuf + sy * W;
    const uint16_t* b = s_canvasBuf + sy2 * W;
    uint16_t* out = s_fb + ty * PW;
    for (int tx = tx0; tx < tx1; tx++) {
      int sx = s_sx[tx], sx2 = sx + 1 < W ? sx + 1 : sx;
      // canvas pixels are byte-swapped (LovyanGFX's swap565)
      uint16_t v = fast ? __builtin_bswap16(a[sx])
                        : avg4(__builtin_bswap16(a[sx]), __builtin_bswap16(a[sx2]),
                               __builtin_bswap16(b[sx]), __builtin_bswap16(b[sx2]));
      out[tx] = s_fbSwapped ? __builtin_bswap16(v) : v;
    }
  }
}


// Big areas go out fast while they change; once the screen settles they're
// redrawn once with the averaging (see present()).
static constexpr int FAST_AREA = 150000;  // canvas pixels
static int s_refX0 = W, s_refY0 = H, s_refX1 = 0, s_refY1 = 0;
static uint32_t s_lastChange = 0;
#endif

// ---- Fonts --------------------------------------------------------------------
// Each font is parsed once at boot into its own VLWfont; switching is just a
// pointer swap, and glyph bitmaps are read straight from flash.
static lgfx::PointerWrapper s_fontData[F_COUNT];
static lgfx::VLWfont s_fonts[F_COUNT];
static int s_charW[F_COUNT];
static int s_lineH[F_COUNT];

static bool loadFont(FontId id, const uint8_t* data, size_t len) {
  s_fontData[id].set(data, len);
  if (!s_fonts[id].loadFont(&s_fontData[id])) return false;
  canvas.setFont(&s_fonts[id]);
  s_charW[id] = canvas.textWidth("M");
  s_lineH[id] = canvas.fontHeight();
  return true;
}

void font(FontId f) { canvas.setFont(&s_fonts[f]); }
int charW(FontId f) { return s_charW[f]; }
int lineH(FontId f) { return s_lineH[f]; }
const lgfx::IFont* fontObj(FontId f) { return &s_fonts[f]; }

// ---- Setup --------------------------------------------------------------------
bool begin() {
  if (!lcd.init()) return false;
  lcd.setBrightness(0);  // stays dark until the first frame is on screen
  lcd.setRotation(SCREEN_ROTATION);
#ifdef BOARD_ELECROW_P4_10
  if (lcd.width() != PW || lcd.height() != PH) {
    Serial.printf("[E][gfx] panel is %dx%d, expected %dx%d\n", (int)lcd.width(), (int)lcd.height(), PW, PH);
    return false;
  }
#elif CONFIG_IDF_TARGET_ESP32P4
  if (lcd.width() != W || lcd.height() != H) {
    Serial.printf("[E][gfx] panel is %dx%d after rotation, expected %dx%d\n",
                  (int)lcd.width(), (int)lcd.height(), W, H);
    return false;
  }
#else
  if (lcd.width() != PW || lcd.height() != PH) {
    Serial.printf("[E][gfx] panel is %dx%d, expected %dx%d\n", (int)lcd.width(), (int)lcd.height(), PW, PH);
    return false;
  }
  for (int i = 0; i < PW; i++) s_sx[i] = i * W / PW;
  for (int i = 0; i < PH; i++) s_sy[i] = i * H / PH;
  s_fb = (uint16_t*)lcd.rgbBus()->getDMABuffer(PW * PH * 2);   // buffer 0: the one scanned out
  if (!s_fb) {
    Serial.println("[E][gfx] no frame buffer");
    return false;
  }
  // Which byte order does the frame buffer use? Draw pure red and look.
  lcd.fillRect(0, 0, 1, 1, (uint16_t)0xF800);
  s_fbSwapped = s_fb[0] == 0x00F8;
  Serial.printf("[I][gfx] frame buffer pixel for red: %04x (%s)\n", s_fb[0], s_fbSwapped ? "byte-swapped" : "native");
#endif

  // Canvas buffer allocated here, cache-line aligned, for the PPA's DMA.
  s_canvasBuf = (uint16_t*)heap_caps_aligned_alloc(64, W * H * 2, MALLOC_CAP_SPIRAM);
  if (!s_canvasBuf) {
    Serial.println("[E][gfx] canvas allocation failed");
    return false;
  }
  canvas.setColorDepth(16);
  canvas.setBuffer(s_canvasBuf, W, H, 16);

#if CONFIG_IDF_TARGET_ESP32P4
  auto* panel = static_cast<lgfx::Panel_DSI*>(lcd.getPanel());
  s_fb = panel->config_detail().buffer;
#ifdef BOARD_ELECROW_P4_10
  s_fbBytes = PW * PH * 2;
  if (s_fb) {
    memset(s_fb, 0, s_fbBytes);  // the side bars stay black
    esp_cache_msync(s_fb, s_fbBytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  }
#else
  s_fbBytes = W * H * 2;
#endif
  ppa_client_config_t pc = {};
  pc.oper_type = PPA_OPERATION_SRM;
  pc.max_pending_trans_num = 1;
  if (!s_fb || ppa_register_client(&pc, &s_ppa) != ESP_OK) {
    Serial.println("[E][gfx] PPA / frame buffer setup failed");
    return false;
  }
#endif
  canvas.setTextDatum(lgfx::textdatum_t::top_left);
  canvas.setTextWrap(false);

  bool ok = loadFont(F_SMALL, font_small, sizeof(font_small)) &&
            loadFont(F_BODY, font_body, sizeof(font_body)) &&
            loadFont(F_LARGE, font_large, sizeof(font_large)) &&
            loadFont(F_HEAD, font_head, sizeof(font_head)) &&
            loadFont(F_HUGE, font_huge, sizeof(font_huge));
  if (!ok) {
    Serial.println("[E][gfx] font load failed");
    return false;
  }
  for (int i = 0; i < F_COUNT; i++) {
    Serial.printf("[I][gfx] font %d: %dx%d px cells\n", i, s_charW[i], s_lineH[i]);
  }
  canvas.fillScreen(C_BG);
#ifdef GFX_PPA_SELFTEST
  ppaSelfTest();
#endif
  return true;
}

// Same as the Sidekick firmware's power-off: the DSI PHY's LDO stays on
// otherwise, and the IP5306 only cuts the battery once the draw is low.
void powerDown() {
  lcd.sleep();
  delay(50);
#if CONFIG_IDF_TARGET_ESP32P4
  lcd.dsiBus()->release();
#else
  lcd.setBrightness(0);
#endif
}

void setBacklightPercent(int pct) {
  pct = constrain(pct, 0, 100);
  lcd.setBrightness((uint8_t)(pct * 255 / 100));
}

static float s_cal[4] = {1, 0, 1, 0};  // ax, bx, ay, by

bool getTouch(int* x, int* y) {
  int32_t tx, ty;
  if (!lcd.getTouch(&tx, &ty)) return false;
#ifdef BOARD_ELECROW_P4_10
  tx = (tx - PX0) * 4 / 3;  // the panel's touch is 1024x600; the scenes think in 1280x800
  ty = ty * 4 / 3;
#elif !CONFIG_IDF_TARGET_ESP32P4
  tx = tx * W / PW;   // the panel's touch is 800x480; the scenes think in 1280x800
  ty = ty * H / PH;
#endif
  *x = constrain((int)(s_cal[0] * tx + s_cal[1] + 0.5f), 0, W - 1);
  *y = constrain((int)(s_cal[2] * ty + s_cal[3] + 0.5f), 0, H - 1);
  return true;
}

void setTouchCal(float ax, float bx, float ay, float by) {
  s_cal[0] = ax; s_cal[1] = bx; s_cal[2] = ay; s_cal[3] = by;
}

void getTouchCal(float* ax, float* bx, float* ay, float* by) {
  *ax = s_cal[0]; *bx = s_cal[1]; *ay = s_cal[2]; *by = s_cal[3];
}

// ---- Background ---------------------------------------------------------------
void clearRect(int x, int y, int w, int h) {
  canvas.fillRect(x, y, w, h, C_BG);
  int r = max(y, BODY_Y);
  r += (SCANLINE_STEP - (r % SCANLINE_STEP)) % SCANLINE_STEP;
  for (; r < y + h; r += SCANLINE_STEP) canvas.drawFastHLine(x, r, w, C_SCANLINE);
  dirty(x, y, w, h);
}

void clearBody() { clearRect(0, BODY_Y, W, BODY_H); }

// ---- Text ---------------------------------------------------------------------
void textN(const char* s, int n, int x, int y, FontId f, uint32_t color) {
  char buf[160];
  n = min(n, (int)sizeof(buf) - 1);
  memcpy(buf, s, n);
  buf[n] = 0;
  font(f);
  canvas.setTextColor(color);  // fg only = transparent, anti-aliased onto the canvas
  canvas.drawString(buf, x, y);
}

void text(const char* s, int x, int y, FontId f, uint32_t color) {
  textN(s, strlen(s), x, y, f, color);
}

void chromaText(const char* s, int n, int x, int y, FontId f, uint32_t color, int offset) {
  textN(s, n, x - offset, y, f, C_RED);
  textN(s, n, x + offset, y, f, C_CYAN);
  textN(s, n, x, y, f, color);
  dirty(x - offset - 2, y, n * s_charW[f] + 2 * offset + 4, s_lineH[f]);
}

// ---- Dirty rectangles -----------------------------------------------------------
struct Rect { int16_t x, y, w, h; };
static constexpr int MAX_DIRTY = 32;
static Rect s_dirty[MAX_DIRTY];
static int s_dirtyCount = 0;

void dirtyAll() {
  s_dirty[0] = {0, 0, (int16_t)W, (int16_t)H};
  s_dirtyCount = 1;
}

void dirty(int x, int y, int w, int h) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > W) w = W - x;
  if (y + h > H) h = H - y;
  if (w <= 0 || h <= 0) return;
  if (s_dirtyCount == MAX_DIRTY) {
    // Out of slots: collapse everything into one bounding box.
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    for (int i = 0; i < s_dirtyCount; i++) {
      x0 = min(x0, (int)s_dirty[i].x);
      y0 = min(y0, (int)s_dirty[i].y);
      x1 = max(x1, s_dirty[i].x + s_dirty[i].w);
      y1 = max(y1, s_dirty[i].y + s_dirty[i].h);
    }
    s_dirty[0] = {(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
    s_dirtyCount = 1;
    return;
  }
  s_dirty[s_dirtyCount++] = {(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h};
}

void present() {
#if !CONFIG_IDF_TARGET_ESP32P4
  if (!s_dirtyCount) {
    if (s_refX1 > s_refX0 && millis() - s_lastChange > 250) {
      scalePush(s_refX0, s_refY0, s_refX1 - s_refX0, s_refY1 - s_refY0, false);
      s_refX0 = W; s_refY0 = H; s_refX1 = 0; s_refY1 = 0;
    }
    return;
  }
  s_lastChange = millis();
#endif
  if (!s_dirtyCount) return;
#ifdef BIGTEXT_TIMING
  uint32_t t0 = micros();
  static int n = 0;
#endif
  for (int i = 0; i < s_dirtyCount; i++) {
    const Rect& r = s_dirty[i];
#if CONFIG_IDF_TARGET_ESP32P4
    ppaPush(r.x, r.y, r.w, r.h, SCREEN_ROTATION);
#else
    bool fast = r.w * r.h > FAST_AREA;
    scalePush(r.x, r.y, r.w, r.h, fast);
    if (fast) {
      s_refX0 = min(s_refX0, (int)r.x); s_refY0 = min(s_refY0, (int)r.y);
      s_refX1 = max(s_refX1, r.x + r.w); s_refY1 = max(s_refY1, r.y + r.h);
    }
#endif
  }
#ifdef BIGTEXT_TIMING
  uint32_t us = micros() - t0;
  if (us > 15000 && ++n % 5 == 0) Serial.printf("[D] present %d rects %u us\n", s_dirtyCount, (unsigned)us);
#endif
  s_dirtyCount = 0;
}

// ---- Glitch -------------------------------------------------------------------
void sliceShift(int y, int h, int dx, int x0, int x1) {
  x0 = constrain(x0, 0, W);
  x1 = constrain(x1, x0, W);
  int span = x1 - x0;
  if (dx == 0 || span <= 1) return;
  y = constrain(y, 0, H - 1);
  h = min(h, H - y);
  dx = constrain(dx, -(span - 1), span - 1);
  auto* buf = (uint16_t*)canvas.getBuffer();
  int n = span - abs(dx);
  for (int r = y; r < y + h; r++) {
    uint16_t* row = buf + (size_t)r * W + x0;
    if (dx > 0) {
      memmove(row + dx, row, n * 2);
      memset(row, 0, dx * 2);
    } else {
      memmove(row, row - dx, n * 2);
      memset(row + n, 0, -dx * 2);
    }
  }
}

void glitchFrame(float intensity, uint32_t tint) {
  int slices = 3 + (int)(intensity * 10);
  for (int i = 0; i < slices; i++) {
    int maxDx = 20 + (int)(intensity * 220);
    int dx = (int)rnd(10, maxDx) * (rnd(2) ? 1 : -1);
    sliceShift(rnd(0, H - 1), rnd(4, 60), dx);
  }
  const uint32_t others[] = {C_CYAN, C_GREEN, C_WHITE, C_RED};
  int bars = 2 + (int)(intensity * 6);
  for (int i = 0; i < bars; i++) {
    uint32_t c = rnd(2) ? tint : others[rnd(sizeof(others) / sizeof(others[0]))];
    int w = rnd(120, W);
    canvas.fillRect(rnd(0, W - w), rnd(0, H - 1), w, rnd(2, 24), c);
  }
  dirtyAll();
}

void fillRing(int cx, int cy, int r0, int r1, uint32_t color) {
  canvas.setColor(color);
  for (int d = -r1; d <= r1; d++) {
    int outer = (int)sqrtf((float)(r1 * r1 - d * d));
    if (abs(d) < r0) {
      int inner = (int)sqrtf((float)(r0 * r0 - d * d));
      canvas.drawFastHLine(cx - outer, cy + d, outer - inner);
      canvas.drawFastHLine(cx + inner + 1, cy + d, outer - inner);
    } else {
      canvas.drawFastHLine(cx - outer, cy + d, 2 * outer + 1);
    }
  }
}

uint32_t rnd(uint32_t lo, uint32_t hi) {
  if (hi <= lo) return lo;
  return lo + esp_random() % (hi - lo + 1);
}

}  // namespace gfx
