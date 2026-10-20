#pragma once
// Photo upload to the Halloween page (config PHOTO_UPLOAD_*). The screen is
// JPEG-encoded by the P4's hardware encoder, queued in flash (LittleFS), and
// sent by a background task over Wi-Fi (through the C6, alongside Bluetooth).
// Wi-Fi is switched on only while the queue has something in it.
#include <Arduino.h>

#include "camera.h"

namespace uploader {

void begin();
// Snapshot the whole screen (as drawn on the canvas) and queue it, with a
// short label shown under the photo on the page. Takes ~50 ms.
// face = the plain camera photo, sent along for the server's face check only.
void queueScreen(const char* label, const camera::Photo* face = nullptr);
int pending();   // photos waiting to go
int sent();      // sent since power-on

}  // namespace uploader
