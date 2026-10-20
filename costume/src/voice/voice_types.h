#pragma once
#include <stdint.h>

// One pre-recorded line: 8-bit mu-law at VOICE_SAMPLE_RATE. `text` is the
// exact string from messages.h it was made from; audio::say() matches on it.
struct VoiceClip {
  const char* text;
  const uint8_t* data;
  uint32_t len;
};

#define VOICE_SAMPLE_RATE 16000
