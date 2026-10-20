#pragma once
// Geiger-counter clicks (randomly spaced, faster when closer) and a few
// tones, generated in a background task through the board's speaker.
#include <Arduino.h>

namespace sound {

enum Tone : uint8_t { LOCK, ZAP, FAIL, BEEP };

void begin();
void setClickRate(float perSecond);  // 0 = silent
void play(Tone t);

}  // namespace sound
