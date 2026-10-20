#pragma once
// Wearer-menu settings, kept in flash (NVS) so they survive a power cycle.
#include <Arduino.h>

namespace settings {

void begin();              // loads, and applies the mute setting
int brightness();          // backlight %, default BACKLIGHT_PERCENT
void setBrightness(int pct);
bool muted();
void setMuted(bool m);
int sirenVolume();         // %, default SIREN_VOLUME_PERCENT
void setSirenVolume(int pct);
int voiceVolume();         // %, default VOICE_VOLUME_PERCENT
void setVoiceVolume(int pct);
bool sirenOn();
void setSirenOn(bool on);
bool voiceOn();
void setVoiceOn(bool on);
int photoLight();          // 0-6: how bright photos come out (camera exposure + midtone lift)
void setPhotoLight(int level);
void saveTouchCal();       // stores gfx's current touch correction

}  // namespace settings
