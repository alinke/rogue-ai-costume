#pragma once
// The bits of the costume's gfx.h that its leds.cpp uses: the palette and rnd().
#include <Arduino.h>
#include <esp_random.h>

namespace gfx {
constexpr uint32_t C_GREEN = 0x28FF5A;
constexpr uint32_t C_DIM = 0x00823A;
constexpr uint32_t C_RED = 0xFF1E28;
constexpr uint32_t C_AMBER = 0xFFAA00;
constexpr uint32_t C_CYAN = 0x00DCFF;
constexpr uint32_t C_WHITE = 0xEBFFF0;
inline uint32_t rnd(uint32_t lo, uint32_t hi) { return lo + esp_random() % (hi - lo + 1); }
inline uint32_t rnd(uint32_t n) { return rnd(0, n - 1); }
}  // namespace gfx
