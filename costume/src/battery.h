#pragma once
// Battery level from the cell voltage (GPIO52 divider). The IP5306 charger on
// this board can't report charge, so the percentage is estimated from a
// Li-ion discharge curve under load -- roughly right, not a fuel gauge.
#include <Arduino.h>

namespace battery {

void begin();
void tick(uint32_t now);   // samples about once a second
float volts();             // smoothed cell voltage
int percent();             // 0-100, or -1 if no battery is detected
bool charging();           // inferred from the voltage (no charge-status pin)
void printEvents();        // recent charging/unplugged changes (serial 'b')

}  // namespace battery
