// LEDLINK -- the jacket's LED strips, on a Gledopto GL-MC-001WL. It listens
// for the costume's Bluetooth beacon, which carries the LED effect the
// costume is running and the matrix's countdown number (bytes 11-12 of "RAI1",
// ../costume/src/researcher.cpp), and runs the costume's own LED code with
// them -- so the strips and the matrix do exactly what they would wired to the
// costume, through this board's 5 V output (no level shifter, no diode).
//
// Updates over Wi-Fi (its USB port is power only): it joins the costume's
// WIFI_NETWORKS (../costume/src/secrets.h) at boot and serves an update page
// at http://rogue-leds.local/update (or its IP, in the serial log).
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "gen/leds.h"

#if __has_include("../../costume/src/secrets.h")
#include "../../costume/src/secrets.h"
#endif

// The Arduino core frees the Bluetooth controller's memory at boot unless the
// program says it uses Bluetooth (its own BLE library does; NimBLE didn't here:
// init failed).
extern "C" bool btInUse() { return true; }

// ---- The costume's beacon ---------------------------------------------------------
static const uint8_t TAG_AI[4] = {'R', 'A', 'I', '1'};
static const uint8_t TAG_LEDS_ONLY[4] = {'R', 'L', 'D', '1'};  // the costume with its AI signal off
static volatile uint32_t s_seen = 0;
static volatile uint8_t s_mode = 0, s_number = 0;  // number + 1, 0 = none
static volatile uint8_t s_threat = 0, s_squad = 0, s_flags = 0, s_bright = 0;

// The scanners and guests' phones it hears itself (same as the costume:
// researcher.cpp): the nearest one's signal for the proximity glow, and
// whether an infected phone is close.
static const uint8_t TAG_SCANNER[4] = {'R', 'S', 'C', '1'};
struct Heard { uint8_t key[6]; float rssi; uint32_t seen; bool infected; bool handheld; bool used; };
static constexpr int MAX_HEARD = 24;
static Heard s_heardList[MAX_HEARD];
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static constexpr int INFECTED_DBM = -70;  // "close" for the purple ripple

static void hear(const uint8_t* key, float r, bool infected, bool handheld) {
  uint32_t now = millis();
  portENTER_CRITICAL(&s_mux);
  Heard* h = nullptr;
  for (auto& e : s_heardList)
    if (e.used && !memcmp(e.key, key, 6)) { h = &e; break; }
  if (!h) {
    h = &s_heardList[0];
    for (auto& e : s_heardList) {
      if (!e.used) { h = &e; break; }
      if ((int32_t)(e.seen - h->seen) < 0) h = &e;
    }
    *h = Heard{};
    memcpy(h->key, key, 6);
    h->used = true;
    h->rssi = r;
  }
  h->rssi = (now - h->seen > 3000) ? r : h->rssi + (r - h->rssi) * 0.25f;
  h->seen = now;
  h->infected = infected;
  h->handheld = handheld;
  portEXIT_CRITICAL(&s_mux);
}

static int hexByte(const std::string& s, int at) {
  if ((int)s.size() < at + 2) return -1;
  char b[3] = {s[at], s[at + 1], 0};
  char* end;
  long v = strtol(b, &end, 16);
  return *end ? -1 : (int)v;
}
static volatile uint32_t s_heard = 0, s_rai = 0, s_raiLen = 0;  // diagnostics (status page)
static const char* volatile s_bleState = "starting";
static volatile int s_nearestShown = -127;
static volatile bool s_infectedShown = false;

class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    s_heard++;
    float r = dev->getRSSI();
    // A guest's phone (the LAB-7 app): service UUID 52534331-SSTT-4C41-4237-...FF,
    // the last two digits its flags (2 = visibly infected).
    for (int i = (int)dev->getServiceUUIDCount() - 1; i >= 0; i--) {
      std::string u = dev->getServiceUUID(i).toString();
      for (auto& ch : u) ch = tolower(ch);
      if (u.size() == 36 && !u.compare(0, 9, "52534331-") && !u.compare(13, 11, "-4c41-4237-")) {
        int session = hexByte(u, 9), flags = hexByte(u, 34);
        if (session < 0) return;
        const uint8_t key[6] = {'i', 'O', 'S', 0, 0, (uint8_t)session};
        hear(key, r, flags >= 0 && (flags & 2), false);
        return;
      }
    }
    if (!dev->haveManufacturerData()) return;
    std::string m = dev->getManufacturerData();
    if (m.size() >= 8 && (uint8_t)m[0] == 0xFF && (uint8_t)m[1] == 0xFF && !memcmp(m.data() + 2, TAG_SCANNER, 4)) {
      hear(dev->getAddress().getVal(), r, false, true);   // a LAB-7 handheld scanner
      return;
    }
    bool ours = m.size() >= 6 && (!memcmp(m.data() + 2, TAG_AI, 4) || !memcmp(m.data() + 2, TAG_LEDS_ONLY, 4));
    if (ours) {
      s_rai++;
      s_raiLen = m.size();
    }
    if (m.size() < 13 || (uint8_t)m[0] != 0xFF || (uint8_t)m[1] != 0xFF || !ours) return;
    s_mode = (uint8_t)m[11];
    s_number = (uint8_t)m[12];
    s_threat = (uint8_t)m[6];
    if (m.size() >= 15) {
      s_squad = (uint8_t)m[13];
      s_flags = (uint8_t)m[14];
    }
    if (m.size() >= 16) s_bright = (uint8_t)m[15];
    s_seen = millis();
  }
};

static void radioTask(void*) {
  if (!NimBLEDevice::init("ROGUE-LEDS")) {
    s_bleState = "init FAILED";
    Serial.println("[ble] init failed");
    vTaskDelete(nullptr);
  }
  s_bleState = "init ok";
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new ScanCB(), true);  // every packet, not just new devices
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(80);
  scan->setMaxResults(0);                      // don't keep a list of everything heard
  s_bleState = scan->start(0, false, true) ? "scanning" : "scan start FAILED";
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!scan->isScanning()) scan->start(0, false, true);
  }
}

static const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_BROWNOUT: return "BROWNOUT (the supply sagged: too much LED current?)";
    case ESP_RST_PANIC: return "CRASH";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "WATCHDOG (hung)";
    case ESP_RST_SW: return "software (an update)";
    default: return "other";
  }
}

// ---- Wi-Fi updates ------------------------------------------------------------------
static WebServer s_web(80);
static HTTPUpdateServer s_updater;
static bool s_webUp = false;

static void wifiBegin() {
#ifdef WIFI_NETWORKS
  struct Net { const char* ssid; const char* pass; };
  static const Net NETS[] = WIFI_NETWORKS;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("rogue-leds");
  for (const auto& n : NETS) {
    WiFi.begin(n.ssid, n.pass);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(250);
    if (WiFi.status() == WL_CONNECTED) break;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[wifi] no known network: updates off until the next boot");
    WiFi.mode(WIFI_OFF);
    return;
  }
  MDNS.begin("rogue-leds");
  s_updater.setup(&s_web, "/update");
  s_web.on("/", [] {
    static const char* const MODES[] = {"idle", "alert", "scan", "breakout", "midnight", "noise", "off", "wary",
                                        "contain", "touch", "shutdown", "overload", "stealth"};
    uint32_t ago = s_seen ? millis() - s_seen : 0;
    char b[520];
    snprintf(b, sizeof(b),
             "ROGUE.AI LEDLINK -- firmware update at /update\ncostume: %s\neffect: %s\nmatrix number: %d\nup %lu s\n"
             "bluetooth: %s, %lu packets heard, %lu from the costume (%lu bytes)\nlast restart: %s\n"
             "threat %u, squad %u, talking %s; nearest handheld scanner %d dBm%s\nLED brightness %d%% (limit %d mA)\n",
             s_seen && ago < COSTUME_LOST_MS ? "seen" : "NOT SEEN", MODES[min<int>(s_mode, 12)], (int)s_number - 1,
             (unsigned long)(millis() / 1000), s_bleState, (unsigned long)s_heard, (unsigned long)s_rai, (unsigned long)s_raiLen, resetReason(), (unsigned)s_threat, (unsigned)s_squad,
             (s_flags & 1) ? "yes" : "no", s_nearestShown, s_infectedShown ? ", an infected phone close" : "",
             leds::brightness(), LED_MAX_MA);
    s_web.send(200, "text/plain", b);
  });
  s_web.begin();
  s_webUp = true;
  Serial.printf("[wifi] %s: update at http://%s/update (rogue-leds.local)\n", WiFi.SSID().c_str(),
                WiFi.localIP().toString().c_str());
#else
  Serial.println("[wifi] no WIFI_NETWORKS (costume/src/secrets.h): updates off");
#endif
}

// ---- Setup / loop -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[boot] ROGUE.AI LEDLINK (Gledopto)");
  wifiBegin();  // before Bluetooth: the radio's first connect is quicker alone
  leds::begin();
  leds::setMode(leds::IDLE);
  xTaskCreatePinnedToCore(radioTask, "radio", 6144, nullptr, 2, nullptr, 0);
}

void loop() {
  uint32_t now = millis();
  if (s_webUp) s_web.handleClient();
  bool costume = s_seen && (int32_t)(now - s_seen) < COSTUME_LOST_MS;
  static bool wasCostume = false;
  if (costume != wasCostume) {
    Serial.printf("[link] costume %s\n", costume ? "found" : "lost: idle on our own");
    wasCostume = costume;
  }
  // Who's near: the nearest LAB-7 handheld scanner heard in the last 5 s (the
  // researcher hunting -- not guests' phones: at a party one is always close),
  // and any infected phone within INFECTED_DBM.
  int nearest = -127;
  bool infected = false;
  portENTER_CRITICAL(&s_mux);
  for (auto& e : s_heardList) {
    if (!e.used || (int32_t)(now - e.seen) > COSTUME_LOST_MS) continue;
    if (e.handheld) nearest = max(nearest, (int)e.rssi);
    if (e.infected && e.rssi >= INFECTED_DBM) infected = true;
  }
  portEXIT_CRITICAL(&s_mux);
  leds::setProximity(nearest);
  leds::setInfectedNear(infected);
  s_nearestShown = nearest;
  s_infectedShown = infected;
  if (costume) {
    leds::setMode((leds::Mode)s_mode);
    leds::matrixNumber((int)s_number - 1);
    leds::setThreat(s_threat);
    leds::setSquad(s_squad);
    leds::setSpeaking(s_flags & 1);
    if (s_bright) leds::setBrightness(s_bright);  // the costume's LEDS - + (operator console / armband)
  } else {
    leds::setMode(leds::IDLE);
    leds::matrixNumber(-1);
    leds::setSquad(0);
    leds::setSpeaking(false);
  }
  leds::tick(now);
  static uint32_t nextLog = 0;
  if ((int32_t)(now - nextLog) >= 0) {
    nextLog = now + 5000;
    Serial.printf("[state] costume %s, effect %u, number %d, heap %u\n", costume ? "seen" : "lost",
                  (unsigned)s_mode, (int)s_number - 1, (unsigned)ESP.getFreeHeap());
  }
  delay(5);
}
