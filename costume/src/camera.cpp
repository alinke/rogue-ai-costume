#include "camera.h"

#include <esp_heap_caps.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "config.h"
#include "esp32-hal-i2c.h"
extern "C" {
#include "esp_sccb_i2c.h"  // no C++ guard of its own
}
#include "esp_sccb_intf.h"
#ifdef CAMERA_DEBUG_LOG
#include "soc/mipi_csi_host_struct.h"
#endif
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "linux/videodev2.h"
#include "esp_cam_sensor.h"
#include "ov02c10.h"  // Guition's driver (lib/ov02c10)

// Exported by the core's prebuilt esp_video 2.3.0 but declared only in its
// private header (esp_video_device_internal.h); declared here to match.
extern "C" {
typedef struct esp_video_csi_device_config {
  bool dont_init_ldo;
} esp_video_csi_device_config_t;
esp_err_t esp_video_create_csi_video_device(esp_cam_sensor_device_t* cam_dev,
                                            const esp_video_csi_device_config_t* config);
}

namespace camera {

static constexpr int BUFFERS = 2;
// PHOTO LIGHT (settings): exposure target, and a gamma curve that lifts the
// midtones after the contrast stretch (faces in a dim room).
static int s_aeTarget = 85 + 15 * PHOTO_LIGHT_DEFAULT;
static float s_gamma = 1.0f - 0.08f * PHOTO_LIGHT_DEFAULT;

static bool s_ok = false;
static int s_fd = -1;
static uint32_t s_camW = 0, s_camH = 0, s_fmt = 0;
static uint8_t* s_buf[BUFFERS];
static size_t s_bufLen[BUFFERS];

static uint8_t* s_photo = nullptr;  // processed grayscale, s_w x s_h
static int s_w = 0, s_h = 0;
static TaskHandle_t s_task = nullptr;
static volatile bool s_ready = false;
static esp_sccb_io_handle_t s_sccb = nullptr;
static volatile bool s_motionEnabled = true;
static volatile bool s_captureRequested = false;

// Live view (the mirror scene): double-buffered grayscale frames.
static uint8_t* s_live[2] = {nullptr, nullptr};
static volatile int s_liveFront = 0;
static volatile uint32_t s_liveFrames = 0;
static volatile bool s_liveRun = false;
static volatile uint32_t s_livePolled = 0;  // last liveFrame() call: the view stops when nobody's watching

static bool xioctl(unsigned long req, void* arg, const char* what) {
  if (ioctl(s_fd, req, arg) == 0) return true;
  Serial.printf("[E][camera] %s failed\n", what);
  return false;
}

// Luma (0-255) of pixel (x, y) in the camera frame.
static inline uint8_t lumaAt(const uint8_t* f, uint32_t x, uint32_t y) {
  if (s_fmt == V4L2_PIX_FMT_GREY) return f[y * s_camW + x];
  uint16_t p = ((const uint16_t*)f)[y * s_camW + x];  // RGB565, little-endian
  uint32_t r = (p >> 11) << 3, g = ((p >> 5) & 63) << 2, b = (p & 31) << 3;
  return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
}

// Centre-crops the frame to the photo's aspect ratio, scales it down, and
// stretches the contrast (2%..98% of the histogram -> full range), so a dim
// party room still gives a punchy image.
#ifdef CAMERA_DEBUG_LOG
// Debug: the full camera frame, decimated 4x, as RGB888 (dumped with 'r').
static uint8_t* s_dbgRgb = nullptr;
static int s_dbgW = 0, s_dbgH = 0;
static void keepDebugRgb(const uint8_t* f) {
  s_dbgW = s_camW / 4;
  s_dbgH = s_camH / 4;
  if (!s_dbgRgb) s_dbgRgb = (uint8_t*)heap_caps_malloc(s_dbgW * s_dbgH * 3, MALLOC_CAP_SPIRAM);
  if (!s_dbgRgb || s_fmt != V4L2_PIX_FMT_RGB565) return;
  for (int y = 0; y < s_dbgH; y++)
    for (int x = 0; x < s_dbgW; x++) {
#ifdef CAMERA_DBG_CROP
      // Full-resolution centre crop instead of a decimated full frame.
      uint32_t sx = (s_camW - s_dbgW) / 2 + x, sy = (s_camH - s_dbgH) / 2 + y;
      uint16_t p = ((const uint16_t*)f)[sy * s_camW + sx];
#else
      uint16_t p = ((const uint16_t*)f)[(y * 4) * s_camW + x * 4];
#endif
      uint8_t* o = s_dbgRgb + (y * s_dbgW + x) * 3;
      o[0] = (p >> 11) << 3; o[1] = ((p >> 5) & 63) << 2; o[2] = (p & 31) << 3;
    }
}
void dumpDebugRgb() {
  if (!s_dbgRgb) return;
  Serial.printf("\n@@PPM@@P6 %d %d 255\n", s_dbgW, s_dbgH);
  for (int off = 0; off < s_dbgW * s_dbgH * 3; off += 2048) {
    Serial.write(s_dbgRgb + off, min(2048, s_dbgW * s_dbgH * 3 - off));
    feedLoopWDT();
  }
  Serial.print("@@END@@\n");
}
#endif

// Centre-crops a camera frame to dst's aspect ratio, scales it down to
// dw x dh grayscale, and stretches the contrast (2%..98% of the histogram ->
// full range), so a dim party room still gives a punchy image. `mirror` flips
// left-right on top of CAMERA_FLIP_H (for the live "mirror" view).
static void processInto(const uint8_t* frame, uint8_t* dst, int dw, int dh, bool mirror) {
  uint32_t cw = s_camW, ch = s_camH;
  if ((uint64_t)cw * dh > (uint64_t)ch * dw) {
    cw = ch * dw / dh;
  } else {
    ch = cw * dh / dw;
  }
  uint32_t cx0 = (s_camW - cw) / 2, cy0 = (s_camH - ch) / 2;
  bool flipH = CAMERA_FLIP_H ^ mirror;

  uint32_t hist[256] = {0};
  for (int y = 0; y < dh; y++) {
    uint32_t sy = cy0 + (uint32_t)y * ch / dh;
    if (CAMERA_FLIP_V) sy = s_camH - 1 - sy;
    uint8_t* row = dst + (size_t)y * dw;
    for (int x = 0; x < dw; x++) {
      uint32_t sx = cx0 + (uint32_t)x * cw / dw;
      if (flipH) sx = s_camW - 1 - sx;
      uint8_t v = lumaAt(frame, sx, sy);
      row[x] = v;
      hist[v]++;
    }
  }
  uint32_t total = (uint32_t)dw * dh, acc = 0;
  int lo = 0, hi = 255;
  for (int i = 0; i < 256; i++) {
    acc += hist[i];
    if (acc < total / 50) lo = i;
    if (acc < total - total / 50) hi = i;
  }
  if (hi <= lo + 16) hi = lo + 16;
  uint8_t lut[256];
  for (int i = 0; i < 256; i++) {
    float v = constrain((float)(i - lo) / (hi - lo), 0.0f, 1.0f);
    lut[i] = (uint8_t)(255.0f * powf(v, s_gamma) + 0.5f);
  }
  for (uint32_t i = 0; i < total; i++) dst[i] = lut[dst[i]];
}

static void process(const uint8_t* frame) {
#ifdef CAMERA_DEBUG_LOG
  keepDebugRgb(frame);
#endif
  processInto(frame, s_photo, s_w, s_h, false);
}

static bool queueAll() {
  for (int i = 0; i < BUFFERS; i++) {
    v4l2_buffer b = {};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = i;
    if (!xioctl(VIDIOC_QBUF, &b, "QBUF")) return false;
  }
  return true;
}

// ---- Auto-exposure ----------------------------------------------------------------
// The core has no ISP tuning (IPA) file for the OV02C10, so the ISP's own
// auto-exposure never runs and the sensor sits at its defaults (1132 lines,
// 8x gain) -- far too dark for a room. This drives the sensor's exposure and
// analog gain directly: exposure first (up to four frame lengths, ~1/7.5 s),
// then gain (up to 15.5x). The settings carry over between captures, so the
// next shot starts close to right.
static constexpr int VTS_30FPS = 1164;          // frame length (lines) of the 1288x728 mode
// Up to four frame lengths (~1/7.5 s): the photo is taken with "HOLD STILL"
// on screen, and a longer exposure means less gain and so less grain.
static constexpr int EXP_MAX = 4 * VTS_30FPS - 32;
static constexpr int GAIN_MIN = 16, GAIN_MAX = 248;  // analog gain x16 (1x .. 15.5x)
static int s_exp = 1132, s_gain = 128;          // sensor defaults

static void applyExposure() {
  int vts = max(VTS_30FPS, s_exp + 32);
  esp_sccb_transmit_reg_a16v8(s_sccb, 0x380e, vts >> 8);
  esp_sccb_transmit_reg_a16v8(s_sccb, 0x380f, vts & 0xff);
  esp_sccb_transmit_reg_a16v8(s_sccb, 0x3501, s_exp >> 8);
  esp_sccb_transmit_reg_a16v8(s_sccb, 0x3502, s_exp & 0xff);
  esp_sccb_transmit_reg_a16v8(s_sccb, 0x3508, (s_gain << 4) >> 8);
  esp_sccb_transmit_reg_a16v8(s_sccb, 0x3509, (s_gain << 4) & 0xff);
}

// Mean luma over a sparse grid, centre rows/columns counted twice (that's
// where a face will be).
static int frameMean(const uint8_t* f) {
  uint32_t sum = 0, n = 0;
  for (uint32_t gy = 1; gy < 18; gy++) {
    for (uint32_t gx = 1; gx < 32; gx++) {
      uint32_t w = (gx > 8 && gx < 24 && gy > 4 && gy < 14) ? 2 : 1;
      sum += w * lumaAt(f, gx * s_camW / 32, gy * s_camH / 18);
      n += w;
    }
  }
  return sum / n;
}

// Returns true when the frame is close enough to the target to keep.
static bool adjustExposure(int mean) {
  if (abs(mean - s_aeTarget) <= s_aeTarget / 5) return true;
  float ratio = constrain((float)s_aeTarget / max(mean, 1), 0.25f, 4.0f);
  float want = (float)s_exp * s_gain * ratio;  // total exposure, lines x gain16
  int exp = constrain((int)(want / GAIN_MIN), 8, EXP_MAX);
  int gain = constrain((int)(want / exp), GAIN_MIN, GAIN_MAX);
  if (exp == s_exp && gain == s_gain) return true;  // at a limit: as good as it gets
  s_exp = exp;
  s_gain = gain;
  applyExposure();
  return false;
}

static void captureOnce() {
  uint32_t t0 = millis();
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
#ifdef CAMERA_DEBUG_LOG
  Serial.println("[D][camera] queue");
#endif
  if (!queueAll()) return;
  if (!xioctl(VIDIOC_STREAMON, &type, "STREAMON")) return;
  // Output-pad enables that Guition's driver has commented out in its
  // stream-on; the OV02C10 driver proven on this board
  // (sullb/esphome-p4-csi-camera) writes them. Not what fixed the zero-frame
  // problem (that was the lane rate, see begin()), kept to match it.
  esp_sccb_transmit_reg_a16v8(s_sccb, 0x3002, 0x01);
  esp_sccb_transmit_reg_a16v8(s_sccb, 0x3010, 0x01);
  esp_sccb_transmit_reg_a16v8(s_sccb, 0x300D, 0x01);
#ifdef CAMERA_TEST_PATTERN
  esp_sccb_transmit_reg_a16v8(s_sccb, 0x4503, 0x80);  // OV02C10 test pattern
#endif
  applyExposure();

  bool got = false;
  int frames = 0, skip = 2, mean = 0;  // first frames still have the old settings
  while (millis() - t0 < CAMERA_MAX_SETTLE_MS + 500) {
    v4l2_buffer b = {};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_fd, VIDIOC_DQBUF, &b) != 0) continue;  // 200 ms timeout, retry
    frames++;
    if (skip > 0) {
      skip--;
    } else {
      mean = frameMean(s_buf[b.index]);
      if (adjustExposure(mean) || millis() - t0 >= CAMERA_MAX_SETTLE_MS) {
        process(s_buf[b.index]);
        got = true;
        break;
      }
      skip = 2;  // new settings take a frame or two to show
    }
    if (!xioctl(VIDIOC_QBUF, &b, "QBUF")) break;
  }
  xioctl(VIDIOC_STREAMOFF, &type, "STREAMOFF");  // sensor back to standby
  Serial.printf("[I][camera] capture %s in %u ms (%d frames, brightness %d, exposure %d, gain %.1fx)\n",
                got ? "ok" : "FAILED", (unsigned)(millis() - t0), frames, mean, s_exp,
                s_gain / 16.0f);
  s_ready = got;
}

// ---- Motion trigger ------------------------------------------------------------
// A 16x9 grid of brightness, compared with the previous peek. Someone stepping
// in front changes the middle of the picture a lot and the edges little; the
// wearer walking around changes everything (ignored).
static constexpr int GW = 16, GH = 9;
static uint8_t s_grid[GW * GH];
static bool s_gridValid = false;
static volatile bool s_motion = false;

static void gridOf(const uint8_t* f, uint8_t* g) {
  for (int gy = 0; gy < GH; gy++) {
    for (int gx = 0; gx < GW; gx++) {
      uint32_t sum = 0;
      for (int sy = 0; sy < 3; sy++)
        for (int sx = 0; sx < 3; sx++)
          sum += lumaAt(f, (gx * 3 + sx) * s_camW / (GW * 3), (gy * 3 + sy) * s_camH / (GH * 3));
      g[gy * GW + gx] = sum / 9;
    }
  }
}

static void motionCheck() {
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!queueAll() || ioctl(s_fd, VIDIOC_STREAMON, &type) != 0) return;
  applyExposure();
  uint8_t g[GW * GH];
  bool got = false;
  for (int frames = 0, tries = 0; frames < 3 && tries < 8; tries++) {
    v4l2_buffer b = {};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_fd, VIDIOC_DQBUF, &b) != 0) continue;
    if (++frames == 3) {
      gridOf(s_buf[b.index], g);
      adjustExposure(frameMean(s_buf[b.index]));  // keep exposure tracking the room
      got = true;
    } else {
      ioctl(s_fd, VIDIOC_QBUF, &b);
    }
  }
  ioctl(s_fd, VIDIOC_STREAMOFF, &type);
  if (!got) return;
  if (s_gridValid) {
    uint32_t centre = 0, edge = 0, nc = 0, ne = 0;
    for (int gy = 0; gy < GH; gy++) {
      for (int gx = 0; gx < GW; gx++) {
        int d = abs((int)g[gy * GW + gx] - (int)s_grid[gy * GW + gx]);
        if (gx >= 5 && gx <= 10 && gy >= 2 && gy <= 7) { centre += d; nc++; }
        else if (gx <= 1 || gx >= 14 || gy == 0) { edge += d; ne++; }
      }
    }
    int c = centre / nc, e = edge / ne;
    if (c >= 22 && e * 3 < c) {
      Serial.printf("[I][camera] motion: centre %d, edges %d\n", c, e);
      s_motion = true;
    }
  }
  memcpy(s_grid, g, sizeof(g));
  s_gridValid = true;
}

static void runLive() {
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!queueAll() || ioctl(s_fd, VIDIOC_STREAMON, &type) != 0) {
    s_liveRun = false;
    return;
  }
  applyExposure();
  int n = 0;
  uint32_t t0 = millis();
  while (s_liveRun && !s_captureRequested) {
    if (millis() - s_livePolled > 1000) {  // the scene was cut short
      s_liveRun = false;
      break;
    }
    v4l2_buffer b = {};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_fd, VIDIOC_DQBUF, &b) != 0) continue;
    if (++n % 3 == 0) adjustExposure(frameMean(s_buf[b.index]));
    int back = s_liveFront ^ 1;
    processInto(s_buf[b.index], s_live[back], s_w, s_h, true);
    s_liveFront = back;
    s_liveFrames = s_liveFrames + 1;
    ioctl(s_fd, VIDIOC_QBUF, &b);
  }
  ioctl(s_fd, VIDIOC_STREAMOFF, &type);
  Serial.printf("[I][camera] live view: %d frames in %u ms\n", n, (unsigned)(millis() - t0));
  s_gridValid = false;
}

static void task(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(MOTION_CHECK_MS));
    if (s_captureRequested) {
      s_captureRequested = false;
#ifdef CAMERA_DEBUG_LOG
      Serial.println("[D][camera] capture requested");
#endif
      captureOnce();
      s_gridValid = false;  // lighting (the flash) changed: start the comparison over
    } else if (s_liveRun) {
      runLive();
    } else if (MOTION_TRIGGER_ENABLED && s_motionEnabled) {
      motionCheck();
    }
  }
}

bool begin(int photoW, int photoH) {
#if !CAMERA_ENABLED
  return false;
#endif
  s_w = photoW;
  s_h = photoH;
  s_photo = (uint8_t*)heap_caps_malloc((size_t)s_w * s_h, MALLOC_CAP_SPIRAM);
  if (!s_photo) return false;

#ifdef CAMERA_DEBUG_LOG
  esp_log_level_set("*", ESP_LOG_INFO);
#endif
  // The board's camera is an OmniVision OV02C10 (I2C 0x36, chip ID 0x5602).
  // The core's prebuilt camera stack has a fixed list of sensor drivers that
  // doesn't include it, so: create the ISP device through the core, then
  // detect the sensor with Guition's driver and register it ourselves -- the
  // same steps esp_video_init() takes for a sensor it knows.
  if (esp_video_init_with_flags(&(const esp_video_init_config_t&)esp_video_init_config_t{},
                                ESP_VIDEO_INIT_FLAGS_ISP) != ESP_OK) {
    Serial.println("[E][camera] ISP init failed");
    return false;
  }
  // Control bus = Arduino's I2C bus 0 (SDA 7 / SCL 8), shared with touch and audio.
  auto bus = (i2c_master_bus_handle_t)i2cBusHandle(0);
  esp_sccb_io_handle_t sccb = nullptr;
  sccb_i2c_config_t sc = {};
  sc.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  sc.device_address = OV02C10_SCCB_ADDR;
  sc.scl_speed_hz = 100000;
  if (!bus || sccb_new_i2c_io(bus, &sc, &sccb) != ESP_OK) {
    Serial.println("[E][camera] SCCB setup failed");
    return false;
  }
  esp_cam_sensor_config_t scfg = {};
  scfg.sccb_handle = sccb;
  scfg.reset_pin = GPIO_NUM_NC;
  scfg.pwdn_pin = GPIO_NUM_NC;
  scfg.xclk_pin = GPIO_NUM_NC;
  scfg.sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
  s_sccb = sccb;
  esp_cam_sensor_device_t* cam = ov02c10_detect(&scfg);
  if (!cam) {
    Serial.println("[I][camera] no OV02C10 camera found");
    esp_sccb_del_i2c_io(sccb);
    return false;
  }
  // Guition's format table tells the CSI receiver 400 Mbps per lane, but the
  // sensor's PLL settings work out to ~425 Mbps. The receiver derives its
  // HS-settle timing from that number, and at 400 it never locks (start-of-
  // transmission errors, zero frames). Measured on this board: 200 and 400
  // fail, 425-1000 all capture. Use a copy of the format with a rate in the
  // middle of the working range (the driver file itself is unchanged).
  static esp_cam_sensor_format_t sensorFmt = *cam->cur_format;
  sensorFmt.mipi_info.mipi_clk = (uint32_t)CAMERA_MIPI_MBPS * 1000000u;
#ifdef CAMERA_BAYER
  // The colour-filter order the ISP demosaics with (see config.h).
  static esp_cam_sensor_isp_info_t ispInfo = *sensorFmt.isp_info;
  ispInfo.isp_v1_info.bayer_type = (esp_cam_sensor_bayer_pattern_t)CAMERA_BAYER;
  sensorFmt.isp_info = &ispInfo;
#endif
  if (esp_cam_sensor_set_format(cam, &sensorFmt) != ESP_OK) {
    Serial.println("[E][camera] set format failed");
    return false;
  }
  esp_video_csi_device_config_t csiCfg = {false};
  if (esp_video_create_csi_video_device(cam, &csiCfg) != ESP_OK) {
    Serial.println("[E][camera] CSI video device setup failed");
    return false;
  }

  s_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
  if (s_fd < 0) {
    Serial.println("[E][camera] open failed");
    return false;
  }
  // This esp_video ignores O_NONBLOCK; bound each dequeue wait instead, so a
  // stalled camera can never hang the capture task.
  timeval dqTimeout = {0, 200000};
  xioctl(VIDIOC_S_DQBUF_TIMEOUT, &dqTimeout, "S_DQBUF_TIMEOUT");
  v4l2_capability cap = {};
  xioctl(VIDIOC_QUERYCAP, &cap, "QUERYCAP");

  // Prefer plain grayscale out of the ISP; otherwise RGB565.
  bool grey = false;
  for (int i = 0;; i++) {
    v4l2_fmtdesc d = {};
    d.index = i;
    d.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_fd, VIDIOC_ENUM_FMT, &d) != 0) break;
    if (d.pixelformat == V4L2_PIX_FMT_GREY) grey = true;
  }
  v4l2_format fmt = {};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!xioctl(VIDIOC_G_FMT, &fmt, "G_FMT")) return false;
  s_camW = fmt.fmt.pix.width;
  s_camH = fmt.fmt.pix.height;
  fmt.fmt.pix.pixelformat = grey ? V4L2_PIX_FMT_GREY : V4L2_PIX_FMT_RGB565;
  if (!xioctl(VIDIOC_S_FMT, &fmt, "S_FMT")) return false;
  s_fmt = fmt.fmt.pix.pixelformat;
#ifdef CAMERA_DEBUG_LOG
  {
    v4l2_format chk = {};
    chk.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(s_fd, VIDIOC_G_FMT, &chk);
    uint32_t pf = chk.fmt.pix.pixelformat;
    Serial.printf("[D][camera] G_FMT after S_FMT: %ux%u fourcc %c%c%c%c bpl %u size %u\n",
                  (unsigned)chk.fmt.pix.width, (unsigned)chk.fmt.pix.height, (char)(pf & 255),
                  (char)((pf >> 8) & 255), (char)((pf >> 16) & 255), (char)(pf >> 24),
                  (unsigned)chk.fmt.pix.bytesperline, (unsigned)chk.fmt.pix.sizeimage);
    for (int i = 0;; i++) {
      v4l2_fmtdesc d = {};
      d.index = i;
      d.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      if (ioctl(s_fd, VIDIOC_ENUM_FMT, &d) != 0) break;
      uint32_t f = d.pixelformat;
      Serial.printf("[D][camera]   supports %c%c%c%c\n", (char)(f & 255), (char)((f >> 8) & 255),
                    (char)((f >> 16) & 255), (char)(f >> 24));
    }
  }
#endif

  v4l2_requestbuffers req = {};
  req.count = BUFFERS;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (!xioctl(VIDIOC_REQBUFS, &req, "REQBUFS")) return false;
  for (int i = 0; i < BUFFERS; i++) {
    v4l2_buffer b = {};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = i;
    if (!xioctl(VIDIOC_QUERYBUF, &b, "QUERYBUF")) return false;
    s_bufLen[i] = b.length;
#ifdef CAMERA_DEBUG_LOG
    Serial.printf("[D][camera] buffer %d: %u bytes\n", i, (unsigned)b.length);
#endif
    s_buf[i] = (uint8_t*)mmap(nullptr, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, s_fd,
                              b.m.offset);
    if (!s_buf[i]) return false;
  }

  xTaskCreatePinnedToCore(task, "camera", 6144, nullptr, 2, &s_task, 1);
  s_ok = true;
  Serial.printf("[I][camera] %s, %ux%u, %s\n", (const char*)cap.card, (unsigned)s_camW,
                (unsigned)s_camH, grey ? "GREY" : "RGB565");
  return true;
}

bool available() { return s_ok; }

void setLight(int level) {
  level = constrain(level, 0, 6);
  s_aeTarget = 85 + 15 * level;
  s_gamma = 1.0f - 0.08f * level;
}

void requestCapture() {
  s_ready = false;
  s_captureRequested = true;
  if (s_task) xTaskNotifyGive(s_task);
}

bool startLive() {
  if (!s_ok) return false;
  for (auto& b : s_live) {
    if (!b) b = (uint8_t*)heap_caps_malloc((size_t)s_w * s_h, MALLOC_CAP_SPIRAM);
    if (!b) return false;
  }
  s_liveFrames = 0;
  s_livePolled = millis();
  s_liveRun = true;
  if (s_task) xTaskNotifyGive(s_task);
  return true;
}

void stopLive() { s_liveRun = false; }

uint32_t liveFrame(const uint8_t** pixels) {
  s_livePolled = millis();
  *pixels = s_live[s_liveFront];
  return s_liveFrames;
}

bool ready() { return s_ready; }

bool takeMotion() {
  bool m = s_motion;
  s_motion = false;
  return m;
}

void setMotionEnabled(bool on) {
  s_motionEnabled = on;
  if (!on) s_motion = false;
}

Photo photo() {
  Photo p;
  p.w = s_w;
  p.h = s_h;
  p.pixels = s_photo;
  return p;
}

}  // namespace camera
