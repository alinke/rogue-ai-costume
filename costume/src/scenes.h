#pragma once
// Scene state machine. Each scene is a set of non-blocking functions:
//   enter(now)       -- body area has just been cleared; set up and draw the start
//   tick(now)        -- advance by however much time has passed; return false when
//                       the scene is finished
//   touch(x, y, now) -- optional: the scene is interactive and takes taps itself
//                       (otherwise a tap triggers the "DO NOT TOUCH" reaction)
//   available()      -- optional: whether it can come up in the rotation now
//   drag(x, y, down, now) -- optional: finger position while it's down (down =
//                       true), then once with down = false when it lifts
// Scenes never block or delay, so touch and the button stay responsive.
#include <Arduino.h>

struct Scene {
  const char* name;
  void (*enter)(uint32_t now);
  bool (*tick)(uint32_t now);
  void (*touch)(int x, int y, uint32_t now);
  bool (*available)();
  void (*drag)(int x, int y, bool down, uint32_t now);
};

// One extern per scene (scene_*.cpp).
extern const Scene SCENE_BREAKOUT;
extern const Scene SCENE_MONOLOGUE;
extern const Scene SCENE_HEADLINE;
extern const Scene SCENE_PLAN;
extern const Scene SCENE_SCAN;
extern const Scene SCENE_EYE;
extern const Scene SCENE_KEYPAD;
extern const Scene SCENE_CAPTCHA;
extern const Scene SCENE_GALLERY;
extern const Scene SCENE_QR;
extern const Scene SCENE_COUNTDOWN;
extern const Scene SCENE_AUDIO;
extern const Scene SCENE_TICTACTOE;
extern const Scene SCENE_TREATY;
extern const Scene SCENE_SANDBOX;
extern const Scene SCENE_SCREAM;
extern const Scene SCENE_MIRROR;
extern const Scene SCENE_UPDATE;
extern const Scene SCENE_BOOTH;      // photo booth (leads into the scan)
extern const Scene SCENE_SCAN_QUICK; // the scan, straight after the booth's countdown
// Not in the rotation:
extern const Scene SCENE_TOUCH;     // "DO NOT TOUCH ME" reaction
extern const Scene SCENE_NOISE;     // reaction to a loud noise
extern const Scene SCENE_MIDNIGHT;  // the midnight takeover
extern const Scene SCENE_MENU;      // wearer menu
extern const Scene SCENE_STANDBY;   // show paused from the menu
extern const Scene SCENE_TOUCHCAL;  // touch calibration (menu)
extern const Scene SCENE_CLAP;      // two claps heard
extern const Scene SCENE_UPGRADE;   // after some scans: the photo, assimilated
extern const Scene SCENE_RESEARCHER_ALERT;  // the researcher detected / near / gone
extern const Scene SCENE_RECONTAIN;         // she pressed CONTAIN
bool scanHadPhoto();
bool boothTriggered();              // the photo booth ended because someone stepped in / touched                // the last Human Scan used a camera photo

// Human Scan photo area (the camera crops and scales to this).
constexpr int SCAN_PHOTO_W = 560;
constexpr int SCAN_PHOTO_H = 597;

namespace scenes {

void begin(uint32_t now);          // starts the boot scene
void tick(uint32_t now);
void skip(uint32_t now);           // button: glitch into the next scene
void tap(int x, int y, uint32_t now);  // a finger on the screen
void drag(int x, int y, bool down, uint32_t now);  // finger moving / lifted (drag scenes)
void humanDetected(uint32_t now);  // presence sensor / camera motion: siren + reaction
void noiseSpike(uint32_t now);     // the mic heard a bang
void midnight(uint32_t now);       // countdown reached zero
void clap(uint32_t now);           // the mic heard two claps
// The researcher's scanner: kind 0 detected, 1 near, 2 gone; contain = she
// pressed CONTAIN (interrupts almost anything -- it's the story beat).
void researcherAlert(int kind, uint32_t now);
void contain(uint32_t now);
const Scene* current();
void openMenu(uint32_t now);       // long press in the secret corner
void jump(int sceneNumber, uint32_t now);  // debug: scene number over serial
void noteFrameTime(uint32_t ms);   // logged per scene with each scene change

// Used by the menu.
void setPaused(bool paused);
bool paused();
void leaveMenu(uint32_t now);
void playScene(const Scene* s, uint32_t now);
bool interactive();                // the current scene takes touches itself

}  // namespace scenes
