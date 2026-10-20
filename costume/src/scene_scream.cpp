// "SCREAM TO PROVE YOU ARE HUMAN": countdown, then a huge live meter for four
// seconds. The loudest scream of the night is the record to beat.
#include "audio.h"
#include "bigtext.h"
#include "gallery.h"
#include "messages.h"
#include "scenes.h"

using namespace gfx;

namespace {

constexpr int MX = 140, MW = W - 280, MY = BODY_Y + 280, MH = 140;
constexpr uint32_t MEASURE_MS = 4000;

enum Phase { PROMPT, COUNT, MEASURE, RESULT };
Phase phase;
uint32_t at;
int count;
float peak;
int record = 0;  // this power-on's loudest
bigtext::Animator anim;
char big[48];

int score(float dbfs) { return constrain((int)(dbfs + 112), 40, 130); }  // a fun "dB" scale

void drawMeter(float dbfs) {
  int s = score(dbfs);
  float f = (s - 40) / 90.0f;
  clearRect(MX, MY, MW, MH + lineH(F_LARGE) + 20);
  for (int t = 0; t < 3; t++) canvas.drawRect(MX + t, MY + t, MW - 2 * t, MH - 2 * t, C_DIM);
  uint32_t c = f > 0.75f ? C_RED : f > 0.5f ? C_AMBER : C_GREEN;
  canvas.fillRect(MX + 6, MY + 6, (int)((MW - 12) * f), MH - 12, c);
  if (record) {  // the record mark
    int rx = MX + 6 + (int)((MW - 12) * (record - 40) / 90.0f);
    canvas.fillRect(rx - 2, MY - 14, 4, MH + 28, C_CYAN);
  }
  char t[48];
  snprintf(t, sizeof(t), "%d dB   (PEAK %d)", s, score(peak));
  text(t, MX, MY + MH + 16, F_LARGE, c);
  dirty(MX, MY - 14, MW, MH + 30 + lineH(F_LARGE));
}

void enter(uint32_t now) {
  anim.start(SCREAM_PROMPT, BODY_Y + 30, 230, C_WHITE, now);
  audio::say(SCREAM_PROMPT);
  if (record) {
    char r[40];
    snprintf(r, sizeof(r), "TONIGHT'S RECORD: %d dB", record);
    text(r, MX, H - 50 - lineH(F_BODY), F_BODY, C_CYAN);
    dirty(0, H - 50 - lineH(F_BODY), W, lineH(F_BODY));
  }
  phase = PROMPT;
  at = now + 2800;
  peak = -90;
}

bool tick(uint32_t now) {
  switch (phase) {
    case PROMPT:
      anim.tick(now);
      if ((int32_t)(now - at) >= 0) {
        phase = COUNT;
        count = 3;
        at = now;
      }
      break;
    case COUNT:
      if ((int32_t)(now - at) >= 0) {
        clearRect(0, BODY_Y, W, 260);
        snprintf(big, sizeof(big), "%s", count ? "" : "SCREAM!");
        if (count) snprintf(big, sizeof(big), "%d", count);
        anim.start(big, BODY_Y + 20, 240, count ? C_AMBER : C_RED, now);
        audio::fx(count ? audio::FX_KEY : audio::FX_OK);
        if (count-- == 0) {
          phase = MEASURE;
          at = now + MEASURE_MS + 400;  // start after the beep has faded
        } else {
          at = now + 900;
        }
      }
      anim.tick(now);
      break;
    case MEASURE: {
      anim.tick(now);
      float lvl = audio::micLevelDb();
      if ((int32_t)(now - (at - MEASURE_MS)) >= 0) peak = max(peak, lvl);
      drawMeter(lvl);
      if ((int32_t)(now - at) >= 0) {
        int s = score(peak);
        const char* verdict = SCREAM_VERDICTS[s < 85 ? 0 : s < 105 ? 1 : 2];
        bool newRecord = s > record && s > 60;
        if (newRecord) record = s;
        Serial.printf("[I][scream] peak %d dB%s\n", s, newRecord ? " (record)" : "");
        clearBody();
        snprintf(big, sizeof(big), "%d dB", s);
        anim.start(big, BODY_Y + 40, 300, newRecord ? C_CYAN : C_GREEN, now);
        int y = BODY_Y + 380;
        text(verdict, (W - (int)strlen(verdict) * charW(F_BODY)) / 2, y, F_BODY, C_WHITE);
        if (newRecord) text(SCREAM_RECORD, (W - (int)strlen(SCREAM_RECORD) * charW(F_LARGE)) / 2, y + 70, F_LARGE, C_CYAN);
        dirty(0, y, W, 200);
        if (newRecord) audio::say(SCREAM_RECORD);
        audio::say(verdict);
        phase = RESULT;
        at = now + 5000;
      }
      break;
    }
    case RESULT:
      anim.tick(now);
      if ((int32_t)(now - at) >= 0) return false;
      break;
  }
  return true;
}

bool available() { return audio::micAvailable(); }

}  // namespace

const Scene SCENE_SCREAM = {"scream meter", enter, tick, nullptr, available, nullptr};
