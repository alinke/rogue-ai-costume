#include "remote.h"

#include "audio.h"
#include "config.h"
#include "gallery.h"
#include "leds.h"
#include "rtclock.h"
#include "scenes.h"
#include "settings.h"

namespace remote {

void run(uint8_t cmd, uint32_t now) {
  Serial.printf("[I][remote] armband command %u\n", cmd);
  // Anything that plays a scene also ends a pause.
  auto play = [&](const Scene* s) {
    scenes::setPaused(false);
    scenes::playScene(s, now);
  };
  switch (cmd) {
    case CMD_SCAN: play(&SCENE_SCAN); break;
    case CMD_KILL_SWITCH: play(&SCENE_KEYPAD); break;
    case CMD_QR: play(&SCENE_QR); break;
    case CMD_NEXT:
      scenes::setPaused(false);
      scenes::skip(now);
      break;
    case CMD_COUNTDOWN: play(rtclock::valid() ? &SCENE_COUNTDOWN : &SCENE_HEADLINE); break;
    case CMD_MIDNIGHT:
      scenes::setPaused(false);
      scenes::midnight(now);
      break;
    case CMD_BOOTH:
      if (CAMERA_ENABLED) play(&SCENE_BOOTH);
      break;
    case CMD_GALLERY:
      if (CAMERA_ENABLED) play(gallery::count() ? &SCENE_GALLERY : &SCENE_SCAN);
      break;
    case CMD_PAUSE:
      scenes::setPaused(true);
      scenes::playScene(&SCENE_STANDBY, now);
      break;
    case CMD_RESUME:
      scenes::setPaused(false);
      scenes::leaveMenu(now);  // = on to the next scene
      break;
    case CMD_MUTE: settings::setMuted(true); break;
    case CMD_UNMUTE: settings::setMuted(false); break;
    case CMD_BRIGHT_DOWN: settings::setBrightness(settings::brightness() - 10); break;
    case CMD_BRIGHT_UP: settings::setBrightness(settings::brightness() + 10); break;
    case CMD_VOICE_DOWN:
    case CMD_VOICE_UP:  // as the costume's own console: turns the voice back on, and you hear the new level
      settings::setVoiceOn(true);
      settings::setVoiceVolume(settings::voiceVolume() + (cmd == CMD_VOICE_UP ? 10 : -10));
      audio::say("I HEARD THAT.");
      break;
    case CMD_STEALTH: leds::setStealth(!leds::stealth()); break;  // toggles: all LEDs dark / back
    case CMD_OVERLOAD: leds::overload(5000); break;
    default: break;
  }
}

}  // namespace remote
