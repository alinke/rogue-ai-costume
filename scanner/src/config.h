#pragma once
// ---- Private settings: src/secrets.h (never committed; copy secrets.h.example) --
// Wi-Fi, the photo service's URL and upload key, your site. Without the file
// (or in a PUBLIC_BUILD, e.g. the web-flasher images) photo upload is off.
#if !defined(PUBLIC_BUILD) && __has_include("secrets.h")
#include "secrets.h"
#endif
#if defined(WIFI_NETWORKS) && defined(PHOTO_SERVICE_URL) && defined(PHOTO_UPLOAD_KEY)
#define PHOTO_SERVICE_CONFIGURED 1
#else
#define PHOTO_SERVICE_CONFIGURED 0
#endif
#ifndef PHOTO_SITE_TEXT
#define PHOTO_SITE_TEXT "THE ROGUE.AI WALL"  // where guests find their photo, shown on screen
#endif

// Scanner settings. Keep the signal thresholds in step with the costume's
// config.h (SCANNER_NEAR_DBM / SCANNER_CLOSE_DBM) so both agree on "near".

#define NEAR_DBM -70          // stronger: SIGNAL STRONG (~2 m)
#define CLOSE_DBM -58         // stronger: TARGET LOCKED (arm's length)
#define HYSTERESIS_DB 4
#define LOST_MS 5000          // no AI beacon this long: NO SIGNAL

#define CONTAIN_COOLDOWN_S 12  // same as the costume's: one attack per press
// ---- Power (500 mAh battery) ----
#define BACKLIGHT_PERCENT 50     // the backlight is a big share of the draw
#define IDLE_DIM_PERCENT 15      // ...dimmed to this after IDLE_DIM_AFTER_S with no AI signal
#define IDLE_DIM_AFTER_S 60
#define FRAME_MS 100             // 10 redraws a second
#define CPU_MHZ 160
#define SCAN_WINDOW_PERCENT 20   // Bluetooth listening duty cycle (the costume beacons ~8x/s)
#define VOLUME_PERCENT 60      // tones
#define GEIGER_CLICKS 0        // 1 = geiger clicks that speed up as she gets closer
#ifdef SCANNER_BOARD_TAB5
#define CONTAIN_BUTTON_PIN -1  // the Tab5 has no free button: on-screen only
#else
#define CONTAIN_BUTTON_PIN 0   // the BOOT button also fires CONTAIN
#endif
#define POWER_HOLD_MS 2000     // hold the PWR key (Waveshare) or the top bar (both) this long: power off

// ---- Evidence photos (Tab5 only: it has the camera) --------------------------
// PHOTO on screen opens the viewfinder; a tap takes the picture, which goes to
// the Halloween page's "LAB-7 evidence" gallery (queued on the SD card, sent
// over Wi-Fi via the C6 when a network is reachable). Networks and the upload
// key: src/secrets.h (copy secrets.h.example). Server: rogue-ai-costume/cloud.
#if defined(SCANNER_BOARD_TAB5) && PHOTO_SERVICE_CONFIGURED
#define PHOTO_UPLOAD_ENABLED 1
#else
#define PHOTO_UPLOAD_ENABLED 0
#endif
#define PHOTO_UPLOAD_URL PHOTO_SERVICE_URL "/upload?set=lab"
#define PHOTO_JPEG_QUALITY 80
#define PHOTO_QUEUE_MAX 40
#define PORTRAIT_ROT 0           // Tab5 held upright, camera at the top: 0, or 2 if it comes out upside down
#define CAMERA_FLIP_V 1          // the Tab5 camera is mounted upside down: flipped = upright and mirrored like a selfie
#define VIEWFINDER_TIMEOUT_S 30  // the viewfinder closes itself (camera off) after this long untouched
#define COMPROMISED_PERCENT 20    // humanity test: how often a guest is found COMPROMISED instead of VERIFIED HUMAN
#ifndef RESEARCHER_NAME  // on the ID card: "AUTHORISED: ..." (set your own in secrets.h)
#define RESEARCHER_NAME "LAB-7 FIELD RESEARCHER"
#endif
