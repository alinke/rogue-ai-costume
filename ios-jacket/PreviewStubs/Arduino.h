#pragma once
// Just enough of Arduino for LEDLINK's looks code (../../ledlink/src/looks.cpp,
// arcade.cpp) to run inside the app, for demo mode's preview (JacketPreview.cpp).
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>

using std::max;
using std::min;

#ifndef PI
#define PI 3.14159265358979f
#endif

template <class T, class L, class H>
static inline T constrain(T v, L lo, H hi) { return v < lo ? (T)lo : v > hi ? (T)hi : v; }

uint32_t millis();  // the preview's clock (JacketPreview.cpp)

struct PreviewSerial {  // log lines go nowhere
  int printf(const char*, ...) { return 0; }
  void println(const char* = "") {}
};
extern PreviewSerial Serial;
