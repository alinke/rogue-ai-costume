#include "leds.h"

#include <driver/gpio.h>
#include <driver/rmt_tx.h>
#include <esp_heap_caps.h>
#include <math.h>

#include "config.h"
#include "gfx.h"

#ifndef LED_MAX_MA
#define LED_MAX_MA 0           // current limit for the whole chain, mA (0 = none)
#endif
#ifndef LED_MA_PER_CHANNEL
#define LED_MA_PER_CHANNEL 12  // one colour channel at full, mA (WS2811 FCOB, per pixel)
#endif
#ifndef LED_IDLE_MA
#define LED_IDLE_MA 1          // each pixel's chip, lit or not
#endif
#ifndef LED_ONBOARD_OFF_PIN
#define LED_ONBOARD_OFF_PIN 26  // the Guition's own WS2812 (LED1); -1 = none
#endif

namespace leds {

static Mode s_mode = IDLE;
static int s_number = -1;  // see matrixNumber()
static uint32_t s_modeSince = 0;

void setMode(Mode m) {
  if (m == s_mode) return;
  s_mode = m;
  s_modeSince = millis();
}

// Kept even without a strip: the beacon carries them to an external LED
// controller (../../ledlink, the Gledopto) that runs this same code.
void matrixNumber(int n) { s_number = n; }
Mode mode() { return s_mode; }
int matrixNumberNow() { return s_number; }

#if LED_STRIP_ENABLED
static const LedSegment CHAIN[] = LED_CHAIN;
static constexpr int SEGMENTS = sizeof(CHAIN) / sizeof(CHAIN[0]);

static int s_total = 0;
static uint8_t* s_px = nullptr;  // 3 bytes per LED in wire order, internal RAM
static rmt_channel_handle_t s_chan = nullptr;
static rmt_encoder_handle_t s_enc = nullptr;
static bool s_busy = false;
static uint32_t s_nextFrame = 0;

static void setPixel(int i, uint32_t rgb, int pct = LED_BRIGHTNESS_PERCENT, int order = LED_COLOR_ORDER) {
  int k = pct * 255 / 100;
  uint8_t r = ((rgb >> 16) & 255) * k / 255, g = ((rgb >> 8) & 255) * k / 255, b = (rgb & 255) * k / 255;
  uint8_t* p = s_px + i * 3;
  switch (order) {  // the byte order the LED chip expects
    case ORDER_GRB: p[0] = g; p[1] = r; p[2] = b; break;
    case ORDER_BRG: p[0] = b; p[1] = r; p[2] = g; break;
    case ORDER_RBG: p[0] = r; p[1] = b; p[2] = g; break;
    case ORDER_GBR: p[0] = g; p[1] = b; p[2] = r; break;
    case ORDER_BGR: p[0] = b; p[1] = g; p[2] = r; break;
    default: p[0] = r; p[1] = g; p[2] = b; break;
  }
}

static uint32_t scale(uint32_t rgb, float k) {
  k = constrain(k, 0.0f, 1.0f);
  return ((uint32_t)(((rgb >> 16) & 255) * k) << 16) | ((uint32_t)(((rgb >> 8) & 255) * k) << 8) |
         (uint32_t)((rgb & 255) * k);
}

void begin() {
#if LED_PIN_FINDER
  // Bring-up: for 20 s, GPIO37 (connector TX) and GPIO38 (connector RX) swap
  // between 3.3 V and 0 V every 5 s, so a multimeter shows which wire is which
  // and that the board really drives it. The log stops while this runs.
  Serial.println("[I][leds] pin finder: GPIO37 high / GPIO38 low, swapping every 5 s for 20 s");
  Serial.flush();
  pinMode(37, OUTPUT);
  pinMode(38, OUTPUT);
  for (int i = 0; i < 4; i++) {
    digitalWrite(37, i % 2 == 0);
    digitalWrite(38, i % 2 == 1);
    delay(5000);
  }
  digitalWrite(37, LOW);
  digitalWrite(38, LOW);
#endif
  for (const auto& seg : CHAIN) s_total += seg.count;
  s_px = (uint8_t*)heap_caps_calloc(s_total, 3, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Serial.printf("[I][leds] %d LEDs in %d parts on GPIO%d -- log output stops here if that's the TX pin\n",
                s_total, SEGMENTS, LED_DATA_PIN);
  Serial.flush();
  if (!s_px) return;
  // The RMT peripheral holds only 48 symbols (2 LEDs) per block and refills
  // from an interrupt; with the display and camera busy the refills come late
  // and only the first LEDs get good data. DMA streams the whole strip instead.
  rmt_tx_channel_config_t cfg = {};
  cfg.gpio_num = (gpio_num_t)LED_DATA_PIN;
  cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  cfg.resolution_hz = 10000000;  // 0.1 us ticks
  cfg.mem_block_symbols = 1024;
  cfg.trans_queue_depth = 2;
  cfg.flags.with_dma = 1;
  esp_err_t err = rmt_new_tx_channel(&cfg, &s_chan);
  bool dma = err == ESP_OK;
  if (!dma) {  // no DMA channel: the biggest plain buffer instead
    cfg.flags.with_dma = 0;
    cfg.mem_block_symbols = 192;
    err = rmt_new_tx_channel(&cfg, &s_chan);
  }
  // WS2812: 0 = 0.4 us high / 0.85 us low, 1 = 0.8 us high / 0.45 us low.
  rmt_bytes_encoder_config_t enc = {};
  enc.bit0 = {4, 1, 9, 0};
  enc.bit1 = {8, 1, 5, 0};
  enc.flags.msb_first = 1;
  if (err == ESP_OK) err = rmt_new_bytes_encoder(&enc, &s_enc);
  if (err == ESP_OK) err = rmt_enable(s_chan);
  // Strongest drive: the connector's TX line also feeds the USB-serial chip.
  if (err == ESP_OK) gpio_set_drive_capability((gpio_num_t)LED_DATA_PIN, GPIO_DRIVE_CAP_3);
  Serial.printf("[I][leds] RMT %s, %s\n", dma ? "DMA" : "no DMA", esp_err_to_name(err));
  // The board's own WS2812 (LED1, GPIO26) keeps whatever colour it last got
  // until power is removed: send it one "off" so it doesn't waste battery.
#if LED_ONBOARD_OFF_PIN >= 0
  rmt_tx_channel_config_t bc = cfg;
  bc.gpio_num = (gpio_num_t)LED_ONBOARD_OFF_PIN;
  bc.flags.with_dma = 0;
  bc.mem_block_symbols = 48;
  rmt_channel_handle_t board = nullptr;
  if (s_enc && rmt_new_tx_channel(&bc, &board) == ESP_OK) {
    static const uint8_t off[3] = {0, 0, 0};
    rmt_transmit_config_t tx = {};
    if (rmt_enable(board) == ESP_OK && rmt_transmit(board, s_enc, off, 3, &tx) == ESP_OK)
      rmt_tx_wait_all_done(board, 100);
    rmt_disable(board);
    rmt_del_channel(board);
    pinMode(LED_ONBOARD_OFF_PIN, OUTPUT);  // hold its data line low so noise can't relight it
    digitalWrite(LED_ONBOARD_OFF_PIN, LOW);
  }
#endif
  if (err != ESP_OK) s_chan = nullptr;
}

// Colour of an LED at position u along its part (0 = fingertips / start,
// 1 = collar / end) at time t (s since the mode started).
static uint32_t partColor(LedPart type, float u, float t) {
  switch (s_mode) {
    case IDLE: {
      // Pulses of green flowing toward the collar.
      float ph = fmodf(u * 3.0f - t * 0.9f, 1.0f);
      if (ph < 0) ph += 1;
      float k = (type == LEDS_TRIM ? 0.05f : 0.08f) + 0.92f * powf(1.0f - ph, 6);
      return scale(gfx::C_GREEN, k);
    }
    case ALERT: return ((int)(t * 8) & 1) ? gfx::C_RED : 0;
    case SCAN: {
      float pos = 0.5f + 0.5f * sinf(t * 3.0f);
      return fabsf(u - pos) < 0.08f ? gfx::C_CYAN : scale(gfx::C_GREEN, 0.06f);
    }
    case BREAKOUT: {
      uint32_t r = gfx::rnd(100);
      return r < 8 ? gfx::C_RED : scale(gfx::C_GREEN, r < 60 ? 0.9f : 0.15f);
    }
    case MIDNIGHT: {
      float ph = fmodf(u * 5.0f + t * 3.0f, 1.0f);
      return scale(gfx::C_RED, ph < 0.3f ? 1.0f : 0.05f);
    }
    case NOISE: return scale(gfx::C_AMBER, fmodf(t * 4, 1.0f) < 0.5f ? 1.0f : 0.2f);
    case OFF: return 0;
    case WARY: {
      float ph = fmodf(u * 3.0f - t * 1.4f, 1.0f);
      if (ph < 0) ph += 1;
      float k = 0.08f + 0.92f * powf(1.0f - ph, 6);
      return scale(gfx::rnd(100) < 12 ? gfx::C_AMBER : gfx::C_GREEN, k);
    }
    case CONTAIN: {  // red and cyan (the beam's colour) -- no white: it's the heaviest load
      int f = (int)(t * 14);
      return (f & 1) ? ((f & 2) ? gfx::C_CYAN : gfx::C_RED) : 0;
    }
  }
  return 0;
}

static uint32_t eyeColor(float t, float tAbs) {
  if (s_mode == OFF) return 0;
  if (s_mode == ALERT || s_mode == MIDNIGHT || s_mode == CONTAIN) return ((int)(t * 8) & 1) ? gfx::C_RED : 0;
  return scale(gfx::C_RED, 0.25f + 0.75f * (0.5f + 0.5f * sinf(tAbs * 2.4f)));
}

// ---- The 8x8 matrix: the watching eye ----------------------------------------------
// Panel index -> (x, y) as seen from the front, y = 0 at the top.
static void matrixXY(int k, int* x, int* y) {
  int r = k / 8, c = k % 8;
#if MATRIX_SERPENTINE
  if (r & 1) c = 7 - c;  // every other row runs back
#endif
#if MATRIX_MIRROR
  c = 7 - c;
#endif
  switch (MATRIX_ROTATION & 3) {
    case 0: *x = c; *y = r; break;
    case 1: *x = 7 - r; *y = c; break;
    case 2: *x = 7 - c; *y = 7 - r; break;
    default: *x = r; *y = 7 - c; break;
  }
}


// 5x7 digits (bit 4 = left column), and a narrow "10" for 8 columns.
static const uint8_t DIGITS[10][7] = {
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},   {14, 17, 1, 2, 4, 8, 31},
    {31, 2, 4, 2, 1, 17, 14},     {2, 6, 10, 18, 31, 2, 2},  {31, 16, 30, 1, 1, 17, 14},
    {6, 8, 16, 30, 17, 17, 14},   {31, 1, 2, 4, 8, 8, 8},    {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 2, 12}};
static const uint8_t TEN[7] = {0x27, 0x65, 0x25, 0x25, 0x25, 0x25, 0x77};  // "10", 7 columns

static bool numberPixel(int n, int x, int y) {
  if (y < 0 || y >= 7) return false;
  if (n == 10) return x < 7 && (TEN[y] >> (6 - x)) & 1;
  if (n < 0 || n > 9 || x < 1 || x > 5) return false;
  return (DIGITS[n][y] >> (5 - x)) & 1;
}

// The eye's state, moved on once a frame.
static float s_lookX = 0, s_lookTo = 0, s_open = 1;
static uint32_t s_nextLook = 0, s_blinkAt = 0;

static void eyeStep(uint32_t now) {
  bool wary = s_mode == WARY;
  if ((int32_t)(now - s_nextLook) >= 0) {  // a new place to look
    s_lookTo = (gfx::rnd(100) < 25) ? 0 : ((int)gfx::rnd(200) - 100) / 100.0f;
    s_nextLook = now + (wary ? gfx::rnd(400, 1200) : gfx::rnd(1500, 4000));
  }
  s_lookX += (s_lookTo - s_lookX) * (wary ? 0.35f : 0.15f);
  if ((int32_t)(now - s_blinkAt) >= 0 && now - s_blinkAt > 220) s_blinkAt = now + gfx::rnd(2500, 7000);
  int32_t b = (int32_t)(now - s_blinkAt);
  float lid = (b >= 0 && b < 220) ? fabsf(b - 110) / 110.0f : 1.0f;  // closes and opens in 0.22 s
  s_open = lid * (wary ? 0.6f : (s_mode == ALERT || s_mode == MIDNIGHT) ? 1.15f : 1.0f);
}

// The eye at (x, y): an almond of dim red, a bright iris, a dark pupil.
static uint32_t eyePixel(int x, int y, uint32_t iris) {
  float fx = x - 3.5f, fy = y - 3.5f;
  float h = s_open * 3.7f * (1.0f - (fx * fx) / 18.0f);
  if (h <= 0.2f || fabsf(fy) > h) return 0;
  float px = s_lookX * 1.7f;
  float d = sqrtf((fx - px) * (fx - px) + fy * fy);
  if (d < 0.75f) return scale(iris, 0.05f);     // pupil
  if (d < 2.1f) return iris;                    // iris
  return scale(gfx::C_RED, 0.12f);              // the rest of the eye
}

static uint32_t matrixColor(int x, int y, float t) {
  if (s_mode == OFF) return 0;
  if (s_number >= 0) return numberPixel(s_number, x, y) ? gfx::C_RED : 0;
  switch (s_mode) {
    case SCAN: {  // a cyan line sweeping over a dim eye
      int line = (int)(3.5f + 3.6f * sinf(t * 3.0f) + 0.5f);
      return y == line ? gfx::C_CYAN : scale(eyePixel(x, y, gfx::C_RED), 0.3f);
    }
    case BREAKOUT: {
      uint32_t r = gfx::rnd(100);
      return r < 10 ? gfx::C_RED : r < 30 ? scale(gfx::C_GREEN, 0.6f) : eyePixel(x, y, gfx::C_RED);
    }
    case CONTAIN: {  // a flashing X: it's being hit
      int f = (int)(t * 14);
      bool on = (x == y || x == 7 - y);
      return on && (f & 1) ? ((f & 2) ? gfx::C_CYAN : gfx::C_RED) : 0;
    }
    case ALERT:
    case MIDNIGHT: return ((int)(t * 8) & 1) ? eyePixel(x, y, gfx::C_RED) : scale(eyePixel(x, y, gfx::C_RED), 0.2f);
    case NOISE: return eyePixel(x, y, gfx::C_AMBER);
    case WARY: return eyePixel(x, y, gfx::rnd(100) < 6 ? gfx::C_AMBER : gfx::C_RED);
    default: return eyePixel(x, y, gfx::C_RED);
  }
}

void tick(uint32_t now) {
  if (!s_chan || (int32_t)(now - s_nextFrame) < 0) return;
  if (s_busy && rmt_tx_wait_all_done(s_chan, 0) != ESP_OK) return;  // last frame still going out
  s_nextFrame = now + 33;
  float t = (now - s_modeSince) / 1000.0f, tAbs = now / 1000.0f;
  int i = 0;
#if LED_INDEX_TEST
  for (; i < s_total; i++) setPixel(i, i % 50 == 0 ? 0xFF0000 : i % 10 == 0 ? 0xFFFFFF : 0);
#elif LED_TEST_COLOR
  for (; i < s_total; i++) setPixel(i, LED_TEST_COLOR);
#else
  eyeStep(now);
  for (const auto& seg : CHAIN) {
    for (int k = 0; k < seg.count; k++, i++) {
      if (seg.type == LEDS_DARK) {
        setPixel(i, 0);
        continue;
      }
      if (seg.type == LEDS_MATRIX) {
        int x, y;
        matrixXY(k, &x, &y);
#if MATRIX_ORIENT_TEST
        // An "F" in the top-left, so the picture's turn/flip is easy to see.
        bool f = x == 1 || (y == 1 && x < 6) || (y == 4 && x < 4);
        setPixel(i, f && y >= 1 && y <= 6 ? gfx::C_GREEN : 0, MATRIX_BRIGHTNESS_PERCENT, MATRIX_COLOR_ORDER);
#else
        setPixel(i, matrixColor(x, y, t), MATRIX_BRIGHTNESS_PERCENT, MATRIX_COLOR_ORDER);
#endif
        continue;
      }
      float u = seg.count > 1 ? (float)k / (seg.count - 1) : 0;
      if (seg.reversed) u = 1 - u;
      setPixel(i, seg.type == LEDS_EYE ? eyeColor(t, tAbs) : partColor(seg.type, u, t));
    }
  }
#endif
#if LED_MAX_MA > 0
  // Current limit (like WLED's): estimate this frame's draw and dim the whole
  // frame just enough to stay under LED_MAX_MA -- a full-chain white strobe
  // can pull more than a USB port or power bank gives, and the controller
  // browns out.
  uint32_t sum = 0;
  for (int k = 0; k < s_total * 3; k++) sum += s_px[k];
  uint32_t idle = (uint32_t)s_total * LED_IDLE_MA;
  uint32_t ma = idle + sum * LED_MA_PER_CHANNEL / 255;
  if (ma > LED_MAX_MA && sum) {
    uint32_t room = LED_MAX_MA > idle ? LED_MAX_MA - idle : 0;
    uint32_t scale = room * 255 / (sum * LED_MA_PER_CHANNEL / 255 + 1);  // 0..255
    for (int k = 0; k < s_total * 3; k++) s_px[k] = s_px[k] * scale / 255;
  }
#endif
  rmt_transmit_config_t tx = {};
  s_busy = rmt_transmit(s_chan, s_enc, s_px, s_total * 3, &tx) == ESP_OK;
}

#else  // no strip

void begin() {}
void tick(uint32_t) {}

#endif

}  // namespace leds
