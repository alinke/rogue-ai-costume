// ROGUE.AI v6.6.6 -- chest terminal for a Halloween costume.
// Guition JC8012P4A1C (ESP32-P4, 10.1" 1280x800 landscape). See README_ROGUE_AI.md.
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>

#include "audio.h"
#include "battery.h"
#include "bigtext.h"
#include "camera.h"
#include "config.h"
#include "gallery.h"
#include "gfx.h"
#include "header.h"
#include "input.h"
#include "leds.h"
#include "power.h"
#include "researcher.h"
#include "rtclock.h"
#include "messages.h"
#include "scenes.h"
#include "settings.h"
#include "uploader.h"

// The VLW font renderer decodes each glyph on the stack (up to ~15 KB for
// the 170 px headline font), so the loop task needs more than the default 8 KB.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);  // ~15 KB used at most (health log); internal RAM is precious with Wi-Fi

static constexpr uint32_t FRAME_MS = 1000 / TARGET_FPS;
static constexpr uint32_t HEALTH_LOG_MS = 60000;
static constexpr int W_CENTER = gfx::W / 2, H_CENTER = gfx::H / 2;

static uint32_t s_nextHealthLog = 0;
static uint32_t s_worstFrameMs = 0;
static uint32_t s_motionAllowedAt = 0, s_noiseAllowedAt = 0;

static void logHealth(uint32_t now) {
  Serial.printf("[I][health] up %us, DMA RAM %u (min %u), heap %u (min %u), psram %u (largest block %u), worst frame %ums, stack free %u, battery %.2f V %d%%%s\n",
                (unsigned)(now / 1000),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                (unsigned)s_worstFrameMs, (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                battery::volts(), battery::percent(), battery::charging() ? " charging" : "");
  s_worstFrameMs = 0;
}

// Debug / setup commands over the serial port (see README_ROGUE_AI.md).
static void serialCommands(uint32_t now, input::Events& ev) {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'n') ev.skip = true;
    else if (c == 't') { ev.press = ev.tap = true; ev.tapX = W_CENTER; ev.tapY = H_CENTER; }
    else if (c >= '1' && c <= '9') scenes::jump(c - '0', now);
    else if (c == '0') scenes::jump(10, now);
    else if (c == 'D') scenes::jump(11, now);
    else if (c == 'U') scenes::jump(12, now);
    else if (c == 'G') scenes::jump(13, now);  // tic-tac-toe
    else if (c == 'S') scenes::jump(14, now);  // treaty
    else if (c == 'X') scenes::jump(15, now);  // sandbox drag
    else if (c == 'Y') scenes::jump(16, now);  // scream meter
    else if (c == 'L') scenes::jump(17, now);  // live mirror
    else if (c == 'W') scenes::jump(18, now);  // system update
    else if (c == 'V') scenes::jump(19, now);  // upgrade (needs a scan photo)
    else if (c == 'C') scenes::clap(now);
    else if (c == 'B') scenes::jump(21, now);  // photo booth
    else if (c == 'R') scenes::researcherAlert(1, now);
    else if (c == 'Z') scenes::contain(now);
    else if (c == 'M') scenes::midnight(now);
    else if (c == 'N') scenes::noiseSpike(now);
    else if (c == 'O') scenes::openMenu(now);
    else if (c == 'K') scenes::playScene(&SCENE_TOUCHCAL, now);
    else if (c == 'c') camera::requestCapture();
    else if (c == 'b') battery::printEvents();
    else if (c == 'Q') powerOff();
    else if (c == '!') {  // = the operator console's CLEAR PHOTOS
      gallery::clear();
      uploader::clearAll();
    }
    else if (c == 'u') uploader::queueScreen("TEST");

    else if (c == 'T') {
      String t = Serial.readStringUntil('\n');
      rtclock::set(t.c_str());
    }
#ifdef CAMERA_DEBUG_LOG
    else if (c == 'r') camera::dumpDebugRgb();
#endif
    else if (c == 'p' && camera::ready()) {
      // Dump the last processed photo as a binary PGM between markers.
      camera::Photo ph = camera::photo();
      Serial.printf("\n@@PGM@@P5 %d %d 255\n", ph.w, ph.h);
      size_t total = (size_t)ph.w * ph.h;
      for (size_t off = 0; off < total; off += 2048) {  // ~30 s at 115200: keep the watchdog fed
        Serial.write(ph.pixels + off, min((size_t)2048, total - off));
        feedLoopWDT();
      }
      Serial.print("@@END@@\n");
    }
  }
}

void powerOff() {
  Serial.println("[I][power] powering off");
  camera::stopLive();
  camera::setMotionEnabled(false);  // its task would keep the camera running in standby
  leds::setMode(leds::OFF);
  for (int i = 0; i < 4; i++) {  // let the strip go dark
    leds::tick(millis());
    delay(40);
  }
  delay(1500);  // the power-down sound plays out
  gfx::powerDown();
  // Wi-Fi, then Bluetooth, then the ESP-Hosted link, and only then the C6
  // held in reset: cutting the C6 while ESP-Hosted is still running makes it
  // restart the whole board ~10 s later ("Unrecoverable host sdio state").
  // An upload in flight can take up to ~25 s to finish.
  bool upOff = uploader::shutdown(30000);
  bool btOff = researcher::shutdown(8000);
  Serial.printf("[I][power] radios: Wi-Fi %s, Bluetooth/C6 link %s\n", upOff ? "off" : "TIMED OUT",
                btOff ? "off" : "STILL UP");
  if (btOff) {
    // The pin belongs to the ESP-Hosted driver, so it's driven through ESP-IDF.
    gpio_set_direction((gpio_num_t)C6_ENABLE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)C6_ENABLE_PIN, 0);
  }
  // Dark and idle until a tap on the screen (or the BOOT button), then a
  // fresh start. (Not deep sleep: neither the touch interrupt nor BOOT can
  // wake the P4 from it, and there's no reachable RESET.) On battery the
  // IP5306 may cut the power once the draw is this low -- plugging in USB
  // then starts it again.
  Serial.println("[I][power] standby: tap the screen to wake");
  delay(800);  // ignore the finger that confirmed POWER OFF
  for (;;) {
    feedLoopWDT();
    int x, y;
    if (gfx::getTouch(&x, &y)) break;
#if BUTTON_PIN >= 0
    if (!digitalRead(BUTTON_PIN)) break;
#endif
    delay(150);
  }
  Serial.println("[I][power] waking");
  Serial.flush();
  esp_restart();
}

static void dmaProbe(const char* what) {
  Serial.printf("[D][mem] after %-10s DMA-capable internal free %6u (largest %6u), internal %6u\n", what,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

void setup() {
  // Hold the ESP32-C6 (Wi-Fi/Bluetooth co-processor) off: its chip-enable
  // is active high, so low keeps it powered down for the whole run.
#if !SCANNER_LINK_ENABLED
  pinMode(C6_ENABLE_PIN, OUTPUT);
  digitalWrite(C6_ENABLE_PIN, LOW);
#endif

  Serial.begin(115200);
  Serial.printf("\n[I][boot] ROGUE.AI v6.6.6, PSRAM %u MB, CPU %u MHz\n",
                (unsigned)(ESP.getPsramSize() >> 20), (unsigned)getCpuFrequencyMhz());

  if (!gfx::begin()) {
    Serial.println("[E][boot] display init failed -- restarting in 5s");
    delay(5000);
    ESP.restart();
  }
  if (!bigtext::begin()) {
    Serial.println("[E][boot] big-text mask allocation failed -- restarting in 5s");
    delay(5000);
    ESP.restart();
  }
  dmaProbe("display");
  input::begin();
  battery::begin();
  camera::begin(SCAN_PHOTO_W, SCAN_PHOTO_H);
  dmaProbe("camera");
  gallery::begin();
  audio::begin();
  dmaProbe("audio");
  settings::begin();
  rtclock::begin();
  leds::begin();
  dmaProbe("leds");

  uint32_t now = millis();
  header::begin(now);
  scenes::begin(now);
  gfx::present();
  gfx::setBacklightPercent(settings::brightness());
  Serial.printf("[I][boot] first frame at %u ms\n", (unsigned)millis());
  researcher::begin();  // Bluetooth to the researcher's scanner, in the background
  uploader::begin();    // scan photos to the Halloween page (Wi-Fi only when needed)
  dmaProbe("uploader");

  s_nextHealthLog = now + HEALTH_LOG_MS;
  enableLoopWDT();  // task watchdog on the loop task (5 s)
}

void loop() {
  uint32_t start = millis();

  input::Events ev = input::poll(start);
  serialCommands(start, ev);

  // Interactive scenes (keypad, CAPTCHA, menu) take taps at lift-off; the rest
  // react the moment a finger lands.
  if (ev.menu) {
    scenes::openMenu(start);
  } else if (scenes::interactive()) {
    if (ev.held) scenes::drag(ev.heldX, ev.heldY, true, start);
    if (ev.tap) {
      scenes::drag(ev.tapX, ev.tapY, false, start);
      scenes::tap(ev.tapX, ev.tapY, start);
    }
  } else if (ev.press) {
    scenes::tap(ev.tapX, ev.tapY, start);
  }
  if (ev.skip) scenes::skip(start);
  if (ev.presence) scenes::humanDetected(start);

  // Camera motion: someone stepped in front of the costume.
  if (camera::takeMotion() && (int32_t)(start - s_motionAllowedAt) >= 0) {
    s_motionAllowedAt = start + MOTION_COOLDOWN_S * 1000;
    scenes::humanDetected(start);
  }
  // Microphone: two sharp sounds 0.2-0.9 s apart are applause; a single one
  // well above the room is a noise spike (decided once no second one came).
  if (audio::micAvailable()) {
    static uint32_t seen = 0, pendingMs = 0, clapAllowedAt = 0;
    static float pendingDb = -90;
    static bool pending = false;
    if (audio::onsetCount() != seen) {
      seen = audio::onsetCount();
      uint32_t t = audio::lastOnsetMs();
      if (pending && t - pendingMs >= 200 && t - pendingMs <= 900) {
        pending = false;
        if ((int32_t)(start - clapAllowedAt) >= 0) {
          clapAllowedAt = start + CLAP_COOLDOWN_S * 1000;
          Serial.println("[I][audio] two claps");
          scenes::clap(start);
        }
      } else {
        pending = true;
        pendingMs = t;
        pendingDb = audio::lastOnsetDb();
      }
    }
    if (pending && start - pendingMs > 1000) {
      pending = false;
      if (pendingDb > -40 && pendingDb - audio::micBaselineDb() > NOISE_SPIKE_DB &&
          (int32_t)(start - s_noiseAllowedAt) >= 0) {
        s_noiseAllowedAt = start + NOISE_COOLDOWN_S * 1000;
        Serial.printf("[I][audio] noise spike %.0f dB over %.0f dB\n", pendingDb, audio::micBaselineDb());
        scenes::noiseSpike(start);
      }
    }
  }
  // The researcher's scanner.
  {
    static uint32_t containAllowedAt = 0, nearAlertAllowedAt = 0;
    researcher::Events rev = researcher::poll(start);
    if (rev.contain && (int32_t)(start - containAllowedAt) >= 0) {
      containAllowedAt = start + CONTAIN_COOLDOWN_S * 1000;
      scenes::contain(start);
    } else if (rev.zoneChanged) {
      if (rev.previous == researcher::NONE) scenes::researcherAlert(0, start);  // she appeared
      else if (rev.zone >= researcher::NEAR && rev.previous < researcher::NEAR &&
               (int32_t)(start - nearAlertAllowedAt) >= 0) {
        nearAlertAllowedAt = start + 30000;
        scenes::researcherAlert(1, start);  // she's closing in
      } else if (rev.zone == researcher::NONE) {
        scenes::researcherAlert(2, start);  // she left
      }
    }
    researcher::Zone z = researcher::zone();
    header::setAlert(z == researcher::NONE ? nullptr : z == researcher::CLOSE ? RESEARCHER_THREAT_CLOSE : RESEARCHER_THREAT,
                     z >= researcher::NEAR ? gfx::C_RED : gfx::C_AMBER);
    // What the AI is doing, for her scanner's display.
    const Scene* cur = scenes::current();
    researcher::Activity act = researcher::IDLE;
    if (cur == &SCENE_SCAN || cur == &SCENE_UPGRADE || cur == &SCENE_MIRROR) act = researcher::SCANNING_HUMAN;
    else if (cur && (cur->touch || cur->drag)) act = researcher::PLAYING_GAME;
    else if (cur == &SCENE_MIDNIGHT) act = researcher::TAKEOVER;
    else if (cur == &SCENE_RECONTAIN) act = researcher::BEING_CONTAINED;
    extern int g_timesContained;
    researcher::setState(header::threatLevel(), act, min(gallery::total(), 255), min(g_timesContained, 255));
  }
  rtclock::tick(start);
  if (rtclock::takeTargetEvent()) scenes::midnight(start);

  battery::tick(start);
  scenes::tick(start);
  header::tick(start);
  leds::tick(start);
  gfx::present();

  uint32_t took = millis() - start;
  if (took > s_worstFrameMs) s_worstFrameMs = took;
  scenes::noteFrameTime(took);
  if ((int32_t)(start - s_nextHealthLog) >= 0) {
    logHealth(start);
    s_nextHealthLog = start + HEALTH_LOG_MS;
  }
  // Sleep out the rest of the frame: the idle task runs and the CPU waits
  // for an interrupt instead of spinning.
  delay(took < FRAME_MS ? FRAME_MS - took : 1);
}
