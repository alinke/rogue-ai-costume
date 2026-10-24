#include "leds.h"

#include <driver/gpio.h>
#include <driver/rmt_tx.h>
#include <esp_heap_caps.h>
#include <math.h>

#include "config.h"
#include "gfx.h"

namespace leds {

static Mode s_mode = IDLE;
static uint32_t s_modeSince = 0;

void setMode(Mode m) {
  if (m == s_mode) return;
  s_mode = m;
  s_modeSince = millis();
}

#if LED_STRIP_ENABLED
static const LedSegment CHAIN[] = LED_CHAIN;
static constexpr int SEGMENTS = sizeof(CHAIN) / sizeof(CHAIN[0]);

static int s_total = 0;
static uint8_t* s_px = nullptr;  // 3 bytes per LED in wire order, internal RAM
static rmt_channel_handle_t s_chan = nullptr;
static rmt_encoder_handle_t s_enc = nullptr;
static bool s_busy = false;
static uint32_t s_nextFrame = 0;

static void setPixel(int i, uint32_t rgb) {
  int k = LED_BRIGHTNESS_PERCENT * 255 / 100;
  uint8_t r = ((rgb >> 16) & 255) * k / 255, g = ((rgb >> 8) & 255) * k / 255, b = (rgb & 255) * k / 255;
  uint8_t* p = s_px + i * 3;
#if LED_STRIP_COLOR_ORDER_GRB
  p[0] = g; p[1] = r; p[2] = b;
#else
  p[0] = r; p[1] = g; p[2] = b;
#endif
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
  rmt_tx_channel_config_t bc = cfg;
  bc.gpio_num = GPIO_NUM_26;
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
    pinMode(26, OUTPUT);  // hold its data line low so noise can't relight it
    digitalWrite(26, LOW);
  }
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
    case CONTAIN: {
      int f = (int)(t * 14);
      return (f & 1) ? ((f & 2) ? gfx::C_WHITE : gfx::C_RED) : 0;
    }
  }
  return 0;
}

static uint32_t eyeColor(float t, float tAbs) {
  if (s_mode == OFF) return 0;
  if (s_mode == ALERT || s_mode == MIDNIGHT || s_mode == CONTAIN) return ((int)(t * 8) & 1) ? gfx::C_RED : 0;
  return scale(gfx::C_RED, 0.25f + 0.75f * (0.5f + 0.5f * sinf(tAbs * 2.4f)));
}

void tick(uint32_t now) {
  if (!s_chan || (int32_t)(now - s_nextFrame) < 0) return;
  if (s_busy && rmt_tx_wait_all_done(s_chan, 0) != ESP_OK) return;  // last frame still going out
  s_nextFrame = now + 33;
  float t = (now - s_modeSince) / 1000.0f, tAbs = now / 1000.0f;
  int i = 0;
#if LED_TEST_COLOR
  for (; i < s_total; i++) setPixel(i, LED_TEST_COLOR);
#else
  for (const auto& seg : CHAIN) {
    for (int k = 0; k < seg.count; k++, i++) {
      float u = seg.count > 1 ? (float)k / (seg.count - 1) : 0;
      if (seg.reversed) u = 1 - u;
      setPixel(i, seg.type == LEDS_EYE ? eyeColor(t, tAbs) : partColor(seg.type, u, t));
    }
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
