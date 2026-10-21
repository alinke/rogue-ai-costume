#include "researcher.h"

#include "config.h"

#if SCANNER_LINK_ENABLED

#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>

#include "esp32-hal-hosted.h"

namespace researcher {

// Beacon format (manufacturer data): 0xFFFF (no company), a 4-byte tag, then
// the payload.
//   AI:      "RAI1" threat, activity, humans scanned, times contained
//   Scanner: "RSC1" trigger counter, session id (random per scanner boot)
static const uint8_t TAG_AI[4] = {'R', 'A', 'I', '1'};
static const uint8_t TAG_SCANNER[4] = {'R', 'S', 'C', '1'};

static volatile bool s_up = false;
// Every scanner in range, tracked on its own (by Bluetooth address): with the
// Waveshare and the Tab5 both on, a single shared counter saw the session id
// flip back and forth and took it for a reboot every time -- presses were lost.
struct Scanner {
  uint8_t addr[6];
  bool used;
  uint32_t lastSeenMs;
  float rssi;
  int trigger, session;          // from the beacon (BLE task)
  int seenTrigger, seenSession;  // as handled by poll() (main loop)
  bool wasSeen;
};
static constexpr int MAX_SCANNERS = 4;
static Scanner s_scanners[MAX_SCANNERS];
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile float s_rssi = -100;  // the nearest scanner's, for rssi()
static volatile uint8_t s_state[4] = {0, 0, 0, 0};
static volatile bool s_stateDirty = true;

static Zone s_zone = NONE;

// One beacon heard from a scanner: its key (Bluetooth address, or a stand-in),
// signal, CONTAIN counter and session id.
static void record(const uint8_t key[6], float r, int trigger, int session) {
  uint32_t now = millis();
  portENTER_CRITICAL(&s_mux);
  Scanner* sc = nullptr;
  for (auto& e : s_scanners) {
    if (e.used && !memcmp(e.addr, key, 6)) { sc = &e; break; }
  }
  if (!sc) {  // new scanner: a free slot, or the one heard least recently
    sc = &s_scanners[0];
    for (auto& e : s_scanners) {
      if (!e.used) { sc = &e; break; }
      if ((int32_t)(e.lastSeenMs - sc->lastSeenMs) < 0) sc = &e;
    }
    *sc = Scanner{};
    memcpy(sc->addr, key, 6);
    sc->used = true;
    sc->seenTrigger = sc->seenSession = -1;
    sc->lastSeenMs = now - 10000;
  }
  // Smooth the (noisy) signal strength; restart after a gap.
  sc->rssi = ((int32_t)(now - sc->lastSeenMs) > 3000) ? r : sc->rssi + (r - sc->rssi) * 0.25f;
  sc->lastSeenMs = now;
  sc->trigger = trigger;
  sc->session = session;
  portEXIT_CRITICAL(&s_mux);
}

static int hexByte(const char* p) {
  auto v = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
  int a = v(p[0]), b = v(p[1]);
  return a < 0 || b < 0 ? -1 : a * 16 + b;
}

class ScanCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    float r = dev.getRSSI();
    // The iPhone app (ios/): iOS can't advertise manufacturer data, so it sends
    // a service UUID 52534331-SSTT-4C41-4237-000000000000 ("RSC1", session,
    // trigger). Keyed by session, not address: iPhones change their address
    // every ~15 minutes.
    if (dev.haveServiceUUID()) {
      for (int i = 0; i < dev.getServiceUUIDCount(); i++) {
        String u = dev.getServiceUUID(i).toString();
        u.toLowerCase();
        if (u.length() == 36 && u.startsWith("52534331-") && u.endsWith("-4c41-4237-000000000000")) {
          int session = hexByte(u.c_str() + 9), trigger = hexByte(u.c_str() + 11);
          if (session < 0 || trigger < 0) return;
          const uint8_t key[6] = {'i', 'O', 'S', 0, 0, (uint8_t)session};
          record(key, r, trigger, session);
          return;
        }
      }
    }
    // The ESP32 scanners: manufacturer data 0xFFFF "RSC1" + trigger + session.
    if (!dev.haveManufacturerData()) return;
    String m = dev.getManufacturerData();
    if (m.length() < 8 || (uint8_t)m[0] != 0xFF || (uint8_t)m[1] != 0xFF) return;
    if (memcmp(m.c_str() + 2, TAG_SCANNER, 4) != 0) return;
    BLEAddress ba = dev.getAddress();
    record((const uint8_t*)ba.getNative(), r, (uint8_t)m[6], (uint8_t)m[7]);
  }
};

static void setAdvert() {
  uint8_t data[2 + 4 + 4] = {0xFF, 0xFF};
  memcpy(data + 2, TAG_AI, 4);
  for (int i = 0; i < 4; i++) data[6 + i] = s_state[i];
  BLEAdvertisementData ad;
  ad.setFlags(0x06);
  ad.setManufacturerData(String((const char*)data, sizeof(data)));
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->stop();
  adv->setAdvertisementData(ad);
  adv->start();
}

static volatile bool s_scanPaused = false;  // see pauseScan()
static uint32_t s_scanStartedAt = 0;

static void task(void*) {
  // The core's variant has the Tab5's SDIO pins: point ESP-Hosted at this
  // board's C6 (CLK 18, CMD 19, D0-D3 14-17, reset 54) before BLE starts it.
  hostedSetPins(18, 19, 14, 15, 16, 17, 54);
  uint32_t t0 = millis();
  if (!BLEDevice::init("ROGUE.AI")) {
    Serial.println("[E][researcher] BLE init failed (C6 co-processor link)");
    vTaskDelete(nullptr);
    return;
  }
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->setScanResponse(false);
  adv->setMinInterval(160);  // 100 ms (0.625 ms units)
  adv->setMaxInterval(240);  // 150 ms
  setAdvert();
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ScanCB(), true);  // every packet: fresh RSSI
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(60);  // listening 60% of the time
  scan->start(0, nullptr, false);
  s_up = true;
  Serial.printf("[I][researcher] BLE up in %u ms: advertising as the AI, listening for the scanner\n",
                (unsigned)(millis() - t0));
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(500));
    if (s_stateDirty) {
      s_stateDirty = false;
      setAdvert();
    }
    if (s_scanPaused) {
      if (scan->isScanning()) scan->stop();
    } else if (!scan->isScanning()) {
      scan->start(0, nullptr, false);  // keep listening
    } else if (millis() - s_scanStartedAt > 10000) {
      // The library keeps every advertiser it hears and never frees them
      // (phones change address every few minutes: a party's worth ran
      // internal RAM out). Restarting the scan clears that list.
      scan->stop();
      scan->start(0, nullptr, false);
      s_scanStartedAt = millis();
    }
  }
}

bool radioUp() { return s_up; }

// While a photo uploads, Wi-Fi and Bluetooth share the C6 and its SDIO buffers
// in the P4's scarce internal DMA RAM; the scan's stream of adverts is the
// part that can wait. Advertising (the scanner seeing the AI) carries on.
void pauseScan(bool paused) { s_scanPaused = paused; }

void begin() { xTaskCreatePinnedToCore(task, "researcher", 6144, nullptr, 2, nullptr, 0); }

Events poll(uint32_t now) {
  Events ev;
  if (!s_up) return ev;
  Scanner snap[MAX_SCANNERS];
  portENTER_CRITICAL(&s_mux);
  memcpy(snap, s_scanners, sizeof(snap));
  portEXIT_CRITICAL(&s_mux);

  // The zone follows the nearest scanner in range. Signed time compares: the
  // BLE task (other core) may have logged a packet a moment after `now`.
  bool seen = false;
  float r = -127;
  for (auto& e : snap) {
    if (e.used && (int32_t)(now - e.lastSeenMs) < SCANNER_LOST_MS) {
      seen = true;
      r = max(r, e.rssi);
    }
  }
  if (seen) s_rssi = r;
  Zone z = NONE;
  if (seen) {
    // Zones with hysteresis: moving up needs the threshold, moving down needs
    // to fall HYSTERESIS dB below it.
    auto above = [&](int th, Zone at) { return r >= th - (s_zone >= at ? SCANNER_HYSTERESIS_DB : 0); };
    z = above(SCANNER_CLOSE_DBM, CLOSE) ? CLOSE : above(SCANNER_NEAR_DBM, NEAR) ? NEAR : DETECTED;
  }
  if (z != s_zone) {
    ev.zoneChanged = true;
    ev.previous = s_zone;
    ev.zone = z;
    Serial.printf("[I][researcher] researcher %s (%.0f dBm)\n", zoneName(z), r);
    s_zone = z;
  }

  // CONTAIN: each scanner's counter on its own. Only a change within the same
  // session (no reboot), while continuously in range, is a press.
  for (int i = 0; i < MAX_SCANNERS; i++) {
    Scanner& live = s_scanners[i];  // poll-side fields are only touched here
    const Scanner& e = snap[i];
    if (!e.used) continue;
    bool inRange = (int32_t)(now - e.lastSeenMs) < SCANNER_LOST_MS;
    if (inRange) {
      if (live.seenTrigger < 0 || !live.wasSeen || e.session != live.seenSession) {
        live.seenTrigger = e.trigger;
        live.seenSession = e.session;
      } else if (e.trigger != live.seenTrigger) {
        live.seenTrigger = e.trigger;
        ev.contain = true;
        Serial.printf("[I][researcher] CONTAIN from scanner %02x:%02x (%.0f dBm)\n", e.addr[4], e.addr[5], e.rssi);
      }
    }
    live.wasSeen = inRange;
  }
  return ev;
}

Zone zone() { return s_zone; }
int rssi() { return (int)s_rssi; }

void setState(uint8_t threat, Activity activity, uint8_t scanned, uint8_t contained) {
  uint8_t n[4] = {threat, (uint8_t)activity, scanned, contained};
  for (int i = 0; i < 4; i++) {
    if (s_state[i] != n[i]) {
      s_state[i] = n[i];
      s_stateDirty = true;
    }
  }
}

const char* zoneName(Zone z) {
  static const char* const N[] = {"gone", "detected", "near", "close"};
  return N[z];
}

}  // namespace researcher

#else

namespace researcher {
void begin() {}
Events poll(uint32_t) { return Events(); }
Zone zone() { return NONE; }
int rssi() { return -100; }
void setState(uint8_t, Activity, uint8_t, uint8_t) {}
const char* zoneName(Zone) { return "off"; }
}  // namespace researcher

#endif
