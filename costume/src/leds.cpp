#include "leds.h"

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
static rmt_data_t* s_bits = nullptr;  // 24 RMT symbols per LED, allocated once
static bool s_ok = false;
static uint32_t s_nextFrame = 0;

// WS2812 timing at a 10 MHz RMT tick (0.1 us): 0 = 0.4 us high / 0.85 us low,
// 1 = 0.8 us high / 0.45 us low.
static inline void encode(rmt_data_t* out, uint8_t byte) {
  for (int b = 7; b >= 0; b--, out++) {
    bool one = byte & (1 << b);
    out->level0 = 1;
    out->duration0 = one ? 8 : 4;
    out->level1 = 0;
    out->duration1 = one ? 5 : 9;
  }
}

static void setPixel(int i, uint32_t rgb) {
  int k = LED_BRIGHTNESS_PERCENT * 255 / 100;
  uint8_t r = ((rgb >> 16) & 255) * k / 255, g = ((rgb >> 8) & 255) * k / 255, b = (rgb & 255) * k / 255;
  rmt_data_t* p = s_bits + i * 24;
#if LED_STRIP_COLOR_ORDER_GRB
  encode(p, g); encode(p + 8, r); encode(p + 16, b);
#else
  encode(p, r); encode(p + 8, g); encode(p + 16, b);
#endif
}

static uint32_t scale(uint32_t rgb, float k) {
  k = constrain(k, 0.0f, 1.0f);
  return ((uint32_t)(((rgb >> 16) & 255) * k) << 16) | ((uint32_t)(((rgb >> 8) & 255) * k) << 8) |
         (uint32_t)((rgb & 255) * k);
}

void begin() {
  for (const auto& seg : CHAIN) s_total += seg.count;
  s_bits = (rmt_data_t*)heap_caps_malloc(s_total * 24 * sizeof(rmt_data_t), MALLOC_CAP_INTERNAL);
  Serial.printf("[I][leds] %d LEDs in %d parts on GPIO%d -- log output stops here if that's the TX pin\n",
                s_total, SEGMENTS, LED_DATA_PIN);
  Serial.flush();
  // Taking the pin for the LED driver detaches the UART's TX from it (the log
  // goes nowhere); RX stays, so serial commands keep working.
  s_ok = s_bits && rmtInit(LED_DATA_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);
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
  if (!s_ok || (int32_t)(now - s_nextFrame) < 0 || !rmtTransmitCompleted(LED_DATA_PIN)) return;
  s_nextFrame = now + 33;
  float t = (now - s_modeSince) / 1000.0f, tAbs = now / 1000.0f;
  int i = 0;
  for (const auto& seg : CHAIN) {
    for (int k = 0; k < seg.count; k++, i++) {
      float u = seg.count > 1 ? (float)k / (seg.count - 1) : 0;
      if (seg.reversed) u = 1 - u;
      setPixel(i, seg.type == LEDS_EYE ? eyeColor(t, tAbs) : partColor(seg.type, u, t));
    }
  }
  rmtWriteAsync(LED_DATA_PIN, s_bits, s_total * 24);
}

#else  // no strip

void begin() {}
void tick(uint32_t) {}

#endif

}  // namespace leds
