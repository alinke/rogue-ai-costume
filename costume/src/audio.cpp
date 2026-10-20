#include "audio.h"

#include <math.h>

#include "config.h"

#if SOUND_ENABLED

#include "JC8012Audio.hpp"  // ES8311 + I2S bring-up, unmodified from the Sidekick firmware
#include "voice/voice_clips.h"

namespace audio {

using namespace jc8012audio;

static constexpr int CHUNK = 256;  // stereo frames per I2S transfer (~5.8 ms)
static constexpr gpio_num_t PIN_MIC_DATA = GPIO_NUM_11;  // ES8311 ADC -> P4 (I2S DIN)

struct Cmd { uint8_t kind; uint8_t fx; const VoiceClip* clip; };
enum : uint8_t { CMD_FX, CMD_VOICE };

static QueueHandle_t s_queue = nullptr;
static volatile bool s_muted = false;
static volatile int s_sirenVol = SIREN_VOLUME_PERCENT, s_voiceVol = VOICE_VOLUME_PERCENT;
static volatile bool s_sirenOn = true, s_voiceOn = true;
static volatile bool s_busy = false;
static volatile uint32_t s_quietSince = 0;  // when the last sound finished
static bool s_mic = false;           // full-duplex I2S with the mic running
static volatile bool s_micAlive = false;

// ---- Effects channel ------------------------------------------------------------
struct Tone {
  bool active = false;
  uint8_t fx = 0;
  uint32_t n = 0, total = 0;
  float phase = 0, swing = 0;
  uint32_t noise = 12345;
};
static Tone s_tone;

static void startFx(uint8_t f) {
  s_tone = Tone();
  s_tone.active = true;
  s_tone.fx = f;
  uint32_t ms = 0;
  switch (f) {
    case FX_SIREN: ms = SIREN_MS; break;
    case FX_SIREN_PREVIEW: ms = 700; break;
    case FX_KEY: ms = 45; break;
    case FX_DENIED: ms = 450; break;
    case FX_OK: ms = 420; break;
    case FX_POWER_DOWN: ms = 1400; break;
    case FX_SHUTTER: ms = 70; break;
  }
  s_tone.total = sample_rate * ms / 1000;
}

static float toneSample() {
  Tone& t = s_tone;
  if (!t.active) return 0;
  if (t.n >= t.total) {
    t.active = false;
    return 0;
  }
  const float sr = (float)sample_rate;
  float pos = (float)t.n / t.total;  // 0..1
  float env = 1.0f, f = 440, s = 0;
  uint32_t fadeIn = sample_rate / 200, fadeOut = sample_rate / 40;
  switch (t.fx) {
    case FX_SIREN:
    case FX_SIREN_PREVIEW: {
      // Yelp: 650 <-> 1450 Hz three times a second, soft-clipped sine.
      t.swing += 2.0f * (float)M_PI * 3.0f / sr;
      f = 650.0f + 800.0f * (0.5f - 0.5f * cosf(t.swing));
      fadeOut = sample_rate / 6;
      break;
    }
    case FX_KEY: f = 1800; break;
    case FX_DENIED: f = 140; break;
    case FX_OK: f = pos < 0.45f ? 880 : 1320; break;
    case FX_POWER_DOWN: f = 1100.0f * powf(0.04f, pos); fadeOut = sample_rate / 3; break;
    case FX_SHUTTER: break;
  }
  if (t.n < fadeIn) env = (float)t.n / fadeIn;
  else if (t.n > t.total - fadeOut) env = (float)(t.total - t.n) / fadeOut;
  if (t.fx == FX_SHUTTER) {
    t.noise = t.noise * 1664525u + 1013904223u;  // a short burst of noise
    s = ((int32_t)(t.noise >> 16) - 32768) / 32768.0f * (1.0f - pos);
  } else {
    t.phase += 2.0f * (float)M_PI * f / sr;
    if (t.phase > 2.0f * (float)M_PI) t.phase -= 2.0f * (float)M_PI;
    float drive = t.fx == FX_DENIED ? 6.0f : 2.5f;  // the buzzer is near-square
    s = tanhf(drive * sinf(t.phase)) / tanhf(drive);
  }
  t.n++;
  bool siren = t.fx == FX_SIREN || t.fx == FX_SIREN_PREVIEW;
  float vol = siren ? s_sirenVol / 100.0f : FX_VOLUME_PERCENT / 100.0f;
  return s * env * vol * 0.8f;
}

// ---- Voice channel (8-bit mu-law, 16 kHz, resampled to 44.1 kHz) ----------------
static const VoiceClip* s_clip = nullptr;
static float s_clipPos = 0;
static const VoiceClip* s_pending[4];  // lines waiting their turn
static int s_pendingN = 0;

static inline float mulaw(uint8_t u) {
  u = ~u;
  int t = ((u & 0x0F) << 3) + 0x84;
  t <<= (u & 0x70) >> 4;
  int v = (u & 0x80) ? (0x84 - t) : (t - 0x84);
  return v / 32768.0f;
}

static float voiceSample() {
  if (!s_clip) return 0;
  uint32_t i = (uint32_t)s_clipPos;
  if (i + 1 >= s_clip->len) {
    s_clip = nullptr;
    if (s_pendingN) {  // next queued line
      s_clip = s_pending[0];
      s_clipPos = 0;
      memmove(s_pending, s_pending + 1, --s_pendingN * sizeof(s_pending[0]));
    }
    return 0;
  }
  float frac = s_clipPos - i;
  float s = mulaw(s_clip->data[i]) * (1 - frac) + mulaw(s_clip->data[i + 1]) * frac;
  s_clipPos += (float)VOICE_SAMPLE_RATE / sample_rate;
  return s * s_voiceVol / 100.0f;
}

// ---- Microphone -------------------------------------------------------------------
static volatile float s_levelDb = -90, s_baselineDb = -60;
static int16_t s_wave[512];
static volatile int s_waveHead = 0;
static float s_windowSq = 0;
static int s_windowN = 0;
static int s_peakAbs = 0;
static uint32_t s_micCheckEnd = 0;
static volatile uint32_t s_onsets = 0, s_onsetMs = 0;
static volatile float s_onsetDb = -90, s_prevDb = -90;

bool busy();

static void micProcess(const int16_t* frames, int n) {
  for (int i = 0; i < n; i++) {
    int16_t v = frames[i * 2];  // left channel = mic
    s_windowSq += (float)v * v;
    s_peakAbs = max(s_peakAbs, (int)abs(v));
    if ((i & 1) == 0) {
      s_wave[s_waveHead] = v;
      s_waveHead = (s_waveHead + 1) & 511;
    }
  }
  s_windowN += n;
  if (s_windowN >= (int)sample_rate / 20) {  // 50 ms windows
    float rms = sqrtf(s_windowSq / s_windowN) / 32768.0f;
    float db = 20.0f * log10f(rms + 1e-5f);
    s_levelDb = db;
    // Onset: a sharp jump well above the room after a quieter window.
    if (!busy() && db - s_baselineDb > CLAP_ONSET_DB && s_prevDb - s_baselineDb < CLAP_ONSET_DB / 2) {
      s_onsetMs = millis();
      s_onsetDb = db;
      s_onsets = s_onsets + 1;
    }
    s_prevDb = db;
    if (!s_busy) s_baselineDb += (db - s_baselineDb) * 0.005f;  // ~10 s average
    s_windowSq = 0;
    s_windowN = 0;
  }
  if (s_micCheckEnd && millis() > s_micCheckEnd) {
    s_micAlive = s_peakAbs > 8;  // a dead input reads (almost) all zeros
    Serial.printf("[I][audio] microphone %s (peak %d)\n", s_micAlive ? "found" : "not found",
                  s_peakAbs);
    s_micCheckEnd = 0;
  }
}

// ---- Task ---------------------------------------------------------------------------
static void handle(const Cmd& c) {
  if (c.kind == CMD_FX) {
    startFx(c.fx);
  } else if (!s_clip) {
    s_clip = c.clip;
    s_clipPos = 0;
  } else if (s_pendingN < 4) {
    s_pending[s_pendingN++] = c.clip;  // speak after the current line
  }
}

static void task(void*) {
  static int16_t out[CHUNK * 2], in[CHUNK * 2];
  bool ampOn = false;
  for (;;) {
    Cmd c;
    // Idle: with the mic running keep reading it; otherwise sleep until a sound.
    if (!s_tone.active && !s_clip) {
      if (ampOn) {
        memset(out, 0, sizeof(out));
        size_t w;
        i2s_write(i2s_port, out, sizeof(out), &w, portMAX_DELAY);  // flush silence first
        paPower(false);
        ampOn = false;
        if (!s_mic) i2s_stop(i2s_port);
      }
      if (s_busy) s_quietSince = millis();
      s_busy = false;
      if (s_mic) {
        size_t r = 0;
        i2s_read(i2s_port, in, sizeof(in), &r, pdMS_TO_TICKS(50));
        micProcess(in, r / 4);
        while (xQueueReceive(s_queue, &c, 0)) handle(c);
        continue;
      }
      if (!xQueueReceive(s_queue, &c, portMAX_DELAY)) continue;
      handle(c);
    }
    while (xQueueReceive(s_queue, &c, 0)) handle(c);
    if (!ampOn) {
      if (!s_mic) {
        i2s_zero_dma_buffer(i2s_port);
        i2s_start(i2s_port);
      }
      paPower(true);
      vTaskDelay(pdMS_TO_TICKS(10));  // let the amp settle (no pop)
      ampOn = true;
    }
    s_busy = true;
    for (int i = 0; i < CHUNK; i++) {
      float s = toneSample() + voiceSample();
      int16_t v = (int16_t)(constrain(s, -1.0f, 1.0f) * 32000.0f);
      out[i * 2] = v;
      out[i * 2 + 1] = v;
    }
    size_t w;
    i2s_write(i2s_port, out, sizeof(out), &w, portMAX_DELAY);
    if (s_mic) {
      size_t r = 0;
      i2s_read(i2s_port, in, sizeof(in), &r, 0);  // keep the RX side drained
      micProcess(in, r / 4);
    }
  }
}

void begin() {
  jc8012audio::init();  // codec + I2S (shares the touch controller's I2C bus; boot only)
  paPower(false);
#if MIC_ENABLED
  // Re-install the port full-duplex so the codec's ADC (mic) comes in on DIN.
  i2s_driver_uninstall(i2s_port);
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate = sample_rate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_desc_num = 4;
  cfg.dma_frame_num = CHUNK;
  cfg.tx_desc_auto_clear = true;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = pin_mclk;
  pins.bck_io_num = pin_bck;
  pins.ws_io_num = pin_ws;
  pins.data_out_num = pin_dout;
  pins.data_in_num = PIN_MIC_DATA;
  if (i2s_driver_install(i2s_port, &cfg, 0, nullptr) == ESP_OK && i2s_set_pin(i2s_port, &pins) == ESP_OK) {
    s_mic = true;
    s_micCheckEnd = millis() + 3000;
  } else {
    Serial.println("[E][audio] full-duplex I2S failed; mic off");
    i2s_driver_install(i2s_port, &cfg, 0, nullptr);
  }
#endif
  if (!s_mic) i2s_stop(i2s_port);
  s_queue = xQueueCreate(8, sizeof(Cmd));
  xTaskCreatePinnedToCore(task, "audio", 6144, nullptr, 3, nullptr, 1);
  Serial.printf("[I][audio] ready, %d voice clips\n", (int)VOICE_CLIP_COUNT);
}

void fx(Fx f) {
  if (s_muted || !s_queue) return;
  if ((f == FX_SIREN || f == FX_SIREN_PREVIEW) && !s_sirenOn) return;
  Cmd c = {CMD_FX, (uint8_t)f, nullptr};
  xQueueSend(s_queue, &c, 0);
}

void siren() { fx(FX_SIREN); }

void say(const char* text) {
  if (s_muted || !s_voiceOn || !s_queue || !text) return;
  for (int i = 0; i < VOICE_CLIP_COUNT; i++) {
    if (strcmp(VOICE_CLIPS[i].text, text) == 0) {
      Cmd c = {CMD_VOICE, 0, &VOICE_CLIPS[i]};
      xQueueSend(s_queue, &c, 0);
      return;
    }
  }
}

void setMuted(bool m) { s_muted = m; }
void setSirenVolume(int p) { s_sirenVol = constrain(p, 0, 100); }
void setVoiceVolume(int p) { s_voiceVol = constrain(p, 0, 100); }
void setSirenEnabled(bool on) { s_sirenOn = on; }
void setVoiceEnabled(bool on) { s_voiceOn = on; }
bool muted() { return s_muted; }
// Busy while playing and for 1.5 s after, so the mic doesn't hear the tail of
// our own voice/siren (and its echo off the room) as a noise spike.
bool busy() { return s_busy || millis() - s_quietSince < 1500; }

bool micAvailable() { return s_micAlive; }
uint32_t onsetCount() { return s_onsets; }
uint32_t lastOnsetMs() { return s_onsetMs; }
float lastOnsetDb() { return s_onsetDb; }
float micLevelDb() { return s_levelDb; }
float micBaselineDb() { return s_baselineDb; }
void micWaveform(int16_t* outp, int n) {
  n = min(n, 512);
  int head = s_waveHead;
  for (int i = 0; i < n; i++) outp[i] = s_wave[(head - n + i + 512) & 511];
}

}  // namespace audio

#else

namespace audio {
void begin() {}
void fx(Fx) {}
void say(const char*) {}
void siren() {}
void setMuted(bool) {}
void setSirenVolume(int) {}
void setVoiceVolume(int) {}
void setSirenEnabled(bool) {}
void setVoiceEnabled(bool) {}
bool muted() { return true; }
bool busy() { return false; }
bool micAvailable() { return false; }
uint32_t onsetCount() { return 0; }
uint32_t lastOnsetMs() { return 0; }
float lastOnsetDb() { return -90; }
float micLevelDb() { return -90; }
float micBaselineDb() { return -90; }
void micWaveform(int16_t* o, int n) { memset(o, 0, n * 2); }
}  // namespace audio

#endif
