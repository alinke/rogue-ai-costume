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
static volatile uint32_t s_seen = 0;
static volatile uint8_t s_mode = 0, s_number = 0;  // number + 1, 0 = none
static volatile uint32_t s_heard = 0, s_rai = 0, s_raiLen = 0;  // diagnostics (status page)
static const char* volatile s_bleState = "starting";

class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    s_heard++;
    if (!dev->haveManufacturerData()) return;
    std::string m = dev->getManufacturerData();
    if (m.size() >= 6 && !memcmp(m.data() + 2, TAG_AI, 4)) {
      s_rai++;
      s_raiLen = m.size();
    }
    if (m.size() < 13 || (uint8_t)m[0] != 0xFF || (uint8_t)m[1] != 0xFF) return;
    if (memcmp(m.data() + 2, TAG_AI, 4) != 0) return;
    s_mode = (uint8_t)m[11];
    s_number = (uint8_t)m[12];
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
    static const char* const MODES[] = {"idle", "alert", "scan", "breakout", "midnight", "noise", "off", "wary", "contain"};
    uint32_t ago = s_seen ? millis() - s_seen : 0;
    char b[300];
    snprintf(b, sizeof(b),
             "ROGUE.AI LEDLINK -- firmware update at /update\ncostume: %s\neffect: %s\nmatrix number: %d\nup %lu s\n"
             "bluetooth: %s, %lu packets heard, %lu from the costume (%lu bytes)\n",
             s_seen && ago < COSTUME_LOST_MS ? "seen" : "NOT SEEN", MODES[min<int>(s_mode, 8)], (int)s_number - 1,
             (unsigned long)(millis() / 1000), s_bleState, (unsigned long)s_heard, (unsigned long)s_rai, (unsigned long)s_raiLen);
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
  if (costume) {
    leds::setMode((leds::Mode)s_mode);
    leds::matrixNumber((int)s_number - 1);
  } else {
    leds::setMode(leds::IDLE);
    leds::matrixNumber(-1);
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
