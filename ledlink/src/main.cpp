// LEDLINK -- the jacket's LED strips, on a Gledopto GL-MC-001WL. It listens
// for the costume's Bluetooth beacon, which carries the LED effect the
// costume is running and the matrix's countdown number (bytes 11-12 of "RAI1",
// ../costume/src/researcher.cpp), and runs the costume's own LED code with
// them -- so the strips and the matrix do exactly what they would wired to the
// costume, through this board's 5 V output (no level shifter, no diode).
//
// Worn without the costume, it runs its own looks (looks.cpp: themes, the
// holidays, bike turn signals, a music visualizer, your heart rate), set from
// the armband or the iPhone app over Bluetooth (jacket_link.h). The look ROGUE
// AI is the costume's show, as above.
//
// Updates over Wi-Fi (its USB port is power only): it joins the costume's
// WIFI_NETWORKS (../costume/src/secrets.h) at boot and serves an update page
// at http://rogue-leds.local/update (or its IP, in the serial log).
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "gen/leds.h"
#include "jacket_link.h"
#include "looks.h"

// Not in a PUBLIC_BUILD (the web-flasher image): no Wi-Fi, so no self-update.
#if !defined(PUBLIC_BUILD) && __has_include("../../costume/src/secrets.h")
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
// More than one costume on (a spare on the bench, a backup in the next room):
// follow the nearest -- the one being worn, a foot away in its pocket. Another
// takes over only once it's clearly stronger (8 dB) or ours goes quiet for 3 s.
static uint8_t s_ownAddr[6];
static float s_ownRssi = -127;
static volatile uint32_t s_ownSeen = 0, s_otherSeen = 0;  // s_otherSeen: a costume ignored (status page)

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

// ---- The jacket's remote (jacket_link.h) ---------------------------------------------
// The armband advertises a command for a few seconds: carried out once per seq.
static volatile uint8_t s_ackSeq = 0;
static volatile bool s_haveSeq = false;
static volatile uint32_t s_cmds = 0;  // diagnostics

static void jacketCommand(const uint8_t* b) {  // seq cmd a b c keyLo keyHi
  if ((b[5] | (b[6] << 8)) != JACKET_LINK_KEY) return;
  if (s_haveSeq && b[0] == s_ackSeq) return;  // the same press, repeated
  s_ackSeq = b[0];
  s_haveSeq = true;
  s_cmds++;
  looks::command(b[1], b[2], b[3], b[4]);
}

// The iPhone app connects instead (it can't advertise manufacturer data).
static NimBLECharacteristic* s_statusChar = nullptr;
static volatile int s_appConns = 0;
static volatile bool s_layoutSaved = false;  // the LED layout is the app's, not config.h's
static volatile bool s_appTest = false;      // the app started a test pattern

class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* srv, NimBLEConnInfo& info) override {
    s_appConns = srv->getConnectedCount();
    Serial.printf("[app] connected (%d)\n", (int)s_appConns);
    NimBLEDevice::getAdvertising()->start();  // keep advertising: the armband reads our status from it
  }
  void onDisconnect(NimBLEServer* srv, NimBLEConnInfo& info, int reason) override {
    s_appConns = srv->getConnectedCount();
    Serial.printf("[app] disconnected (%d), reason %d\n", (int)s_appConns, reason);
    if (s_appTest) {  // a test pattern left on: back to the looks
      leds::testPattern(0);
      s_appTest = false;
    }
  }
};

class CommandCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    if (v.size() < 1) return;
    const uint8_t* d = v.data();
    s_cmds++;
    looks::command(d[0], v.size() > 1 ? d[1] : 0, v.size() > 2 ? d[2] : 0, v.size() > 3 ? d[3] : 0);
  }
};

// The LED layout (the app's settings page, jacket_link.h LAYOUT): config.h's
// LED_CHAIN until the app saves another, kept in flash ("jacket" / "layout").

static void saveLayout(const LedSegment* segs, int n) {
  uint8_t b[1 + LED_MAX_SEGMENTS * jacket::LAYOUT_PART_LEN];
  b[0] = n;
  for (int k = 0; k < n; k++) {
    uint8_t* p = b + 1 + k * jacket::LAYOUT_PART_LEN;
    p[0] = segs[k].type;
    p[1] = (uint8_t)segs[k].side;
    p[2] = segs[k].reversed;
    p[3] = segs[k].count & 255;
    p[4] = segs[k].count >> 8;
  }
  Preferences pr;
  pr.begin("jacket", false);
  if (n) pr.putBytes("layout", b, 1 + n * jacket::LAYOUT_PART_LEN);
  else pr.remove("layout");
  pr.end();
}

static int parseParts(const uint8_t* p, int n, LedSegment* out) {  // 0: not a layout
  if (n < 1 || n > LED_MAX_SEGMENTS) return 0;
  for (int k = 0; k < n; k++, p += jacket::LAYOUT_PART_LEN) {
    if (p[0] > LEDS_DARK) return 0;
    out[k] = LedSegment{(LedPart)p[0], p[3] | (p[4] << 8), p[2] != 0, (int8_t)p[1]};
  }
  return n;
}

static void loadLayout() {
  Preferences pr;
  pr.begin("jacket", true);
  uint8_t b[1 + LED_MAX_SEGMENTS * jacket::LAYOUT_PART_LEN];
  size_t len = pr.getBytesLength("layout");
  if (len && len <= sizeof(b)) pr.getBytes("layout", b, len);
  pr.end();
  LedSegment segs[LED_MAX_SEGMENTS];
  int n = len ? parseParts(b + 1, b[0], segs) : 0;
  if (n && len == 1 + (size_t)n * jacket::LAYOUT_PART_LEN && leds::setChain(segs, n)) {
    s_layoutSaved = true;
    Serial.printf("[layout] saved layout, %d parts\n", n);
  }
}

static std::string layoutBytes() {
  LedSegment segs[LED_MAX_SEGMENTS];
  int n = leds::chain(segs, LED_MAX_SEGMENTS), total = 0;
  for (int k = 0; k < n; k++) total += segs[k].count;
  std::string o = {'L', 1, (char)((s_layoutSaved ? jacket::LAYOUT_SAVED : 0) | (leds::noMatrix() ? jacket::LAYOUT_NO_MATRIX : 0)),
                   (char)n, (char)(total & 255), (char)(total >> 8), (char)(LED_MAX_LEDS & 255), (char)(LED_MAX_LEDS >> 8),
                   (char)LED_DATA_PIN, (char)(LED_MAX_MA & 255), (char)(LED_MAX_MA >> 8)};
  for (int k = 0; k < n; k++)
    o += {(char)segs[k].type, (char)segs[k].side, (char)segs[k].reversed, (char)(segs[k].count & 255), (char)(segs[k].count >> 8)};
  if (WiFi.status() == WL_CONNECTED) o += WiFi.localIP().toString().c_str();
  return o;
}

class LayoutCB : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* c, NimBLEConnInfo&) override { c->setValue(layoutBytes()); }
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    const uint8_t* d = v.data();
    if (v.size() >= 3 && d[0] == 'L' && d[1] == 1) {
      LedSegment segs[LED_MAX_SEGMENTS];
      int n = d[2];
      if (v.size() != 3 + (size_t)n * jacket::LAYOUT_PART_LEN || !parseParts(d + 3, n, segs) || !leds::setChain(segs, n)) {
        Serial.println("[layout] rejected");
        return;
      }
      saveLayout(segs, n);
      s_layoutSaved = true;
      Serial.printf("[layout] new layout from the app, %d parts\n", n);
    } else if (v.size() >= 1 && d[0] == 'D') {
      LedSegment segs[LED_MAX_SEGMENTS];
      leds::setChain(segs, leds::defaultChain(segs, LED_MAX_SEGMENTS));
      saveLayout(segs, 0);
      s_layoutSaved = false;
      Serial.println("[layout] back to config.h's");
    } else if (v.size() >= 6 && d[0] == 'T') {
      leds::testPattern(d[1], d[2] | (d[3] << 8), d[4] | (d[5] << 8));
      s_appTest = d[1] != 0;
    }
  }
};

class LiveCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    const uint8_t* d = v.data();
    if (v.size() >= 6 && d[0] == 'M') looks::music(d[1], d[2], d[3], d[4], d[5] != 0);
    else if (v.size() >= 2 && d[0] == 'H') looks::heartRate(d[1]);
  }
};

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
    if (m.size() >= 13 && (uint8_t)m[0] == 0xFF && (uint8_t)m[1] == 0xFF && !memcmp(m.data() + 2, jacket::TAG_COMMAND, 4)) {
      jacketCommand((const uint8_t*)m.data() + 6);  // the armband
      return;
    }
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
    const uint8_t* addr = dev->getAddress().getVal();
    uint32_t now = millis();
    if (memcmp(addr, s_ownAddr, 6)) {
      if ((int32_t)(now - s_ownSeen) < 3000 && r < s_ownRssi + 8) {
        s_otherSeen = now;
        return;  // another costume, further away
      }
      memcpy(s_ownAddr, addr, 6);
      s_ownRssi = r;
    }
    s_ownRssi += (r - s_ownRssi) * 0.25f;
    s_ownSeen = now;
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
  NimBLEServer* srv = NimBLEDevice::createServer();
  srv->setCallbacks(new ServerCB());
  srv->advertiseOnDisconnect(true);
  NimBLEService* svc = srv->createService(JACKET_SERVICE_UUID);
  svc->createCharacteristic(JACKET_COMMAND_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR)
      ->setCallbacks(new CommandCB());
  svc->createCharacteristic(JACKET_LIVE_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR)
      ->setCallbacks(new LiveCB());
  s_statusChar = svc->createCharacteristic(JACKET_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  svc->createCharacteristic(JACKET_LAYOUT_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE, 512)
      ->setCallbacks(new LayoutCB());
  svc->start();
  // Now, before the scan starts: NimBLE won't register services while it's
  // scanning or advertising (it would wait for the first advert, after the
  // scan -- and the app then finds no services at all).
  srv->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData scanResp;   // the app finds us by the service; the armband by the status advert
  scanResp.setName("ROGUE-LEDS");
  scanResp.addServiceUUID(JACKET_SERVICE_UUID);
  adv->setScanResponseData(scanResp);
  adv->enableScanResponse(true);
  adv->setMinInterval(160);  // 100-150 ms
  adv->setMaxInterval(240);
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new ScanCB(), true);  // every packet, not just new devices
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(80);
  scan->setMaxResults(0);                      // don't keep a list of everything heard
  s_bleState = scan->start(0, false, true) ? "scanning" : "scan start FAILED";
  uint8_t last[jacket::STATUS_LEN] = {};
  uint32_t lastSent = 0, lastScanCheck = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));
    // Our status, advertised (the armband) and notified (the app) when it changes.
    uint8_t st[2 + jacket::STATUS_LEN] = {0xFF, 0xFF};
    memcpy(st + 2, jacket::TAG_STATUS, 4);
    bool costume = s_seen && (int32_t)(millis() - s_seen) < COSTUME_LOST_MS && looks::following();
    looks::status(st + 6, s_ackSeq, s_appConns > 0, costume);
    if (memcmp(last, st + 2, sizeof(last)) || millis() - lastSent > 5000) {
      memcpy(last, st + 2, sizeof(last));
      lastSent = millis();
      NimBLEAdvertisementData ad;
      ad.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
      ad.setManufacturerData(st, sizeof(st));
      adv->setAdvertisementData(ad);
      if (!adv->isAdvertising()) adv->start();
      s_statusChar->setValue(st + 6, 10);
      if (s_appConns > 0) s_statusChar->notify();
    }
    if (millis() - lastScanCheck > 1000) {
      lastScanCheck = millis();
      if (!scan->isScanning()) scan->start(0, false, true);
    }
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
  // Wiring checks (no reflash): /test?mode=index numbers the chain (0 green,
  // every 10th white, 50th red, 100th blue); /test?mode=range&a=120&b=135
  // lights just those (a in green); /test?mode=lit shows exactly what the
  // chain lights (dim white); /test?mode=off goes back to the show.
  s_web.on("/test", [] {
    String m = s_web.arg("mode");
    int a = s_web.arg("a").toInt(), b = s_web.hasArg("b") ? s_web.arg("b").toInt() : a;
    if (m == "index") leds::testPattern(1);
    else if (m == "range") leds::testPattern(2, a, b);
    else if (m == "lit") leds::testPattern(3);
    else leds::testPattern(0);
    char r[120];
    snprintf(r, sizeof(r), "test %s (%d..%d), chain %d LEDs%s\n", m.c_str(), a, b, leds::chainLength(),
             leds::noMatrix() ? ", no matrix" : "");
    s_web.send(200, "text/plain", r);
  });
  // The jacket's remote, over Wi-Fi (testing): /cmd?c=1&a=5 = CMD_LOOK 5 (jacket_link.h).
  s_web.on("/cmd", [] {
    looks::command(s_web.arg("c").toInt(), s_web.arg("a").toInt(), s_web.arg("b").toInt(), s_web.arg("d").toInt());
    s_web.send(200, "text/plain", String("look ") + looks::INFO[looks::look()].name + "\n");
  });
  s_web.on("/live", [] {  // /live?bpm=120 or /live?bass=200&mid=100&treble=50&level=150&beat=1
    if (s_web.hasArg("bpm")) looks::heartRate(s_web.arg("bpm").toInt());
    else looks::music(s_web.arg("bass").toInt(), s_web.arg("mid").toInt(), s_web.arg("treble").toInt(),
                      s_web.arg("level").toInt(), s_web.arg("beat").toInt());
    s_web.send(200, "text/plain", "ok\n");
  });
  s_web.on("/", [] {
    static const char* const MODES[] = {"idle", "alert", "scan", "breakout", "midnight", "noise", "off", "wary",
                                        "contain", "touch", "shutdown", "overload", "stealth", "sentinelone", "safe mode", "kill switch"};
    uint32_t ago = s_seen ? millis() - s_seen : 0;
    char b[900];
    snprintf(b, sizeof(b),
             "ROGUE.AI LEDLINK -- firmware update at /update\ncostume: %s (%.0f dBm)%s\neffect: %s\nmatrix number: %d\nup %lu s\n"
             "bluetooth: %s, %lu packets heard, %lu from the costume (%lu bytes)\nlast restart: %s\n"
             "threat %u, squad %u, talking %s; nearest handheld scanner %d dBm%s\nLED brightness %d%% (limit %d mA), matrix %s\n"
             "look: %s; remote commands %lu; iPhone app %s\n",
             s_seen && ago < COSTUME_LOST_MS ? "seen" : "NOT SEEN", s_ownRssi,
             millis() - s_otherSeen < 5000 ? ", another costume further away ignored" : "", MODES[min<int>(s_mode, 15)], (int)s_number - 1,
             (unsigned long)(millis() / 1000), s_bleState, (unsigned long)s_heard, (unsigned long)s_rai, (unsigned long)s_raiLen, resetReason(), (unsigned)s_threat, (unsigned)s_squad,
             (s_flags & 1) ? "yes" : "no", s_nearestShown, s_infectedShown ? ", an infected phone close" : "",
             leds::brightness(), LED_MAX_MA, leds::noMatrix() ? "NOT wired (No Matrix)" : "wired",
             looks::INFO[looks::look()].name, (unsigned long)s_cmds, s_appConns > 0 ? "connected" : "not connected");
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
  loadLayout();  // the app's layout, if it saved one (else config.h's LED_CHAIN)
  leds::begin();
  leds::setMode(leds::IDLE);
  leds::setNoMatrix(LED_NO_MATRIX);
  looks::begin();
  xTaskCreatePinnedToCore(radioTask, "radio", 6144, nullptr, 2, nullptr, 0);
}

void loop() {
  uint32_t now = millis();
  if (s_webUp) s_web.handleClient();
  bool costume = s_seen && (int32_t)(now - s_seen) < COSTUME_LOST_MS && looks::following();
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
    leds::setNoMatrix(s_flags & 16);   // the costume's operator console says whether the 8x8 is wired in
    if (s_bright) leds::setBrightness(s_bright);  // the costume's LEDS - + (operator console / armband)
  } else {
    leds::setNoMatrix(LED_NO_MATRIX);  // worn without the costume: the 8x8 is off the jacket
    leds::setMode(leds::IDLE);
    leds::matrixNumber(-1);
    leds::setSquad(0);
    leds::setSpeaking(false);
  }
  looks::tick(now);
  leds::tick(now);
  static uint32_t nextLog = 0;
  if ((int32_t)(now - nextLog) >= 0) {
    nextLog = now + 5000;
    Serial.printf("[state] look %s, costume %s, effect %u, number %d, heap %u\n", looks::INFO[looks::look()].name,
                  costume ? "seen" : "lost", (unsigned)s_mode, (int)s_number - 1, (unsigned)ESP.getFreeHeap());
  }
  delay(5);
}
