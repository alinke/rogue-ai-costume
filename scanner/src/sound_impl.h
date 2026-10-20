#pragma once
#ifdef SCANNER_BOARD_TAB5
#include "Tab5Audio.hpp"
#else
#include "Waveshare349Audio.hpp"
#endif
#include "config.h"
#include "sound.h"
#include <math.h>
// Sound engine implementation. Included ONCE, from main.cpp, after LGFX_Config.hpp:
// the board header keeps the I/O expander's output bits in a per-file variable,
// so the amp-enable writes must come from the same file that set up the LCD
// reset / power-hold bits, or they would clear them.

// One lock for the board's shared I2C bus (Tab5: touch, amp, battery monitor;
// Waveshare: the I/O expander, whose output bits are read-modify-written).
static SemaphoreHandle_t s_busLock = xSemaphoreCreateMutex();
struct BusLock {
  BusLock() { xSemaphoreTake(s_busLock, portMAX_DELAY); }
  ~BusLock() { xSemaphoreGive(s_busLock); }
};

namespace sound {

#ifdef SCANNER_BOARD_TAB5
using namespace tab5audio;
// Tab5Audio leaves the amp on for good; switch it per sound like the
// Waveshare does (NS4150B enable = I/O expander 0x43, output register bit 1).
static void paPower(bool on) {
  if (on) lgfx::i2c::writeRegister8(i2c_port, pi4io1_i2c_addr, 0x05, 0x02, 0xFF, 400000);
  else lgfx::i2c::writeRegister8(i2c_port, pi4io1_i2c_addr, 0x05, 0x00, 0xFD, 400000);
}
static void codecInit() {
  tab5audio::init();
  // Tab5Audio's amp-enable write sets EVERY output bit of expander 0x43
  // (writeRegister8 keeps the masked bits and ORs in 0xFF), including bit 0,
  // the antenna switch: high = the external antenna socket, which is empty.
  // Wi-Fi then dropped to -91 dBm. Back to the internal antenna.
  lgfx::i2c::writeRegister8(i2c_port, pi4io1_i2c_addr, 0x05, 0x00, 0xFE, 400000);
}
#else
using namespace waveshare349audio;
static void codecInit() { waveshare349audio::init(); }
#endif

static volatile float s_rate = 0;
static volatile int s_tone = -1;  // pending tone request
static TaskHandle_t s_task;

static void task(void*) {
  {
    BusLock l;
    codecInit();
    paPower(false);
  }
  static int16_t buf[256 * 2];
  uint32_t rng = 0x12345678;
  int clickLeft = 0;                 // samples left in the current click
  int toneType = -1, toneN = 0, toneLen = 0;
  float phase = 0;
  bool ampOn = false;
  uint32_t quietSince = millis();
  const float vol = VOLUME_PERCENT / 100.0f;
  for (;;) {
    if (s_tone >= 0) {
      toneType = s_tone;
      s_tone = -1;
      toneN = 0;
      toneLen = sample_rate * (toneType == ZAP ? 700 : toneType == FAIL ? 650 : toneType == LOCK ? 260 : 60) / 1000;
    }
    bool active = s_rate > 0 || toneType >= 0;
    if (!active) {
      if (ampOn && millis() - quietSince > 1500) {
        {
          BusLock l;
          paPower(false);
        }
        ampOn = false;
      }
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (!ampOn) {
      {
        BusLock l;
        paPower(true);
      }
      ampOn = true;
    }
    quietSince = millis();
    float pClick = s_rate / sample_rate;
    for (int i = 0; i < 256; i++) {
      float s = 0;
      // Geiger click: a short burst of decaying noise, at random times.
      rng = rng * 1664525u + 1013904223u;
      if (!clickLeft && (rng >> 8) / 16777216.0f < pClick) clickLeft = sample_rate / 450;
      if (clickLeft) {
        rng = rng * 1664525u + 1013904223u;
        float n = ((int32_t)(rng >> 16) - 32768) / 32768.0f;
        s += n * clickLeft / (float)(sample_rate / 450) * 0.9f;
        clickLeft--;
      }
      if (toneType >= 0) {
        float t = (float)toneN / toneLen, f = 1000;
        switch (toneType) {
          case ZAP: f = 2400 * powf(0.08f, t); break;                 // falling "pew"
          case FAIL: f = 140; break;                                  // buzzer
          case LOCK: f = (toneN / (sample_rate / 16)) & 1 ? 0 : 1600; break;  // beep-beep
          case BEEP: f = 1200; break;
        }
        if (f > 0) {
          phase += 2 * (float)M_PI * f / sample_rate;
          if (phase > 2 * (float)M_PI) phase -= 2 * (float)M_PI;
          float drive = toneType == FAIL ? 5.0f : 1.5f;
          s += tanhf(drive * sinf(phase)) / tanhf(drive) * 0.6f * (1 - t * 0.5f);
        }
        if (++toneN >= toneLen) toneType = -1;
      }
      int16_t v = (int16_t)(constrain(s * vol, -1.0f, 1.0f) * 30000);
      buf[i * 2] = buf[i * 2 + 1] = v;
    }
    size_t w;
    i2s_write(i2s_port, buf, sizeof(buf), &w, portMAX_DELAY);
  }
}

void begin() { xTaskCreatePinnedToCore(task, "sound", 4096, nullptr, 3, &s_task, 1); }
void setClickRate(float r) { s_rate = r; }
void play(Tone t) { s_tone = t; }

}  // namespace sound
