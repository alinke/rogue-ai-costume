#pragma once
// The night's scanned humans: half-size copies of each scan photo for the
// "ASSIMILATED HUMANS" wall. Kept in PSRAM only, gone at power-off. (The
// upload to the Halloween page is separate: uploader.h.)
#include <Arduino.h>

#include "camera.h"

namespace gallery {

constexpr int THUMB_W = 280, THUMB_H = 298;  // half the scan photo

struct Entry {
  const uint8_t* pixels;  // THUMB_W x THUMB_H grayscale
  int number;             // specimen number (1, 2, ...)
  const char* verdict;
};

bool begin();
int add(const camera::Photo& photo, const char* verdict);  // returns the specimen number
int count();       // entries held (up to GALLERY_SIZE), newest last
void clear();      // empty the wall and restart the specimen numbers (operator console)
int total();       // humans scanned since power-on
Entry entry(int i);

}  // namespace gallery
