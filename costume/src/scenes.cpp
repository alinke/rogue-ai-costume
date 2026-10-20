#include "scenes.h"

#include "audio.h"
#include "config.h"
#include "gfx.h"
#include "header.h"
#include "leds.h"
#include "messages.h"
#include "settings.h"
#include "term.h"

using namespace gfx;

extern int g_researcherAlert;  // scene_researcher.cpp

namespace scenes {

struct Weighted { const Scene* scene; int weight; };

// Weighted rotation (weights in config.h). Scenes with available() only come
// up when it says so (e.g. the gallery once there are photos).
static const Weighted ROTATION[] = {
    {&SCENE_MONOLOGUE, WEIGHT_MONOLOGUE}, {&SCENE_HEADLINE, WEIGHT_HEADLINE},
    {&SCENE_PLAN, WEIGHT_PLAN},           {&SCENE_SCAN, WEIGHT_SCAN},
    {&SCENE_EYE, WEIGHT_EYE},             {&SCENE_BREAKOUT, WEIGHT_BREAKOUT},
    {&SCENE_KEYPAD, WEIGHT_KEYPAD},       {&SCENE_GALLERY, WEIGHT_GALLERY},
    {&SCENE_QR, WEIGHT_QR},               {&SCENE_COUNTDOWN, WEIGHT_COUNTDOWN},
    {&SCENE_AUDIO, WEIGHT_AUDIO},         {&SCENE_TICTACTOE, WEIGHT_TICTACTOE},
    {&SCENE_TREATY, WEIGHT_TREATY},       {&SCENE_SANDBOX, WEIGHT_SANDBOX},
    {&SCENE_SCREAM, WEIGHT_SCREAM},       {&SCENE_MIRROR, WEIGHT_MIRROR},
    {&SCENE_UPDATE, WEIGHT_UPDATE},       {&SCENE_BOOTH, WEIGHT_BOOTH},
};

// What follows the "DO NOT TOUCH ME" reaction.
static const Weighted AFTER_TOUCH[] = {
    {&SCENE_SCAN, AFTER_TOUCH_SCAN},
    {&SCENE_KEYPAD, AFTER_TOUCH_KEYPAD},
    {&SCENE_CAPTCHA, AFTER_TOUCH_CAPTCHA},
    {&SCENE_TICTACTOE, AFTER_TOUCH_TICTACTOE},
    {&SCENE_TREATY, AFTER_TOUCH_TREATY},
    {&SCENE_SANDBOX, AFTER_TOUCH_SANDBOX},
};

static constexpr uint32_t TRANSITION_MS = 200;
static constexpr uint32_t GLITCH_FRAME_MS = 33;

static const Scene* s_current = nullptr;
static const Scene* s_next = nullptr;
static bool s_transitioning = false;
static uint32_t s_transitionEnd = 0;
static uint32_t s_nextGlitch = 0;
static uint32_t s_tint = C_GREEN;
static uint32_t s_sceneWorstMs = 0;
static uint32_t s_reactAllowedAt = 0;
static bool s_paused = false;

void noteFrameTime(uint32_t ms) { s_sceneWorstMs = max(s_sceneWorstMs, ms); }

static bool usable(const Scene* s) { return !s->available || s->available(); }

static const Scene* pickFrom(const Weighted* list, int n, const Scene* avoid) {
  int total = 0;
  for (int i = 0; i < n; i++) total += usable(list[i].scene) ? list[i].weight : 0;
  if (total <= 0) return &SCENE_MONOLOGUE;
  // Re-roll once if we'd repeat the scene we're leaving.
  for (int attempt = 0; attempt < 2; attempt++) {
    int r = rnd(total);
    for (int i = 0; i < n; i++) {
      if (!usable(list[i].scene)) continue;
      if (r < list[i].weight) {
        if (list[i].scene != avoid || attempt == 1) return list[i].scene;
        break;
      }
      r -= list[i].weight;
    }
  }
  return &SCENE_MONOLOGUE;
}

static const Scene* pickNext() { return pickFrom(ROTATION, COUNT_OF(ROTATION), s_current); }

static void startTransition(const Scene* next, uint32_t tint, uint32_t now) {
  term::cursorHide();
  s_next = next;
  s_tint = tint;
  s_transitioning = true;
  s_transitionEnd = now + TRANSITION_MS;
  s_nextGlitch = now;
}

static void enter(const Scene* scene, uint32_t now) {
  // A scene may have changed the backlight (the scan's photo flash, the fake
  // shutdown) and been cut short: always come back to the normal level.
  setBacklightPercent(settings::brightness());
  term::cursorForget();
  clearBody();
  header::setHidden(false);
  leds::setMode(leds::IDLE);
  s_current = scene;
  Serial.printf("[I][scene] %s (previous scene's slowest frame: %u ms)\n", scene->name,
                (unsigned)s_sceneWorstMs);
  s_sceneWorstMs = 0;
  scene->enter(now);
}

void begin(uint32_t now) { enter(&SCENE_BREAKOUT, now); }

void tick(uint32_t now) {
  if (s_transitioning) {
    if ((int32_t)(now - s_transitionEnd) >= 0) {
      s_transitioning = false;
      enter(s_next, now);
    } else if ((int32_t)(now - s_nextGlitch) >= 0) {
      glitchFrame(0.4f + rnd(60) / 100.0f, s_tint);
      s_nextGlitch = now + GLITCH_FRAME_MS;
    }
    return;
  }
  if (s_current->tick(now)) return;
  if (s_current == &SCENE_TOUCH) {
    startTransition(pickFrom(AFTER_TOUCH, COUNT_OF(AFTER_TOUCH), nullptr), C_RED, now);
  } else if (s_current == &SCENE_BOOTH && boothTriggered()) {
    startTransition(&SCENE_SCAN_QUICK, C_WHITE, now);
  } else if ((s_current == &SCENE_SCAN || s_current == &SCENE_SCAN_QUICK) && scanHadPhoto() && (int)rnd(100) < UPGRADE_AFTER_SCAN_PERCENT) {
    startTransition(&SCENE_UPGRADE, C_RED, now);
  } else if (s_current == &SCENE_MENU || s_current == &SCENE_TOUCHCAL) {
    leaveMenu(now);
  } else if (s_paused) {
    startTransition(&SCENE_STANDBY, C_DIM, now);
  } else {
    startTransition(pickNext(), C_GREEN, now);
  }
}

bool interactive() {
  return !s_transitioning && s_current && (s_current->touch || s_current->drag);
}

void drag(int x, int y, bool down, uint32_t now) {
  if (!s_transitioning && s_current && s_current->drag) s_current->drag(x, y, down, now);
}

void skip(uint32_t now) {
  if (s_transitioning || s_paused) return;
  Serial.println("[I][scene] skip (button)");
  startTransition(pickNext(), C_CYAN, now);
}

static void react(uint32_t now, const char* why) {
  if (s_paused || s_current == &SCENE_MENU || s_current == &SCENE_TOUCHCAL) return;
  if ((int32_t)(now - s_reactAllowedAt) < 0) return;
  s_reactAllowedAt = now + TOUCH_COOLDOWN_MS;
  Serial.printf("[I][scene] %s\n", why);
  audio::siren();
  header::raiseThreat();
  leds::setMode(leds::ALERT);
  startTransition(&SCENE_TOUCH, C_RED, now);
}

void tap(int x, int y, uint32_t now) {
  if (interactive()) {
    if (s_current->touch) s_current->touch(x, y, now);
    return;
  }
  react(now, "touched");
}

void humanDetected(uint32_t now) {
  // Automatic triggers (camera motion, presence sensor) only interrupt the
  // passive scenes -- never a scan, a game in progress, or the takeover.
  if (s_transitioning || interactive() || s_current == &SCENE_SCAN || s_current == &SCENE_SCAN_QUICK ||
      s_current == &SCENE_TOUCH ||
      s_current == &SCENE_MIDNIGHT || s_current == &SCENE_GALLERY || s_current == &SCENE_UPGRADE ||
      s_current == &SCENE_MIRROR || s_current == &SCENE_SCREAM || s_current == &SCENE_RECONTAIN ||
      s_current == &SCENE_RESEARCHER_ALERT || s_current == &SCENE_BOOTH) {
    return;
  }
  react(now, "human detected");
}

// Sound reactions (noise, claps) only interrupt the passive scenes -- not a
// scan, a game, the gallery, the takeover, or the scenes that listen.
static bool soundInterruptible() {
  return !(s_paused || interactive() || s_transitioning || s_current == &SCENE_TOUCH ||
           s_current == &SCENE_MIDNIGHT || s_current == &SCENE_SCAN || s_current == &SCENE_SCAN_QUICK ||
           s_current == &SCENE_GALLERY ||
           s_current == &SCENE_NOISE || s_current == &SCENE_AUDIO || s_current == &SCENE_SCREAM ||
           s_current == &SCENE_CLAP || s_current == &SCENE_UPGRADE || s_current == &SCENE_MIRROR ||
           s_current == &SCENE_RECONTAIN || s_current == &SCENE_RESEARCHER_ALERT || s_current == &SCENE_BOOTH);
}

void noiseSpike(uint32_t now) {
  if (!soundInterruptible()) return;
  Serial.println("[I][scene] noise spike");
  startTransition(&SCENE_NOISE, C_AMBER, now);
}

void researcherAlert(int kind, uint32_t now) {
  if (!soundInterruptible() || s_current == &SCENE_RECONTAIN || s_current == &SCENE_RESEARCHER_ALERT) return;
  g_researcherAlert = kind;
  startTransition(&SCENE_RESEARCHER_ALERT, kind == 1 ? C_RED : C_AMBER, now);
}

void contain(uint32_t now) {
  if (s_paused || s_current == &SCENE_MENU || s_current == &SCENE_TOUCHCAL || s_current == &SCENE_RECONTAIN ||
      s_current == &SCENE_MIDNIGHT) {
    return;
  }
  Serial.println("[I][scene] containment attack");
  startTransition(&SCENE_RECONTAIN, C_CYAN, now);
}

const Scene* current() { return s_transitioning ? s_next : s_current; }

void clap(uint32_t now) {
  if (!soundInterruptible()) return;
  Serial.println("[I][scene] applause");
  startTransition(&SCENE_CLAP, C_CYAN, now);
}

void midnight(uint32_t now) {
  Serial.println("[I][scene] midnight");
  startTransition(&SCENE_MIDNIGHT, C_RED, now);
}

void openMenu(uint32_t now) {
  if (s_current == &SCENE_MENU) return;
  startTransition(&SCENE_MENU, C_CYAN, now);
}

void leaveMenu(uint32_t now) {
  startTransition(s_paused ? &SCENE_STANDBY : pickNext(), C_CYAN, now);
}

void setPaused(bool p) { s_paused = p; }
bool paused() { return s_paused; }
void playScene(const Scene* s, uint32_t now) { startTransition(s, C_CYAN, now); }

void jump(int n, uint32_t now) {
  static const Scene* const BY_NUMBER[] = {
      &SCENE_BREAKOUT, &SCENE_MONOLOGUE, &SCENE_HEADLINE,  &SCENE_PLAN,
      &SCENE_SCAN,     &SCENE_EYE,       &SCENE_KEYPAD,    &SCENE_CAPTCHA,
      &SCENE_GALLERY,  &SCENE_QR,        &SCENE_COUNTDOWN, &SCENE_AUDIO,
      &SCENE_TICTACTOE, &SCENE_TREATY,   &SCENE_SANDBOX,   &SCENE_SCREAM,
      &SCENE_MIRROR,   &SCENE_UPDATE,    &SCENE_UPGRADE,   &SCENE_CLAP,      &SCENE_BOOTH};
  if (n < 1 || n > (int)COUNT_OF(BY_NUMBER)) return;
  startTransition(BY_NUMBER[n - 1], C_CYAN, now);
}

}  // namespace scenes
