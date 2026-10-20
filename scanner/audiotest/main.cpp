// Minimal Tab5 speaker test: display bring-up (expanders, I2C) + the Sidekick's
// Tab5Audio tone, nothing else. Build: pio run -e tab5-audiotest
#include <Arduino.h>
#include "LGFX_Config.hpp"
#include "Tab5Audio.hpp"
static LGFX lcd;
void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.setBrightness(128);
  lcd.fillScreen(TFT_BLUE);
  lcd.setTextSize(4);
  lcd.drawString("AUDIO TEST", 100, 100);
}
void loop() {
  Serial.println("[audiotest] tone");
  tab5audio::playTone(880, 600, 0.5f);
  delay(1000);
}
