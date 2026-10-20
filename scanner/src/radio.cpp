#include "radio.h"

#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>

#include "config.h"

namespace radio {

static const uint8_t TAG_AI[4] = {'R', 'A', 'I', '1'};
static const uint8_t TAG_SCANNER[4] = {'R', 'S', 'C', '1'};

static volatile uint32_t s_lastSeen = 0;
static volatile float s_rssi = -100;
static volatile uint8_t s_state[4];
static volatile uint8_t s_trigger = 0;
static uint8_t s_session = 0;  // random per boot: the costume ignores a counter that restarted
static volatile bool s_dirty = true;
static volatile bool s_up = false;
// While a photo uploads, Wi-Fi and Bluetooth share the C6 and its buffers in
// the P4's scarce internal DMA RAM; listening can wait (advertising carries on).
static volatile bool s_scanPaused = false;

class ScanCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    if (!dev.haveManufacturerData()) return;
    String m = dev.getManufacturerData();
    if (m.length() < 10 || (uint8_t)m[0] != 0xFF || (uint8_t)m[1] != 0xFF) return;
    if (memcmp(m.c_str() + 2, TAG_AI, 4) != 0) return;
    uint32_t now = millis();
    float r = dev.getRSSI();
    s_rssi = ((int32_t)(now - s_lastSeen) > 3000) ? r : s_rssi + (r - s_rssi) * 0.25f;
    s_lastSeen = now;
    for (int i = 0; i < 4; i++) s_state[i] = (uint8_t)m[6 + i];
  }
};

static void setAdvert() {
  uint8_t data[2 + 4 + 2] = {0xFF, 0xFF};
  memcpy(data + 2, TAG_SCANNER, 4);
  data[6] = s_trigger;
  data[7] = s_session;  // changes every boot, so a reset counter isn't read as a press
  BLEAdvertisementData ad;
  ad.setFlags(0x06);
  ad.setManufacturerData(String((const char*)data, sizeof(data)));
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->stop();
  adv->setAdvertisementData(ad);
  adv->start();
}

static void task(void*) {
  BLEDevice::init("LAB-7 SCANNER");
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->setScanResponse(false);
  adv->setMinInterval(80);   // 50 ms: a CONTAIN press reaches the costume fast
  adv->setMaxInterval(120);
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ScanCB(), true);
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(SCAN_WINDOW_PERCENT);  // ms per 100 ms interval: listening duty cycle
  scan->start(0, nullptr, false);
  Serial.println("[radio] up: advertising as the scanner, listening for the AI");
  s_up = true;
  uint32_t scanStartedAt = millis();
  for (;;) {
    if (s_dirty) {
      s_dirty = false;
      setAdvert();
    }
    if (s_scanPaused) {
      if (scan->isScanning()) scan->stop();
    } else if (!scan->isScanning()) {
      scan->start(0, nullptr, false);
    } else if (millis() - scanStartedAt > 10000) {
      // The library keeps every advertiser it hears and never frees them
      // (phones change address every few minutes: internal RAM runs out at
      // a party). Restarting the scan clears that list.
      scan->stop();
      scan->start(0, nullptr, false);
      scanStartedAt = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

bool up() { return s_up; }
void pauseScan(bool paused) { s_scanPaused = paused; }

void begin() {
  s_session = 1 + esp_random() % 255;
  xTaskCreatePinnedToCore(task, "radio", 6144, nullptr, 2, nullptr, 0);
}

AiState ai() {
  AiState a;
  uint32_t last = s_lastSeen;  // signed below: the BLE task may update it mid-read
  a.seen = last && (int32_t)(millis() - last) < LOST_MS;
  a.rssi = (int)s_rssi;
  a.threat = s_state[0];
  a.activity = s_state[1];
  a.scanned = s_state[2];
  a.contained = s_state[3];
  return a;
}

void fireContain() {
  s_trigger = s_trigger + 1;
  s_dirty = true;
  Serial.printf("[radio] CONTAIN #%u\n", s_trigger);
}

}  // namespace radio
