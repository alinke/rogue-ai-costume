#include "settings.h"

#include "camera.h"

#include <Preferences.h>

#include "audio.h"
#include "config.h"
#include "gfx.h"

namespace settings {

static Preferences s_prefs;
static int s_photoLight = PHOTO_LIGHT_DEFAULT;
static int s_brightness = BACKLIGHT_PERCENT;
static bool s_muted = false;
static int s_siren = SIREN_VOLUME_PERCENT, s_voice = VOICE_VOLUME_PERCENT;
static bool s_sirenOn = true, s_voiceOn = true;

void begin() {
  s_prefs.begin("rogueai", false);
  s_brightness = constrain((int)s_prefs.getUChar("bright", BACKLIGHT_PERCENT), 10, 100);
  s_muted = s_prefs.getBool("muted", false);
  audio::setMuted(s_muted);
  s_siren = constrain((int)s_prefs.getUChar("sirenv", SIREN_VOLUME_PERCENT), 0, 100);
  s_voice = constrain((int)s_prefs.getUChar("voicev", VOICE_VOLUME_PERCENT), 0, 100);
  audio::setSirenVolume(s_siren);
  audio::setVoiceVolume(s_voice);
  s_sirenOn = s_prefs.getBool("sirenon", true);
  s_photoLight = constrain((int)s_prefs.getUChar("plight", PHOTO_LIGHT_DEFAULT), 0, 6);
  camera::setLight(s_photoLight);
  s_voiceOn = s_prefs.getBool("voiceon", true);
  audio::setSirenEnabled(s_sirenOn);
  audio::setVoiceEnabled(s_voiceOn);
  gfx::setTouchCal(s_prefs.getFloat("tcax", 1), s_prefs.getFloat("tcbx", 0), s_prefs.getFloat("tcay", 1),
                   s_prefs.getFloat("tcby", 0));
  float ax, bx, ay, by;
  gfx::getTouchCal(&ax, &bx, &ay, &by);
  Serial.printf("[I][settings] siren %s %d%%, voice %s %d%%\n", s_sirenOn ? "on" : "OFF", s_siren,
                s_voiceOn ? "on" : "OFF", s_voice);
  Serial.printf("[I][settings] brightness %d%%, sound %s, touch cal x=%.3f*x%+.0f y=%.3f*y%+.0f\n", s_brightness,
                s_muted ? "off" : "on", ax, bx, ay, by);
}

int brightness() { return s_brightness; }

void setBrightness(int pct) {
  s_brightness = constrain(pct, 10, 100);
  s_prefs.putUChar("bright", (uint8_t)s_brightness);
  gfx::setBacklightPercent(s_brightness);
}

bool muted() { return s_muted; }

void setMuted(bool m) {
  s_muted = m;
  s_prefs.putBool("muted", m);
  audio::setMuted(m);
}

int photoLight() { return s_photoLight; }
void setPhotoLight(int level) {
  s_photoLight = constrain(level, 0, 6);
  s_prefs.putUChar("plight", (uint8_t)s_photoLight);
  camera::setLight(s_photoLight);
}

int sirenVolume() { return s_siren; }

void setSirenVolume(int pct) {
  s_siren = constrain(pct, 0, 100);
  s_prefs.putUChar("sirenv", (uint8_t)s_siren);
  audio::setSirenVolume(s_siren);
}

int voiceVolume() { return s_voice; }

void setVoiceVolume(int pct) {
  s_voice = constrain(pct, 0, 100);
  s_prefs.putUChar("voicev", (uint8_t)s_voice);
  audio::setVoiceVolume(s_voice);
}

bool sirenOn() { return s_sirenOn; }

void setSirenOn(bool on) {
  s_sirenOn = on;
  s_prefs.putBool("sirenon", on);
  audio::setSirenEnabled(on);
}

bool voiceOn() { return s_voiceOn; }

void setVoiceOn(bool on) {
  s_voiceOn = on;
  s_prefs.putBool("voiceon", on);
  audio::setVoiceEnabled(on);
}

void saveTouchCal() {
  float ax, bx, ay, by;
  gfx::getTouchCal(&ax, &bx, &ay, &by);
  s_prefs.putFloat("tcax", ax);
  s_prefs.putFloat("tcbx", bx);
  s_prefs.putFloat("tcay", ay);
  s_prefs.putFloat("tcby", by);
}

}  // namespace settings
