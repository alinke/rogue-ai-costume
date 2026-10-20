#pragma once
// Button, touch and the optional presence sensor, polled from loop().
#include <Arduino.h>

namespace input {

struct Events {
  bool skip = false;      // button pressed
  bool presence = false;  // presence sensor (rising edge)
  // A touch gives two events. `press` fires the moment a finger lands (for the
  // "DO NOT TOUCH" reaction; its position is rough). `tap` fires when it lifts,
  // with the last position sampled before lift-off -- the GSL3680's first
  // samples of a press are unreliable, so buttons and keys use this one.
  bool press = false;
  bool tap = false;
  int tapX = 0, tapY = 0;
  bool menu = false;      // long press in the secret corner: wearer menu
  // While a finger is down: its position every poll (drawing, dragging).
  bool held = false;
  int heldX = 0, heldY = 0;
};

void begin();
Events poll(uint32_t now);

}  // namespace input
