#pragma once
// Sound through the onboard speaker: synthesized effects (siren, keypad, ...)
// mixed with pre-recorded robot voice clips (tools/make_voice.py). Plays in
// its own task, so the display keeps animating; the amplifier runs only while
// something is playing. All calls return immediately.
#include <Arduino.h>

namespace audio {

enum Fx : uint8_t {
  FX_SIREN,       // alarm yelp (SIREN_MS)
  FX_KEY,         // keypad click
  FX_DENIED,      // low buzzer
  FX_OK,          // rising two-tone chime
  FX_POWER_DOWN,  // falling sweep (fake shutdown)
  FX_SHUTTER,     // camera click
  FX_SIREN_PREVIEW,  // a short burst of the siren (volume setting)
};

void begin();
void fx(Fx f);                // replaces whatever effect is playing
void say(const char* text);   // robot voice, if a clip exists for exactly this text
void siren();                 // = fx(FX_SIREN)
void setMuted(bool muted);
void setSirenVolume(int percent);  // 0-100
void setVoiceVolume(int percent);  // 0-100
void setSirenEnabled(bool on);     // siren off: everything else still plays
void setVoiceEnabled(bool on);     // voice off: effects and siren still play
bool muted();
bool busy();                  // something is playing (the mic ignores itself then)

// Microphone (ES8311 input), if fitted -- see config MIC_ENABLED.
bool micAvailable();
float micLevelDb();           // short-term level, dBFS (about -90 .. 0)
float micBaselineDb();        // slow average of the room
void micWaveform(int16_t* out, int n);  // the latest n samples (for drawing)
// Sharp sounds (a clap, a bang): a counter that goes up by one per onset,
// with the time and level of the latest.
uint32_t onsetCount();
uint32_t lastOnsetMs();
float lastOnsetDb();

}  // namespace audio
