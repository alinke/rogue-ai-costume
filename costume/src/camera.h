#pragma once
// Snapshot camera (SC2336 on the board's MIPI-CSI connector) for the Human
// Scan. The sensor streams only while a photo is being taken (~1 s); the rest
// of the time it sits in standby.
#include <Arduino.h>

namespace camera {

// Size of the processed photo: grayscale, one byte per pixel.
struct Photo {
  int w = 0, h = 0;
  const uint8_t* pixels = nullptr;  // w*h, row-major
};

bool begin(int photoW, int photoH);  // false = no camera (scan uses its silhouette)
bool available();

void requestCapture();   // starts a capture in the background
bool ready();            // the photo from the last request is complete
Photo photo();           // valid once ready()

// Live view (mirrored, photo-sized frames) for the mirror scene.
bool startLive();
void stopLive();
uint32_t liveFrame(const uint8_t** pixels);  // newest frame; returns a frame counter (0 = none yet)

// Motion trigger (config MOTION_TRIGGER_ENABLED): someone stepped in front.
bool takeMotion();       // true once per detection
void setMotionEnabled(bool on);
void setLight(int level);  // 0-6 (settings PHOTO LIGHT): exposure target + midtone lift
void dumpDebugRgb();     // CAMERA_DEBUG_LOG builds: last colour frame as PPM over serial

}  // namespace camera
