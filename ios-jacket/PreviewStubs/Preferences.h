#pragma once
// The preview keeps nothing: every setting starts at its default.
#include <stddef.h>
#include <stdint.h>

struct Preferences {
  bool begin(const char*, bool = false) { return true; }
  void end() {}
  uint8_t getUChar(const char*, uint8_t d = 0) { return d; }
  uint32_t getUInt(const char*, uint32_t d = 0) { return d; }
  bool getBool(const char*, bool d = false) { return d; }
  size_t putUChar(const char*, uint8_t) { return 1; }
  size_t putUInt(const char*, uint32_t) { return 4; }
  size_t putBool(const char*, bool) { return 1; }
};
