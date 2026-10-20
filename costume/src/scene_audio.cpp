// "INTERCEPTING HUMAN AUDIO...": the party, live, as an oscilloscope trace
// with a level meter. Only when the microphone works.
#include "audio.h"
#include "messages.h"
#include "scenes.h"
#include "term.h"

using namespace gfx;

namespace {

constexpr int AX = MARGIN_X, AW = W - 2 * MARGIN_X - 90;  // trace area (meter on the right)
constexpr int AY = BODY_Y + 110, AH = 440;
constexpr int MX = W - MARGIN_X - 60, MW = 60;            // level meter
constexpr int SAMPLES = 400;
constexpr uint32_t DURATION_MS = 10000, FRAME_MS = 50;

term::Typer title, note;
term::Bag noteBag;
uint32_t endAt, nextFrame;
float gain = 4.0f;
int16_t wave[SAMPLES];

void drawFrame() {
  clearRect(AX, AY, AW, AH);
  int mid = AY + AH / 2;
  canvas.drawFastHLine(AX, mid, AW, C_DIM);
  audio::micWaveform(wave, SAMPLES);
  int peak = 1;
  for (int i = 0; i < SAMPLES; i++) peak = max(peak, (int)abs(wave[i]));
  // Auto-gain: aim the loudest sample at ~90% of the half-height, smoothly.
  float want = constrain((AH * 0.45f) / peak, 0.002f, 0.2f);
  gain += (want - gain) * 0.15f;
  int px = AX, py = mid;
  for (int i = 0; i < SAMPLES; i++) {
    int x = AX + i * AW / SAMPLES;
    int y = constrain(mid - (int)(wave[i] * gain), AY, AY + AH - 1);
    if (i) canvas.drawLine(px, py, x, y, C_GREEN);
    if (i) canvas.drawLine(px, py + 1, x, y + 1, C_GREEN);
    px = x;
    py = y;
  }
  // Level meter: -60 .. 0 dBFS.
  float lvl = constrain((audio::micLevelDb() + 60.0f) / 60.0f, 0.0f, 1.0f);
  clearRect(MX, AY, MW, AH);
  canvas.drawRect(MX, AY, MW, AH, C_DIM);
  int h = (int)((AH - 6) * lvl);
  uint32_t c = lvl > 0.8f ? C_RED : lvl > 0.6f ? C_AMBER : C_GREEN;
  canvas.fillRect(MX + 3, AY + AH - 3 - h, MW - 6, h, c);
  dirty(MX, AY, MW, AH);
}

void enter(uint32_t now) {
  if (noteBag.n == 0) noteBag.init(COUNT_OF(AUDIO_SCENE_NOTES));
  title.start(AUDIO_SCENE_TITLE, strlen(AUDIO_SCENE_TITLE), MARGIN_X, BODY_Y + 30, F_BODY, C_AMBER, 30, now);
  const char* n = AUDIO_SCENE_NOTES[noteBag.next()];
  note.start(n, strlen(n), MARGIN_X, H - 30 - lineH(F_BODY), F_BODY, C_GREEN, 40, now + 3000);
  endAt = now + DURATION_MS;
  nextFrame = now;
  gain = 0.02f;
}

bool tick(uint32_t now) {
  if ((int32_t)(now - nextFrame) >= 0) {
    drawFrame();
    nextFrame = now + FRAME_MS;
  }
  if (title.tick(now)) note.tick(now);
  term::cursorTick(now);
  if ((int32_t)(now - endAt) >= 0) {
    term::cursorHide();
    return false;
  }
  return true;
}

bool available() { return audio::micAvailable(); }

}  // namespace

const Scene SCENE_AUDIO = {"audio intercept", enter, tick, nullptr, available};
