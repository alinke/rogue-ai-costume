#include "radio.h"

#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <esp_random.h>

#include "config.h"
#include "../../ledlink/src/jacket_link.h"

namespace radio {

static const uint8_t TAG_AI[4] = {'R', 'A', 'I', '1'};
static const uint8_t TAG_SCANNER[4] = {'R', 'S', 'C', '1'};
static const uint8_t TAG_REMOTE[4] = {'R', 'C', 'T', '1'};
static const uint8_t TAG_LEDS_ONLY[4] = {'R', 'L', 'D', '1'};  // the costume with its AI signal off

// The operator console: one command at a time, advertised for SEND_MS or
// until the costume's beacon carries its seq back.
static volatile uint8_t s_seq = 0, s_cmd = 0, s_ack = 0;
static volatile bool s_sendReq = false;
// The jacket's commands go out the same way, one at a time (a new press
// replaces whatever was being sent).
static volatile bool s_toJacket = false;
static volatile uint8_t s_jArgs[3];
static uint32_t s_jacketSeen = 0;
static float s_jacketRssi = -100;
static uint8_t s_jState[10];

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
      // The app uses the last 12 digits for its own flags (the infection game).
      if (u.length() == 36 && u.startsWith("52534331-") && u.substring(13, 24) == "-4c41-4237-") {
        uint8_t key[6] = {'i', 'O', 'S', 0, 0, (uint8_t)hexByte(u, 9)};
        recordHunter(key, r, (uint8_t)hexByte(u, 11), key[5]);
        return;
      }
    }
    if (!dev.haveManufacturerData()) return;
    String m = dev.getManufacturerData();
    if (m.length() < 8 || (uint8_t)m[0] != 0xFF || (uint8_t)m[1] != 0xFF) return;
    if (m.length() >= 2 + jacket::STATUS_LEN && !memcmp(m.c_str() + 2, jacket::TAG_STATUS, 4)) {
      uint32_t now = millis();
      portENTER_CRITICAL(&s_mux);
      s_jacketRssi = (now - s_jacketSeen > 3000) ? r : s_jacketRssi + (r - s_jacketRssi) * 0.25f;
      s_jacketSeen = now;
      memcpy(s_jState, m.c_str() + 6, 10);
      portEXIT_CRITICAL(&s_mux);
    } else if (m.length() >= 10 && !memcmp(m.c_str() + 2, TAG_AI, 4)) {
      uint32_t now = millis();
      portENTER_CRITICAL(&s_mux);
      s_coreRssi = (now - s_coreSeen > 3000) ? r : s_coreRssi + (r - s_coreRssi) * 0.25f;
      s_coreSeen = now;
      for (int i = 0; i < 4; i++) s_state[i] = (uint8_t)m[6 + i];
      if (m.length() >= 11) s_ack = (uint8_t)m[10];  // the last console command it carried out
      portEXIT_CRITICAL(&s_mux);
    } else if (m.length() >= 11 && !memcmp(m.c_str() + 2, TAG_LEDS_ONLY, 4)) {
      s_ack = (uint8_t)m[10];  // out of the game, but still confirms console commands
    } else if (!memcmp(m.c_str() + 2, TAG_SCANNER, 4)) {
      BLEAddress ba = dev.getAddress();
      recordHunter((const uint8_t*)ba.getNative(), r, (uint8_t)m[6], (uint8_t)m[7]);
    }
  }
};

static volatile bool s_stopReq = false, s_stopped = false;

static void advertise(BLEAdvertising* adv) {
  uint8_t data[2 + 4 + 7] = {0xFF, 0xFF};
  size_t len;
  if (s_toJacket) {  // RJC1 seq cmd a b c key
    memcpy(data + 2, jacket::TAG_COMMAND, 4);
    data[6] = s_seq;
    data[7] = s_cmd;
    data[8] = s_jArgs[0];
    data[9] = s_jArgs[1];
    data[10] = s_jArgs[2];
    data[11] = JACKET_LINK_KEY & 0xFF;
    data[12] = JACKET_LINK_KEY >> 8;
    len = 13;
  } else {           // RCT1 target seq cmd key
    memcpy(data + 2, TAG_REMOTE, 4);
    data[6] = ARM_TARGET;
    data[7] = s_seq;
    data[8] = s_cmd;
    data[9] = ARM_LINK_KEY & 0xFF;
    data[10] = ARM_LINK_KEY >> 8;
    len = 11;
  }
  BLEAdvertisementData ad;
  ad.setFlags(0x06);
  ad.setManufacturerData(String((const char*)data, len));
  adv->stop();
  adv->setAdvertisementData(ad);
  adv->start();
}

static void task(void*) {
  BLEDevice::init("ARM-7");
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->setScanResponse(false);
  adv->setMinInterval(48);  // 30-50 ms: the costume's scan window catches it fast
  adv->setMaxInterval(80);
  bool sending = false;
  uint32_t sendUntil = 0;
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ScanCB(), true);  // every packet: live signal strength
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(40);  // listening 40% of the time
  scan->start(0, nullptr, false);
  Serial.printf("[radio] listening (heap %u)\n", (unsigned)ESP.getFreeHeap());
  uint32_t startedAt = millis();
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(50));
    if (s_sendReq) {
      s_sendReq = false;
      advertise(adv);
      sending = true;
      sendUntil = millis() + SEND_MS;
    } else if (sending && ((int32_t)(millis() - sendUntil) >= 0 || (s_toJacket ? s_jState[4] == s_seq : s_ack == s_seq))) {
      adv->stop();
      sending = false;
    }
    if (s_stopReq) {
      adv->stop();
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

void begin() {
  s_seq = (uint8_t)esp_random();  // a fresh start each boot, so the costume can't take it for an old press
  xTaskCreatePinnedToCore(task, "radio", 6144, nullptr, 2, nullptr, 0);
}

uint8_t send(Command cmd) {
  s_toJacket = false;
  s_cmd = cmd;
  s_seq = s_seq + 1;
  s_sendReq = true;
  Serial.printf("[console] send %u (seq %u)\n", (unsigned)cmd, (unsigned)s_seq);
  return s_seq;
}

bool acked(uint8_t seq) { return s_ack == seq; }

uint8_t sendJacket(uint8_t cmd, uint8_t a, uint8_t b, uint8_t c) {
  s_toJacket = true;
  s_cmd = cmd;
  s_jArgs[0] = a;
  s_jArgs[1] = b;
  s_jArgs[2] = c;
  s_seq = s_seq + 1;
  s_sendReq = true;
  Serial.printf("[jacket] send %u (%u %u %u) seq %u\n", (unsigned)cmd, (unsigned)a, (unsigned)b, (unsigned)c, (unsigned)s_seq);
  return s_seq;
}

bool jacketAcked(uint8_t seq) { return s_jState[4] == seq; }

Jacket jacket() {
  Jacket j;
  portENTER_CRITICAL(&s_mux);
  uint32_t last = s_jacketSeen;
  j.rssi = (int)s_jacketRssi;
  j.look = s_jState[0]; j.bright = s_jState[1]; j.speed = s_jState[2]; j.flags = s_jState[3]; j.ack = s_jState[4];
  j.color = ((uint32_t)s_jState[5] << 16) | ((uint32_t)s_jState[6] << 8) | s_jState[7];
  j.turn = s_jState[8]; j.bpm = s_jState[9];
  portEXIT_CRITICAL(&s_mux);
  j.seen = last && (int32_t)(millis() - last) < LOST_MS;
  return j;
}

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
