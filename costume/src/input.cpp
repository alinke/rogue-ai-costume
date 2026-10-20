#include "input.h"

#include "config.h"
#include "gfx.h"

namespace input {

static constexpr uint32_t DEBOUNCE_MS = 30;
static constexpr uint32_t TOUCH_POLL_MS = 25;  // each poll is an I2C read

static bool s_btnStable = true;  // HIGH = released (pull-up)
static bool s_btnRaw = true;
static uint32_t s_btnChangedAt = 0;

static bool s_touchDown = false;
static uint32_t s_nextTouchPoll = 0;
static int s_firstX, s_firstY, s_lastX, s_lastY, s_samples;
static bool s_suppressRelease = false;  // the press opened the menu

// A press in the secret corner is held back: released quickly it's an ordinary
// tap, held for MENU_HOLD_MS it opens the wearer menu instead.
static bool s_cornerPending = false;
static uint32_t s_cornerSince = 0;

#if PRESENCE_SENSOR_ENABLED
static bool s_presence = false;
#endif

void begin() {
#if BUTTON_PIN >= 0
  pinMode(BUTTON_PIN, INPUT_PULLUP);
#endif
#if PRESENCE_SENSOR_ENABLED
  pinMode(PRESENCE_SENSOR_PIN, INPUT_PULLDOWN);
#endif
}

static bool inSecretCorner(int x, int y) { return x < MENU_CORNER_W && y < MENU_CORNER_H; }

Events poll(uint32_t now) {
  Events ev;

#if BUTTON_PIN >= 0
  bool raw = digitalRead(BUTTON_PIN);
  if (raw != s_btnRaw) {
    s_btnRaw = raw;
    s_btnChangedAt = now;
  } else if (raw != s_btnStable && now - s_btnChangedAt >= DEBOUNCE_MS) {
    s_btnStable = raw;
    if (!raw) {
      Serial.println("[I][input] button");
      ev.skip = true;
    }
  }
#endif

  if ((int32_t)(now - s_nextTouchPoll) >= 0) {
    s_nextTouchPoll = now + TOUCH_POLL_MS;
    int x, y;
    bool down = gfx::getTouch(&x, &y);
    if (down) {
      if (!s_touchDown) {  // finger landed
        s_firstX = x;
        s_firstY = y;
        s_samples = 0;
        s_suppressRelease = false;
        if (inSecretCorner(x, y)) {
          s_cornerPending = true;  // maybe the menu: decide on hold or release
          s_cornerSince = now;
        } else {
          ev.press = true;
        }
      }
      s_lastX = x;
      s_lastY = y;
      s_samples++;
      if (!s_cornerPending && !s_suppressRelease) {
        ev.held = true;
        ev.heldX = x;
        ev.heldY = y;
      }
      if (s_cornerPending && now - s_cornerSince >= MENU_HOLD_MS) {
        s_cornerPending = false;
        s_suppressRelease = true;
        Serial.println("[I][input] wearer menu");
        ev.menu = true;
      }
    } else if (s_touchDown) {  // finger lifted
      if (s_cornerPending) {
        s_cornerPending = false;
        ev.press = true;  // a short press in the corner is just a touch
      }
      if (!s_suppressRelease) {
        ev.tap = true;
        ev.tapX = s_lastX;
        ev.tapY = s_lastY;
        Serial.printf("[I][input] tap at %d,%d (landed at %d,%d, %d samples)\n", s_lastX, s_lastY,
                      s_firstX, s_firstY, s_samples);
      }
    }
    s_touchDown = down;
  }

#if PRESENCE_SENSOR_ENABLED
  bool present = digitalRead(PRESENCE_SENSOR_PIN);
  if (present && !s_presence) {
    Serial.println("[I][input] presence sensor");
    ev.presence = true;
  }
  s_presence = present;
#endif

  return ev;
}

}  // namespace input
