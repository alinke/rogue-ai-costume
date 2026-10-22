#include "radio.h"

#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>

#include "config.h"

namespace radio {

static const uint8_t TAG_AI[4] = {'R', 'A', 'I', '1'};
static const uint8_t TAG_SCANNER[4] = {'R', 'S', 'C', '1'};

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_coreSeen = 0;
static float s_coreRssi = -100;
static uint8_t s_state[4];

// More than one scanner can be out (Tab5, Waveshare, iPhone): each tracked on
// its own, the nearest one counts.
struct Scanner {
  uint8_t key[6]; float rssi; uint32_t seen; bool used;
  uint8_t trigger, session;           // from the beacon
  int seenTrigger = -1; uint8_t seenSession; bool wasSeen;  // beamFired()'s side
};
static constexpr int MAX_SCANNERS = 4;
static Scanner s_scanners[MAX_SCANNERS];

static void recordHunter(const uint8_t* key, float r, uint8_t trigger, uint8_t session) {
  uint32_t now = millis();
  portENTER_CRITICAL(&s_mux);
  Scanner* slot = nullptr;
  for (auto& s : s_scanners)
    if (s.used && !memcmp(s.key, key, 6)) slot = &s;
  if (!slot) {  // a free slot, or the one heard longest ago
    for (auto& s : s_scanners)
      if (!slot || !s.used || (slot->used && s.seen < slot->seen)) slot = &s;
    *slot = Scanner();
    memcpy(slot->key, key, 6);
    slot->used = true;
    slot->rssi = r;
  }
  slot->rssi = (now - slot->seen > 3000) ? r : slot->rssi + (r - slot->rssi) * 0.25f;
  slot->seen = now;
  slot->trigger = trigger;
  slot->session = session;
  portEXIT_CRITICAL(&s_mux);
}

static int hexByte(const String& s, int at) { return (int)strtol(s.substring(at, at + 2).c_str(), nullptr, 16); }

class ScanCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    float r = dev.getRSSI();
    // The iPhone app: its newest UUID is the last one (the library appends).
    for (int i = dev.getServiceUUIDCount() - 1; i >= 0; i--) {
      String u = dev.getServiceUUID(i).toString();
      u.toLowerCase();
      if (u.length() == 36 && u.startsWith("52534331-") && u.endsWith("-4c41-4237-000000000000")) {
        uint8_t key[6] = {'i', 'O', 'S', 0, 0, (uint8_t)hexByte(u, 9)};
        recordHunter(key, r, (uint8_t)hexByte(u, 11), key[5]);
        return;
      }
    }
    if (!dev.haveManufacturerData()) return;
    String m = dev.getManufacturerData();
    if (m.length() < 8 || (uint8_t)m[0] != 0xFF || (uint8_t)m[1] != 0xFF) return;
    if (m.length() >= 10 && !memcmp(m.c_str() + 2, TAG_AI, 4)) {
      uint32_t now = millis();
      portENTER_CRITICAL(&s_mux);
      s_coreRssi = (now - s_coreSeen > 3000) ? r : s_coreRssi + (r - s_coreRssi) * 0.25f;
      s_coreSeen = now;
      for (int i = 0; i < 4; i++) s_state[i] = (uint8_t)m[6 + i];
      portEXIT_CRITICAL(&s_mux);
    } else if (!memcmp(m.c_str() + 2, TAG_SCANNER, 4)) {
      BLEAddress ba = dev.getAddress();
      recordHunter((const uint8_t*)ba.getNative(), r, (uint8_t)m[6], (uint8_t)m[7]);
    }
  }
};

static volatile bool s_stopReq = false, s_stopped = false;

static void task(void*) {
  BLEDevice::init("ARM-7");
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ScanCB(), true);  // every packet: live signal strength
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(40);  // listening 40% of the time
  scan->start(0, nullptr, false);
  Serial.printf("[radio] listening (heap %u)\n", (unsigned)ESP.getFreeHeap());
  uint32_t startedAt = millis();
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(500));
    if (s_stopReq) {
      scan->stop();
      BLEDevice::deinit(true);
      s_stopped = true;
      vTaskDelete(nullptr);
    }
    if (!scan->isScanning() || millis() - startedAt > 10000) {
      // The library keeps every advertiser it hears until the scan restarts
      // (phones change address every few minutes: RAM runs out at a party).
      scan->stop();
      scan->start(0, nullptr, false);
      startedAt = millis();
    }
  }
}

void shutdown() {
  s_stopReq = true;
  for (int i = 0; i < 40 && !s_stopped; i++) delay(50);
}

void begin() { xTaskCreatePinnedToCore(task, "radio", 6144, nullptr, 2, nullptr, 0); }

Core core() {
  Core c;
  portENTER_CRITICAL(&s_mux);
  uint32_t last = s_coreSeen;
  c.rssi = (int)s_coreRssi;
  c.threat = s_state[0]; c.activity = s_state[1]; c.scanned = s_state[2]; c.contained = s_state[3];
  portEXIT_CRITICAL(&s_mux);
  c.seen = last && (int32_t)(millis() - last) < LOST_MS;
  return c;
}

Hunter hunter() {
  Hunter h;
  uint32_t now = millis();
  float best = -127;
  portENTER_CRITICAL(&s_mux);
  for (auto& s : s_scanners)
    if (s.used && (int32_t)(now - s.seen) < LOST_MS && s.rssi > best) best = s.rssi;
  portEXIT_CRITICAL(&s_mux);
  h.seen = best > -127;
  h.rssi = h.seen ? (int)best : -100;
  return h;
}

bool beamFired() {
  bool fired = false;
  uint32_t now = millis();
  portENTER_CRITICAL(&s_mux);
  for (auto& e : s_scanners) {
    if (!e.used) continue;
    bool inRange = (int32_t)(now - e.seen) < LOST_MS;
    if (inRange) {
      // Only a change within the same session, while continuously in range,
      // is a press (not a scanner that rebooted or just walked back in).
      if (e.seenTrigger < 0 || !e.wasSeen || e.session != e.seenSession) {
        e.seenTrigger = e.trigger;
        e.seenSession = e.session;
      } else if (e.trigger != e.seenTrigger) {
        e.seenTrigger = e.trigger;
        fired = true;
      }
    }
    e.wasSeen = inRange;
  }
  portEXIT_CRITICAL(&s_mux);
  return fired;
}

}  // namespace radio
