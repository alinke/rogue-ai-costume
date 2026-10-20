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

#include "camera.h"
#include "gfx.h"
#include "researcher.h"

#if !SCANNER_LINK_ENABLED
#error "PHOTO_UPLOAD_ENABLED needs SCANNER_LINK_ENABLED (it brings up the C6)"
#endif

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
static constexpr int OUT_W = gfx::W * 3 / 4, OUT_H = gfx::H * 3 / 4;
static ppa_client_handle_t s_ppa;
static uint8_t* s_rgb = nullptr;  // RGB888 copy of the screen (encoder input)
static size_t s_rgbCap = 0;
static uint8_t* s_jpg = nullptr;  // encoder output
static size_t s_jpgCap = 0;

// One photo handed from loop() to the task at a time.
static uint8_t* s_hand = nullptr;
static size_t s_handLen = 0;
static char s_handLabel[40];
static size_t s_handFace = 0;  // trailing bytes of s_hand: the plain photo, for the face check
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

// jpg = the dossier, followed by faceLen bytes of plain photo (face check only).
static void store(const uint8_t* jpg, size_t len, size_t faceLen, const char* label) {
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
  if (f) { f.printf("%s\n%s\n%u", label, id, (unsigned)faceLen); f.close(); }
  f = s_fs->open(String(base) + ".jpg", "w");
  size_t w = f ? f.write(jpg, len) : 0;
  if (f) f.close();
  if (w != len) {
    Serial.printf("[E][upload] couldn't store photo (%u of %u bytes)\n", (unsigned)w, (unsigned)len);
    s_fs->remove(String(base) + ".jpg");
  }
  if (s_sd) {  // the keepsake copy, never deleted
    f = s_fs->open(String(ARCHIVE) + "/" + id + ".jpg", "w");
    if (f) { f.write(jpg, len - faceLen); f.close(); }
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

static volatile bool s_clearQueue = false, s_clearServer = false;

// POST /clear with the admin key: every photo off the web page.
static bool postClear() {
#ifdef PHOTO_ADMIN_KEY
  WiFiClientSecure tls;
  tls.setInsecure();
  String url = PHOTO_SERVICE_URL;
  int hs = url.indexOf("//") + 2;
  String host = url.substring(hs);
  int slash = host.indexOf('/');
  if (slash >= 0) host = host.substring(0, slash);
  int code = -1;
  if (tls.connect(host.c_str(), 443)) {
    String req = "POST /clear HTTP/1.1\r\nHost: " + host + "\r\nX-Key: " PHOTO_ADMIN_KEY
                 "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    if (tls.print(req) == req.length()) {
      uint32_t w0 = millis();
      while (!tls.available() && tls.connected() && millis() - w0 < 25000) delay(20);
      tls.setTimeout(5);
      String status = tls.readStringUntil('\n');
      int sp = status.indexOf(' ');
      if (sp > 0) code = status.substring(sp + 1).toInt();
    }
    tls.stop();
  }
  Serial.printf("[%s][upload] clear the web page: HTTP %d\n", code == 200 ? "I" : "W", code);
  return code == 200 || code == 403;  // 403: wrong key -- retrying won't help
#else
  return true;
#endif
}

static void dropQueue() {
  String oldest;
  int n;
  scanQueue(&oldest, &n);
  while (n > 0) {
    s_fs->remove(path(oldest));
    oldest.replace(".jpg", ".txt");
    s_fs->remove(path(oldest));
    scanQueue(&oldest, &n);
  }
  Serial.println("[I][upload] upload queue cleared");
}

void clearAll() {
  s_clearQueue = true;
#ifdef PHOTO_ADMIN_KEY
  s_clearServer = true;
#endif
}
bool clearPending() { return s_clearServer; }

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
  size_t faceLen = 0;
  String txt = name;
  txt.replace(".jpg", ".txt");
  File t = s_fs->open(path(txt));
  if (t) {
    label = t.readStringUntil('\n');
    id = t.readStringUntil('\n');
    faceLen = t.readString().toInt();
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
    researcher::pauseScan(true);
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
      if (faceLen) req += "X-Face-Bytes: " + String(faceLen) + "\r\n";
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
    researcher::pauseScan(false);
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
    size_t len = 0, face = 0;
    char label[40];
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_hand) {
      hand = s_hand; len = s_handLen; memcpy(label, s_handLabel, sizeof(label));
      face = s_handFace;
      s_hand = nullptr;
    }
    xSemaphoreGive(s_lock);
    if (hand) {
      store(hand, len, face, label);
      heap_caps_free(hand);
    }

    if (s_clearQueue) {
      s_clearQueue = false;
      dropQueue();
    }
    if (s_clearServer && researcher::radioUp() && (int32_t)(millis() - retryAt) >= 0) {
      if (join() && postClear()) {
        s_clearServer = false;
        idleSince = millis();
        wifiOn = true;
      } else {
        retryAt = millis() + RETRY_MS;
      }
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
    } else if (researcher::radioUp() && (int32_t)(now - retryAt) >= 0) {
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

// The plain camera photo (grayscale) as a small JPEG, appended to the dossier
// for the server's face check: the red scan-line dossier is much harder for
// face detection. Returns its length (0 = none). Rows cut to a multiple of 8.
static size_t encodeFace(const camera::Photo* ph, uint8_t* out, size_t cap) {
  if (!ph || !ph->pixels || ph->w % 8) return 0;
  int h = ph->h & ~7;
  size_t n = (size_t)ph->w * h;
  if (n > s_rgbCap) return 0;
  memcpy(s_rgb, ph->pixels, n);  // encoder input must be its own aligned buffer
  jpeg_encode_cfg_t cfg = {};
  cfg.width = ph->w;
  cfg.height = h;
  cfg.src_type = JPEG_ENCODE_IN_FORMAT_GRAY;
  cfg.sub_sample = JPEG_DOWN_SAMPLING_GRAY;
  cfg.image_quality = 75;
  uint32_t len = 0;
  esp_err_t e = jpeg_encoder_process(s_enc, &cfg, s_rgb, n, out, cap, &len);
  if (e != ESP_OK) Serial.printf("[W][upload] face-check photo not encoded: %s\n", esp_err_to_name(e));
  return e == ESP_OK ? len : 0;
}

void queueScreen(const char* label, const camera::Photo* face) {
  if (!s_ok) return;
  uint32_t t0 = millis();
  // Canvas pixels are LovyanGFX's byte-swapped RGB565.
  ppa_srm_oper_config_t op = {};
  op.in.buffer = gfx::canvas.getBuffer();
  op.in.pic_w = gfx::W;
  op.in.pic_h = gfx::H;
  op.in.block_w = gfx::W;
  op.in.block_h = gfx::H;
  op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.out.buffer = s_rgb;
  op.out.buffer_size = s_rgbCap;
  op.out.pic_w = OUT_W;
  op.out.pic_h = OUT_H;
  op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB888;
  op.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  op.scale_x = op.scale_y = 0.75f;
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
  // The encoder's output must start aligned: encode the plain photo at the
  // start of the buffer (the dossier is safe in `copy`), then append it.
  size_t faceLen = encodeFace(face, s_jpg, s_jpgCap);
  if (faceLen) {
    uint8_t* both = (uint8_t*)heap_caps_realloc(copy, len + faceLen, MALLOC_CAP_SPIRAM);
    if (both) {
      copy = both;
      memcpy(copy + len, s_jpg, faceLen);
    } else {
      faceLen = 0;
    }
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  if (s_hand) heap_caps_free(s_hand);  // the task hasn't stored the last one yet: replace it
  s_hand = copy;
  s_handLen = len + faceLen;
  s_handFace = faceLen;
  snprintf(s_handLabel, sizeof(s_handLabel), "%s", label);
  xSemaphoreGive(s_lock);
  Serial.printf("[I][upload] screen captured: %u KB JPEG (+%u KB photo for the face check) in %u ms\n",
                (unsigned)(len / 1024), (unsigned)(faceLen / 1024), (unsigned)(millis() - t0));
}

int pending() { return s_pending; }
int sent() { return s_sent; }

}  // namespace uploader

#else

namespace uploader {
void begin() {}
void queueScreen(const char*, const camera::Photo*) {}
void clearAll() {}
bool clearPending() { return false; }
int pending() { return 0; }
int sent() { return 0; }
}  // namespace uploader

#endif
