// Wearer menu ("OPERATOR CONSOLE"), opened by holding a finger in the
// top-left corner for MENU_HOLD_MS: brightness, sound, pause, run any scene.
// Closes itself after 15 s untouched.
#include "audio.h"
#include "battery.h"
#include "camera.h"
#include "config.h"
#include "gallery.h"
#include "messages.h"
#include "power.h"
#include "rtclock.h"
#include "scenes.h"
#include "settings.h"
#include "ui.h"

using namespace gfx;

namespace {

constexpr int BW = 280, BH = 72, GAP = 16, COLS = 4;  // 6 rows, clear of the bottom strip
constexpr int ROW = BH + 10;
constexpr int X0 = (W - (COLS * BW + (COLS - 1) * GAP)) / 2;
constexpr int Y0 = BODY_Y + 150;
constexpr uint32_t IDLE_CLOSE_MS = 15000;

enum Action {
  BRIGHT_DOWN, BRIGHT_SHOW, BRIGHT_UP, SOUND,
  SIREN_DOWN, SIREN_SHOW, SIREN_UP, PAUSE,
  VOICE_DOWN, VOICE_SHOW, VOICE_UP, EXIT,
  GO_SCAN, GO_KEYPAD, GO_CAPTCHA, GO_GALLERY,
  GO_QR, GO_COUNTDOWN, GO_MIDNIGHT, GO_TOUCHCAL,
  GO_BOOTH, LIGHT_DOWN, LIGHT_SHOW, LIGHT_UP, COUNT
};

char labels[COUNT][24];
uint32_t lastTouch, nextStatus;
// POWER OFF sits top right, beside the title; it takes a second tap within 4 s.
const ui::Button POWER_BTN = {W - X0 - BW, BODY_Y + 16, BW, 110, nullptr};
uint32_t powerArmedAt = 0;
bool powerArmed(uint32_t now) { return powerArmedAt && now - powerArmedAt < 4000; }

ui::Button button(int i) { return {X0 + (i % COLS) * (BW + GAP), Y0 + (i / COLS) * ROW, BW, BH, labels[i]}; }

void makeLabels() {
  static const char* const FIXED[COUNT] = {
      "BRIGHT -", "", "BRIGHT +", "",
      "SIREN -", "", "SIREN +", "",
      "VOICE -", "", "VOICE +", "EXIT",
      "SCAN", "KILL SWITCH", "CAPTCHA", "GALLERY",
      "QR CODE", "COUNTDOWN", "MIDNIGHT TEST", "TOUCH CAL",
      "PHOTO BOOTH", "PHOTO -", "", "PHOTO +"};
  for (int i = 0; i < COUNT; i++) snprintf(labels[i], sizeof(labels[i]), "%s", FIXED[i]);
  snprintf(labels[BRIGHT_SHOW], sizeof(labels[0]), "LIGHT %d%%", settings::brightness());
  snprintf(labels[SOUND], sizeof(labels[0]), "SOUND: %s", settings::muted() ? "OFF" : "ON");
  // The middle buttons switch each sound on/off (tap), showing its volume.
  if (settings::sirenOn()) snprintf(labels[SIREN_SHOW], sizeof(labels[0]), "SIREN %d%%", settings::sirenVolume());
  else snprintf(labels[SIREN_SHOW], sizeof(labels[0]), "SIREN OFF");
  if (settings::voiceOn()) snprintf(labels[VOICE_SHOW], sizeof(labels[0]), "VOICE %d%%", settings::voiceVolume());
  else snprintf(labels[VOICE_SHOW], sizeof(labels[0]), "VOICE OFF");
  snprintf(labels[LIGHT_SHOW], sizeof(labels[0]), "PHOTO LIGHT %d", settings::photoLight());
  snprintf(labels[PAUSE], sizeof(labels[0]), "%s", scenes::paused() ? "RESUME SHOW" : "PAUSE SHOW");
}

uint32_t colorOf(int i) {
  if (i == EXIT) return C_RED;
  if (i == SIREN_DOWN || i == SIREN_SHOW || i == SIREN_UP) return C_AMBER;
  if (i >= LIGHT_DOWN) return C_AMBER;
  if (i < GO_SCAN) return C_CYAN;
  return C_GREEN;
}

void drawPower(uint32_t now) {
  ui::Button b = POWER_BTN;
  bool armed = powerArmed(now);
  b.label = armed ? "TAP AGAIN\nTO POWER OFF" : "POWER OFF";
  clearRect(b.x, b.y, b.w, b.h);
  ui::drawButton(b, F_BODY, C_RED, armed);
}

void drawButtons() {
  makeLabels();
  for (int i = 0; i < COUNT; i++) ui::drawButton(button(i), F_BODY, colorOf(i), false);
  drawPower(millis());
}

void drawStatus() {
  char line[120];
  char clock[16] = "NOT SET";
  if (rtclock::valid()) snprintf(clock, sizeof(clock), "%02d:%02d:%02d", rtclock::hour(), rtclock::minute(), rtclock::second());
  snprintf(line, sizeof(line), "BATT %.2fV %s  MIC %s  CAM %s  CLOCK %s  SCANNED %d",
           battery::volts(), battery::charging() ? "CHG" : "", audio::micAvailable() ? "OK" : "--",
           camera::available() ? "OK" : "--", clock, gallery::total());
  int y = BODY_Y + 20 + lineH(F_LARGE) + 4, room = POWER_BTN.x - GAP - X0;
  line[min<int>(sizeof(line) - 1, room / charW(F_SMALL))] = 0;  // clear of POWER OFF
  clearRect(X0, y, room, lineH(F_SMALL));
  text(line, X0, y, F_SMALL, C_DIM);
}

void enter(uint32_t now) {
  powerArmedAt = 0;
  text(MENU_TITLE, X0, BODY_Y + 20, F_LARGE, C_CYAN);
  dirty(0, BODY_Y, W, 140);
  drawButtons();
  drawStatus();
  lastTouch = now;
  nextStatus = now + 1000;
}

void touch(int x, int y, uint32_t now) {
  lastTouch = now;
  if (POWER_BTN.contains(x, y)) {
    if (powerArmed(now)) {
      audio::fx(audio::FX_POWER_DOWN);
      ui::Button b = POWER_BTN;
      b.label = "SHUTTING\nDOWN...";
      clearRect(b.x, b.y, b.w, b.h);
      ui::drawButton(b, F_BODY, C_RED, true);
      gfx::present();
      powerOff();
    }
    audio::fx(audio::FX_KEY);
    powerArmedAt = now;
    drawPower(now);
    return;
  }
  int hit = -1;
  for (int i = 0; i < COUNT; i++) {
    if (button(i).contains(x, y)) hit = i;
  }
  if (hit < 0) return;
  audio::fx(audio::FX_KEY);
  switch (hit) {
    case BRIGHT_DOWN: settings::setBrightness(settings::brightness() - 10); break;
    case BRIGHT_UP: settings::setBrightness(settings::brightness() + 10); break;
    case SOUND: settings::setMuted(!settings::muted()); break;
    case SIREN_SHOW:
      settings::setSirenOn(!settings::sirenOn());
      if (settings::sirenOn()) audio::fx(audio::FX_SIREN_PREVIEW);
      break;
    case VOICE_SHOW:
      settings::setVoiceOn(!settings::voiceOn());
      if (settings::voiceOn()) audio::say("I HEARD THAT.");
      break;
    case SIREN_DOWN:
    case SIREN_UP:
      settings::setSirenOn(true);  // adjusting a switched-off sound turns it back on
      settings::setSirenVolume(settings::sirenVolume() + (hit == SIREN_UP ? 10 : -10));
      audio::fx(audio::FX_SIREN_PREVIEW);  // hear the new level
      break;
    case VOICE_DOWN:
    case VOICE_UP:
      settings::setVoiceOn(true);
      settings::setVoiceVolume(settings::voiceVolume() + (hit == VOICE_UP ? 10 : -10));
      audio::say("I HEARD THAT.");  // hear the new level
      break;
    case PAUSE:
      scenes::setPaused(!scenes::paused());
      scenes::leaveMenu(now);
      return;
    case GO_SCAN: scenes::playScene(&SCENE_SCAN, now); return;
    case GO_BOOTH: scenes::playScene(&SCENE_BOOTH, now); return;
    case LIGHT_DOWN: settings::setPhotoLight(settings::photoLight() - 1); break;
    case LIGHT_UP: settings::setPhotoLight(settings::photoLight() + 1); break;
    case LIGHT_SHOW: scenes::playScene(&SCENE_MIRROR, now); return;  // see the effect live
    case GO_KEYPAD: scenes::playScene(&SCENE_KEYPAD, now); return;
    case GO_CAPTCHA: scenes::playScene(&SCENE_CAPTCHA, now); return;
    case GO_GALLERY: scenes::playScene(gallery::count() ? &SCENE_GALLERY : &SCENE_SCAN, now); return;
    case GO_QR: scenes::playScene(&SCENE_QR, now); return;
    case GO_COUNTDOWN: scenes::playScene(rtclock::valid() ? &SCENE_COUNTDOWN : &SCENE_HEADLINE, now); return;
    case GO_MIDNIGHT: scenes::midnight(now); return;
    case GO_TOUCHCAL: scenes::playScene(&SCENE_TOUCHCAL, now); return;
    case EXIT: scenes::leaveMenu(now); return;
    default: return;
  }
  drawButtons();  // brightness / sound labels changed
}

bool tick(uint32_t now) {
  if (powerArmedAt && !powerArmed(now)) {  // not confirmed: back to POWER OFF
    powerArmedAt = 0;
    drawPower(now);
  }
  if ((int32_t)(now - nextStatus) >= 0) {
    drawStatus();
    nextStatus = now + 1000;
  }
  return now - lastTouch < IDLE_CLOSE_MS;
}

}  // namespace

const Scene SCENE_MENU = {"wearer menu", enter, tick, touch, nullptr};
