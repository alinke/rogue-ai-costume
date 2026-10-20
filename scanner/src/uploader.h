#pragma once
// Evidence photo upload to the Halloween page (config PHOTO_UPLOAD_*). Copied from
// the costume (rogue-ai-costume/src/uploader.cpp). The image is
// JPEG-encoded by the P4's hardware encoder, queued in flash (LittleFS), and
// sent by a background task over Wi-Fi (through the C6, alongside Bluetooth).
// Wi-Fi is switched on only while the queue has something in it.
#include <Arduino.h>

namespace uploader {

void begin();
// Queue a 720x1280 image (LovyanGFX's byte-swapped RGB565, e.g. the canvas),
// with a short label shown under the photo on the page. Takes ~70 ms.
void queueImage(const uint16_t* swapped565, const char* label);
int pending();   // photos waiting to go
int sent();      // sent since power-on

}  // namespace uploader
