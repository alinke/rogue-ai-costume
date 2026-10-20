#include "uploader.h"

#include "config.h"

#if PHOTO_UPLOAD_ENABLED

#include <LittleFS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/jpeg_encode.h>
#include <driver/ppa.h>
#include <mbedtls/platform.h>

#include "radio.h"


namespace uploader {

// Queue on the SD card when there is one (fast, and no display freeze --
// writing the board's own flash stalls everything for ~1 s), with a keepsake
// copy of every photo in /rogue-ai/photos. Without a card: the flash
// (LittleFS), queue only. Never both: Wi-Fi start-up then ran out of VFS slots
// for its sockets and aborted.
static fs::FS* s_fs = nullptr;
static bool s_sd = false;
static String DIR = "/q";
static constexpr const char* ARCHIVE = "/rogue-ai/photos";
static constexpr uint32_t WIFI_JOIN_MS = 20000;
static constexpr uint32_t RETRY_MS = 60000;     // after a failed join / upload
static constexpr uint32_t IDLE_OFF_MS = 30000;  // queue empty this long: Wi-Fi off

struct Net { const char* ssid; const char* pass; };
static const Net NETS[] = WIFI_NETWORKS;

static bool s_ok = false;
static jpeg_encoder_handle_t s_enc;
// The screen is scaled to 960x600 for the page (~45% fewer bytes over the
// air than 1280x800), by the PPA, which also converts it to the encoder's
// RGB888 (byte order B, G, R -- the same for both).
static constexpr int SRC_W = 720, SRC_H = 1280;  // the Tab5 screen (portrait)
static constexpr int OUT_W = SRC_W, OUT_H = SRC_H;  // full size: 3/4 would be 540 wide, not a multiple of 16
static ppa_client_handle_t s_ppa;
static uint8_t* s_rgb = nullptr;  // RGB888 copy of the screen (encoder input)
static size_t s_rgbCap = 0;
static uint8_t* s_jpg = nullptr;  // encoder output
static size_t s_jpgCap = 0;

// One photo handed from loop() to the task at a time.
static uint8_t* s_hand = nullptr;
static size_t s_handLen = 0;
static char s_handLabel[40];
static SemaphoreHandle_t s_lock;
static volatile int s_pending = 0, s_sent = 0;

// ---- Queue in flash ------------------------------------------------------------
// /q/<seq>.jpg plus /q/<seq>.txt (label). seq keeps the order across reboots.
static uint32_t s_seq = 0;

static void scanQueue(String* oldest, int* count) {
  File d = s_fs->open(DIR);
  *count = 0;
  uint32_t lo = UINT32_MAX;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    String n = f.name();
    if (!n.endsWith(".jpg")) continue;
    uint32_t seq = strtoul(n.c_str(), nullptr, 10);
    (*count)++;
    if (seq < lo) { lo = seq; *oldest = n; }
    if (seq >= s_seq) s_seq = seq + 1;
  }
}

static String path(const String& name) { return String(DIR) + "/" + name; }

static void store(const uint8_t* jpg, size_t len, const char* label) {
  String oldest;
  int n;
  scanQueue(&oldest, &n);
  while (n >= PHOTO_QUEUE_MAX) {  // full: drop the oldest
    s_fs->remove(path(oldest));
    oldest.replace(".jpg", ".txt");
    s_fs->remove(path(oldest));
    scanQueue(&oldest, &n);
  }
  char base[64];
  snprintf(base, sizeof(base), "%s/%08u", DIR.c_str(), (unsigned)s_seq++);
  // Sidecar: label, then this photo's id (a retry reuses it, so the server
  // never stores a second copy).
  char id[24];
  snprintf(id, sizeof(id), "%06llx-%08lx", (unsigned long long)(ESP.getEfuseMac() & 0xFFFFFF),
           (unsigned long)esp_random());
  File f = s_fs->open(String(base) + ".txt", "w");
  if (f) { f.printf("%s\n%s", label, id); f.close(); }
  f = s_fs->open(String(base) + ".jpg", "w");
  size_t w = f ? f.write(jpg, len) : 0;
  if (f) f.close();
  if (w != len) {
    Serial.printf("[E][upload] couldn't store photo (%u of %u bytes)\n", (unsigned)w, (unsigned)len);
    s_fs->remove(String(base) + ".jpg");
  }
  if (s_sd) {  // the keepsake copy, never deleted
    f = s_fs->open(String(ARCHIVE) + "/" + id + ".jpg", "w");
    if (f) { f.write(jpg, len); f.close(); }
  }
}

// ---- Wi-Fi --------------------------------------------------------------------
static bool join() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);  // modem sleep between packets: Bluetooth shares the radio
    WiFi.setAutoReconnect(false);
  }
  for (const Net& n : NETS) {
    Serial.printf("[I][upload] Wi-Fi: joining %s\n", n.ssid);
    WiFi.begin(n.ssid, n.pass);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_JOIN_MS) delay(250);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[I][upload] Wi-Fi: on %s, %s, %d dBm\n", n.ssid, WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());
      return true;
    }
    WiFi.disconnect();
  }
  Serial.println("[W][upload] Wi-Fi: no network reachable");
  return false;
}

// Disconnect, but keep the Wi-Fi driver initialized: a full WIFI_OFF and
// restart through ESP-Hosted lost 4-10 KB of internal RAM every cycle. A
// station that isn't connected doesn't transmit.
static void wifiOff() {
  WiFi.disconnect(false, false);
  Serial.printf("[I][upload] Wi-Fi disconnected (internal RAM free %u)\n",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

static uint32_t s_dmaMin;
static void noteDma() {
  s_dmaMin = min<uint32_t>(s_dmaMin, heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
}

static bool send(const String& name) {
  File f = s_fs->open(path(name));
  if (!f) return false;
  size_t len = f.size();
  uint8_t* buf = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
  bool ok = buf && f.read(buf, len) == len;
  f.close();
  String label, id;
  String txt = name;
  txt.replace(".jpg", ".txt");
  File t = s_fs->open(path(txt));
  if (t) {
    label = t.readStringUntil('\n');
    id = t.readString();
    t.close();
  }
  if (ok) {
    // Hand-written request, the body in small writes: each write becomes one
    // TLS record, which the AES hardware encrypts through an internal-RAM
    // bounce buffer of the same size -- a whole 16 KB record didn't fit
    // ("-1" from mbedtls_ssl_write).
    WiFiClientSecure tls;
    tls.setInsecure();  // the key isn't worth pinning certificates for (party photos)
    tls.setTimeout(20);
    String url = PHOTO_UPLOAD_URL;  // https://host/path
    int hs = url.indexOf("//") + 2, ps = url.indexOf('/', hs);
    String host = url.substring(hs, ps), pathq = url.substring(ps);
    Serial.printf("[I][upload] sending %s id=%s\n", name.c_str(), id.c_str());
    s_dmaMin = UINT32_MAX;
    radio::pauseScan(true);
    vTaskDelay(pdMS_TO_TICKS(600));  // the scan stops on the BLE task's next pass
    uint32_t t0 = millis();
    int code = -1;
    const char* stage = "connect";
    size_t sentBytes = 0;
    if (tls.connect(host.c_str(), 443)) {
      stage = "send";
      String req = "POST " + pathq + " HTTP/1.1\r\nHost: " + host +
                   "\r\nContent-Type: image/jpeg\r\nX-Key: " PHOTO_UPLOAD_KEY "\r\n";
      if (label.length()) req += "X-Label: " + label + "\r\n";
      if (id.length()) req += "X-Id: " + id + "\r\n";
      req += "Content-Length: " + String(len) + "\r\nConnection: close\r\n\r\n";
      bool wrote = tls.print(req) == req.length();
      for (size_t off = 0; wrote && off < len; off += 2048) {
        size_t n = min((size_t)2048, len - off);
        wrote = tls.write(buf + off, n) == n;
        if (wrote) sentBytes = off + n;
        noteDma();
        vTaskDelay(pdMS_TO_TICKS(8));  // pace it: lets ESP-Hosted's buffers drain
      }
      if (wrote) {
        // The server answers after its face check (~1-2 s): wait for it.
        // (Reading straight away timed out in milliseconds, and closing the
        // connection then cut off uploads still in flight on a weak signal.)
        stage = "response";
        uint32_t w0 = millis();
        while (!tls.available() && tls.connected() && millis() - w0 < 25000) delay(20);
        tls.setTimeout(5);
        String status = tls.readStringUntil('\n');  // "HTTP/1.1 200 OK"
        int sp = status.indexOf(' ');
        if (sp > 0) code = status.substring(sp + 1).toInt();
      }
      tls.stop();
    }
    radio::pauseScan(false);
    ok = code == 200;
    // No reply = not delivered: it stays queued and goes again (same id, so
    // the server never keeps two copies).
    char err[80] = "";
    if (!ok) {
      tls.lastError(err, sizeof(err));
      Serial.printf("[W][upload] failed at %s, %u/%u bytes sent, DMA RAM free %u (largest %u): %s\n", stage,
                    (unsigned)sentBytes, (unsigned)len,
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA), err);
    }
    Serial.printf("[%s][upload] %s (%u KB, \"%s\"): HTTP %d in %u ms, DMA RAM low %u\n", ok ? "I" : "W",
                  name.c_str(), (unsigned)(len / 1024), label.c_str(), code, (unsigned)(millis() - t0),
                  (unsigned)s_dmaMin);
    // 4xx: the server will never take it (bad key, not a JPEG): drop it.
    if (code >= 400 && code < 500) ok = true;
  }
  heap_caps_free(buf);
  if (ok) {
    s_fs->remove(path(name));
    s_fs->remove(path(txt));
  }
  return ok;
}

static void task(void*) {
  uint32_t retryAt = 0, idleSince = 0;
  bool wifiOn = false;
  for (;;) {
    // A new photo from loop(): into the flash queue.
    uint8_t* hand = nullptr;
    size_t len = 0;
    char label[40];
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_hand) {
      hand = s_hand; len = s_handLen; memcpy(label, s_handLabel, sizeof(label));
      s_hand = nullptr;
    }
    xSemaphoreGive(s_lock);
    if (hand) {
      store(hand, len, label);
      heap_caps_free(hand);
    }

    String oldest;
    int n;
    scanQueue(&oldest, &n);
    s_pending = n;
    uint32_t now = millis();
    if (n == 0) {
      if (wifiOn && now - idleSince > IDLE_OFF_MS) {
        wifiOff();
        wifiOn = false;
      }
    } else if (radio::up() && (int32_t)(now - retryAt) >= 0) {
      wifiOn = true;
      if (!join() || !send(oldest)) {
        retryAt = millis() + RETRY_MS;
        wifiOff();
        wifiOn = false;
      } else {
        s_sent = s_sent + 1;
        idleSince = millis();
        continue;  // straight on to the next one
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// TLS memory. This core's mbedTLS allocates everything from internal RAM, and
// with the display, camera, audio and Bluetooth running there isn't enough
// left: the handshake failed with a bare "-1". So it all goes to PSRAM.
static void* tlsCalloc(size_t n, size_t size) {
  void* p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM);
  return p ? p : heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void begin() {
  mbedtls_platform_set_calloc_free(tlsCalloc, heap_caps_free);
  // SD card: slot 0's I/O is powered by the P4's on-chip LDO channel 4.
  SD_MMC.setPowerChannel(4);
  if (SD_MMC.begin("/sdcard", false, false, SDMMC_FREQ_DEFAULT, 4)) {
    s_fs = &SD_MMC;
    s_sd = true;
    DIR = "/rogue-ai/queue";
    SD_MMC.mkdir("/rogue-ai");
    SD_MMC.mkdir(DIR);
    SD_MMC.mkdir(ARCHIVE);
    Serial.printf("[I][upload] SD card: %llu MB, %llu MB free\n", SD_MMC.cardSize() >> 20,
                  (SD_MMC.totalBytes() - SD_MMC.usedBytes()) >> 20);
  } else if (LittleFS.begin(true, "/littlefs", 4, "spiffs")) {
    s_fs = &LittleFS;
    if (!LittleFS.exists(DIR)) LittleFS.mkdir(DIR);
    Serial.println("[I][upload] no SD card: queueing in flash");
  } else {
    Serial.println("[E][upload] no SD card and no flash queue (LittleFS)");
    return;
  }

  jpeg_encode_engine_cfg_t ecfg = {};
  ecfg.timeout_ms = 200;
  if (jpeg_new_encoder_engine(&ecfg, &s_enc) != ESP_OK) {
    Serial.println("[E][upload] JPEG encoder unavailable");
    return;
  }
  jpeg_encode_memory_alloc_cfg_t in = {.buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER};
  jpeg_encode_memory_alloc_cfg_t out = {.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER};
  // PPA output and JPEG input: cache-line aligned (128 covers both).
  s_rgbCap = (OUT_W * OUT_H * 3 + 127) & ~127;
  s_rgb = (uint8_t*)heap_caps_aligned_calloc(128, 1, s_rgbCap, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
  (void)in;
  ppa_client_config_t pc = {};
  pc.oper_type = PPA_OPERATION_SRM;
  pc.max_pending_trans_num = 1;
  if (ppa_register_client(&pc, &s_ppa) != ESP_OK) {
    Serial.println("[E][upload] PPA unavailable");
    return;
  }
  s_jpg = (uint8_t*)jpeg_alloc_encoder_mem(1024 * 1024, &out, &s_jpgCap);
  if (!s_rgb || !s_jpg) {
    Serial.println("[E][upload] JPEG buffers: out of memory");
    return;
  }
  s_lock = xSemaphoreCreateMutex();
  String oldest;
  int n;
  scanQueue(&oldest, &n);
  s_pending = n;
  xTaskCreatePinnedToCore(task, "uploader", 8192, nullptr, 1, nullptr, 0);
  s_ok = true;
  Serial.printf("[I][upload] ready, %d photo(s) waiting\n", n);
}

void queueImage(const uint16_t* swapped565, const char* label) {
  if (!s_ok) return;
  uint32_t t0 = millis();
  // Canvas pixels are LovyanGFX's byte-swapped RGB565.
  ppa_srm_oper_config_t op = {};
  op.in.buffer = swapped565;
  op.in.pic_w = SRC_W;
  op.in.pic_h = SRC_H;
  op.in.block_w = SRC_W;
  op.in.block_h = SRC_H;
  op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.out.buffer = s_rgb;
  op.out.buffer_size = s_rgbCap;
  op.out.pic_w = OUT_W;
  op.out.pic_h = OUT_H;
  op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB888;
  op.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  op.scale_x = op.scale_y = (float)OUT_W / SRC_W;
  op.byte_swap = true;
  op.mode = PPA_TRANS_MODE_BLOCKING;
  size_t heapBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  esp_err_t pe = ppa_do_scale_rotate_mirror(s_ppa, &op);
  if (pe != ESP_OK) {
    Serial.printf("[E][upload] PPA scale failed: %s (internal RAM %u -> %u)\n", esp_err_to_name(pe),
                  (unsigned)heapBefore, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    return;
  }
  const int count = OUT_W * OUT_H;
  jpeg_encode_cfg_t cfg = {};
  cfg.width = OUT_W;
  cfg.height = OUT_H;
  cfg.src_type = JPEG_ENCODE_IN_FORMAT_RGB888;
  cfg.sub_sample = JPEG_DOWN_SAMPLING_YUV420;
  cfg.image_quality = PHOTO_JPEG_QUALITY;
  uint32_t len = 0;
  if (jpeg_encoder_process(s_enc, &cfg, s_rgb, count * 3, s_jpg, s_jpgCap, &len) != ESP_OK || !len) {
    Serial.println("[E][upload] JPEG encode failed");
    return;
  }
  uint8_t* copy = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
  if (!copy) return;
  memcpy(copy, s_jpg, len);
  xSemaphoreTake(s_lock, portMAX_DELAY);
  if (s_hand) heap_caps_free(s_hand);  // the task hasn't stored the last one yet: replace it
  s_hand = copy;
  s_handLen = len;
  snprintf(s_handLabel, sizeof(s_handLabel), "%s", label);
  xSemaphoreGive(s_lock);
  Serial.printf("[I][upload] screen captured: %u KB JPEG in %u ms\n", (unsigned)(len / 1024),
                (unsigned)(millis() - t0));
}

int pending() { return s_pending; }
int sent() { return s_sent; }

}  // namespace uploader

#else

namespace uploader {
void begin() {}
void queueImage(const uint16_t*, const char*) {}
int pending() { return 0; }
int sent() { return 0; }
}  // namespace uploader

#endif
