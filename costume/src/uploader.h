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
// CLEAR PHOTOS (operator console): drop the photos waiting to upload and, if
// PHOTO_ADMIN_KEY is set, delete every photo on the web page (as soon as Wi-Fi
// is reachable). The SD card's keepsake copies stay.
void clearAll();
bool clearPending();  // the web page hasn't been cleared yet
// Power off: finish (or give up on) the current upload, Wi-Fi fully off, task
// parked. Waits up to timeoutMs; true = done.
bool shutdown(uint32_t timeoutMs);
int pending();   // photos waiting to go
int sent();      // sent since power-on

}  // namespace uploader
