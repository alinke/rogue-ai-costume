#pragma once
// Tab5 camera (SC2356 = the core's SC202CS driver, MIPI-CSI, 1280x720) for the
// researcher's evidence photos. Included ONCE, from main.cpp, after
// sound_impl.h (BusLock): the sensor's control bus is the Tab5's internal I2C
// bus, which LovyanGFX drives directly (touch, I/O expanders), so the camera
// driver gets a small SCCB adapter that goes through LovyanGFX's I2C under the
// same lock, instead of an ESP-IDF I2C driver fighting it for the pins.
// Wiring from M5Stack's Tab5 demo (M5Tab5-UserDemo, bsp_cam_osc_init and
// hal_camera.cpp): 24 MHz sensor clock on GPIO36, reset on I/O expander 0x43
// bit 6 (already released by LGFX_Config.hpp's init).
#include <driver/ledc.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "esp_cam_sensor.h"
#include "esp_sccb_io_interface.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "esp_video_isp_ioctl.h"
#include "linux/videodev2.h"
#include "sc202cs.h"

// Exported by the core's prebuilt esp_video but declared only in its private
// header (same as the costume's camera.cpp).
extern "C" {
typedef struct esp_video_csi_device_config {
  bool dont_init_ldo;
} esp_video_csi_device_config_t;
esp_err_t esp_video_create_csi_video_device(esp_cam_sensor_device_t* cam_dev,
                                            const esp_video_csi_device_config_t* config);
}

namespace tab5cam {

static constexpr int BUFFERS = 2;
static bool s_ok = false, s_streaming = false;
static int s_fd = -1;
static int s_w = 0, s_h = 0;
static uint8_t* s_buf[BUFFERS];
static size_t s_bufLen[BUFFERS];
static int s_held = -1;  // buffer index handed out by frame(), returned by the next call

// ---- White balance ------------------------------------------------------------
// The ISP applies no white balance on its own here (everything came out
// green), so: grey-world auto white balance. Every few frames, the frame's
// average R, G, B (sampled, ignoring near-black and clipped pixels) nudges
// the ISP's red/blue gains towards a neutral average. Gains carry over
// between viewfinder sessions.
static int s_ispFd = -1;
static float s_rGain = 1.6f, s_bGain = 1.6f;
static uint32_t s_frames = 0;

static void applyWb() {
  if (s_ispFd < 0) return;
  esp_video_isp_wb_t wb = {};
  wb.enable = true;
  wb.red_gain = s_rGain;
  wb.blue_gain = s_bGain;
  v4l2_ext_control c = {};
  c.id = V4L2_CID_USER_ESP_ISP_WB;
  c.size = sizeof(wb);
  c.p_u8 = (uint8_t*)&wb;
  v4l2_ext_controls cs = {};
  cs.ctrl_class = V4L2_CTRL_CLASS_USER;
  cs.count = 1;
  cs.controls = &c;
  if (ioctl(s_ispFd, VIDIOC_S_EXT_CTRLS, &cs) != 0) {
    Serial.printf("[camera] white balance not applied (errno %d)\n", errno);
    close(s_ispFd);
    s_ispFd = -1;
  }
}

static void autoWb(const uint16_t* px, int w, int h) {
  uint32_t r = 0, g = 0, b = 0, n = 0;
  for (int y = 8; y < h; y += 16) {
    for (int x = 8; x < w; x += 16) {
      uint16_t v = px[y * w + x];
      uint32_t pr = (v >> 11) << 3, pg = ((v >> 5) & 0x3F) << 2, pb = (v & 0x1F) << 3;
      uint32_t mx = max(pr, max(pg, pb));
      if (mx < 24 || mx > 240) continue;  // too dark to tell / clipped
      r += pr; g += pg; b += pb; n++;
    }
  }
  if (n < 200 || !r || !b) return;
  // Partial steps (square root) so it settles without hunting.
  s_rGain = constrain(s_rGain * sqrtf((float)g / r), 0.5f, 6.0f);
  s_bGain = constrain(s_bGain * sqrtf((float)g / b), 0.5f, 6.0f);
  applyWb();
}

// ---- SCCB over LovyanGFX's I2C ----------------------------------------------------
struct Sccb {
  esp_sccb_io_t io;
  uint8_t addr;
};

static esp_err_t sccbTx(esp_sccb_io_t* h, const uint8_t* wb, size_t n, int) {
  BusLock l;
  return lgfx::i2c::transactionWrite(tab5_i2c_port, ((Sccb*)h)->addr, wb, n, 400000).has_value() ? ESP_OK
                                                                                                    : ESP_FAIL;
}
static esp_err_t sccbTxRx(esp_sccb_io_t* h, const uint8_t* wb, size_t n, uint8_t* rb, size_t rn, int) {
  BusLock l;
  return lgfx::i2c::transactionWriteRead(tab5_i2c_port, ((Sccb*)h)->addr, wb, n, rb, rn, 400000).has_value()
             ? ESP_OK
             : ESP_FAIL;
}
static esp_err_t sccbRx(esp_sccb_io_t* h, uint8_t* rb, size_t rn, int) {
  BusLock l;
  return lgfx::i2c::transactionRead(tab5_i2c_port, ((Sccb*)h)->addr, rb, rn, 400000).has_value() ? ESP_OK
                                                                                                  : ESP_FAIL;
}
static esp_err_t sccbDel(esp_sccb_io_t*) { return ESP_OK; }

static Sccb s_sccb;

static bool xioctl(unsigned long req, void* arg, const char* what) {
  if (ioctl(s_fd, req, arg) == 0) return true;
  Serial.printf("[camera] %s failed (errno %d)\n", what, errno);
  return false;
}

bool begin() {
  // Sensor clock: 24 MHz on GPIO36 (LEDC timer 0 -- the backlight uses timer 3).
  ledc_timer_config_t t = {};
  t.speed_mode = LEDC_LOW_SPEED_MODE;
  t.duty_resolution = LEDC_TIMER_1_BIT;
  t.timer_num = LEDC_TIMER_0;
  t.freq_hz = 24000000;
  t.clk_cfg = LEDC_AUTO_CLK;
  ledc_channel_config_t c = {};
  c.gpio_num = 36;
  c.speed_mode = LEDC_LOW_SPEED_MODE;
  c.channel = LEDC_CHANNEL_0;
  c.timer_sel = LEDC_TIMER_0;
  c.duty = 1;
  if (ledc_timer_config(&t) != ESP_OK || ledc_channel_config(&c) != ESP_OK) {
    Serial.println("[camera] sensor clock setup failed");
    return false;
  }
  delay(10);

  if (esp_video_init_with_flags(&(const esp_video_init_config_t&)esp_video_init_config_t{},
                                ESP_VIDEO_INIT_FLAGS_ISP) != ESP_OK) {
    Serial.println("[camera] ISP init failed");
    return false;
  }
  s_sccb.io = {};
  s_sccb.io.transmit_reg_a8v8 = sccbTx;
  s_sccb.io.transmit_reg_a16v8 = sccbTx;
  s_sccb.io.transmit_reg_a8v16 = sccbTx;
  s_sccb.io.transmit_reg_a16v16 = sccbTx;
  s_sccb.io.transmit_reg_a16v32 = sccbTx;
  s_sccb.io.transmit_receive_reg_a8v8 = sccbTxRx;
  s_sccb.io.transmit_receive_reg_a16v8 = sccbTxRx;
  s_sccb.io.transmit_receive_reg_a8v16 = sccbTxRx;
  s_sccb.io.transmit_receive_reg_a16v16 = sccbTxRx;
  s_sccb.io.transmit_receive_reg_a16v32 = sccbTxRx;
  s_sccb.io.transmit_v16 = sccbTx;
  s_sccb.io.receive_v16 = sccbRx;
  s_sccb.io.del = sccbDel;
  s_sccb.addr = SC202CS_SCCB_ADDR;

  esp_cam_sensor_config_t scfg = {};
  scfg.sccb_handle = &s_sccb.io;
  scfg.reset_pin = GPIO_NUM_NC;
  scfg.pwdn_pin = GPIO_NUM_NC;
  scfg.xclk_pin = GPIO_NUM_NC;
  scfg.sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
  esp_cam_sensor_device_t* cam = sc202cs_detect(&scfg);
  if (!cam) {
    Serial.println("[camera] no SC2356/SC202CS camera found");
    return false;
  }
  // Pick the 1280x720 MIPI format explicitly (the driver doesn't apply one
  // until asked; without it the ISP gets data it doesn't expect).
  esp_cam_sensor_format_array_t fa = {};
  esp_cam_sensor_query_format(cam, &fa);
  const esp_cam_sensor_format_t* pick = nullptr;
  for (uint32_t i = 0; i < fa.count; i++) {
    const esp_cam_sensor_format_t& f = fa.format_array[i];
    Serial.printf("[camera] format %u: %s (%ux%u, %u fps, port %d)\n", (unsigned)i, f.name, f.width, f.height,
                  f.fps, (int)f.port);
    if (!pick && f.port == ESP_CAM_SENSOR_MIPI_CSI && f.width == 1280 && f.height == 720) pick = &f;
  }
  if (!pick || esp_cam_sensor_set_format(cam, pick) != ESP_OK) {
    Serial.println("[camera] no usable 1280x720 MIPI format");
    return false;
  }
  Serial.printf("[camera] using %s, MIPI %u Mbps x %u lanes\n", pick->name,
                (unsigned)(pick->mipi_info.mipi_clk / 1000000), (unsigned)pick->mipi_info.lane_num);
  esp_video_csi_device_config_t csiCfg = {false};
  if (esp_video_create_csi_video_device(cam, &csiCfg) != ESP_OK) {
    Serial.println("[camera] CSI video device setup failed");
    return false;
  }
  s_ispFd = open(ESP_VIDEO_ISP1_DEVICE_NAME, O_RDWR);
  s_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
  if (s_fd < 0) {
    Serial.println("[camera] open failed");
    return false;
  }
  timeval dqTimeout = {0, 300000};  // bound each dequeue wait (O_NONBLOCK is ignored)
  xioctl(VIDIOC_S_DQBUF_TIMEOUT, &dqTimeout, "S_DQBUF_TIMEOUT");
  v4l2_format fmt = {};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!xioctl(VIDIOC_G_FMT, &fmt, "G_FMT")) return false;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  if (!xioctl(VIDIOC_S_FMT, &fmt, "S_FMT")) return false;
  s_w = fmt.fmt.pix.width;
  s_h = fmt.fmt.pix.height;
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
    s_buf[i] = (uint8_t*)mmap(nullptr, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, s_fd, b.m.offset);
    if (!s_buf[i]) return false;
  }
  s_ok = true;
  Serial.printf("[camera] SC2356 ready, %dx%d RGB565\n", s_w, s_h);
  return true;
}

bool ok() { return s_ok; }
int width() { return s_w; }
int height() { return s_h; }

// The sensor streams only while the viewfinder is up.
bool start() {
  if (!s_ok || s_streaming) return s_streaming;
  for (int i = 0; i < BUFFERS; i++) {
    v4l2_buffer b = {};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = i;
    if (!xioctl(VIDIOC_QBUF, &b, "QBUF")) return false;
  }
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!xioctl(VIDIOC_STREAMON, &type, "STREAMON")) return false;
  s_streaming = true;
  s_held = -1;
  applyWb();
  return true;
}

void stop() {
  if (!s_streaming) return;
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  xioctl(VIDIOC_STREAMOFF, &type, "STREAMOFF");
  s_streaming = false;
  s_held = -1;
}

// Newest frame (RGB565, little-endian, width() x height()), valid until the
// next call; nullptr if none arrived in time.
const uint16_t* frame() {
  if (!s_streaming) return nullptr;
  if (s_held >= 0) {  // give the previous one back
    v4l2_buffer b = {};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = s_held;
    ioctl(s_fd, VIDIOC_QBUF, &b);
    s_held = -1;
  }
  v4l2_buffer b = {};
  b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  b.memory = V4L2_MEMORY_MMAP;
  if (ioctl(s_fd, VIDIOC_DQBUF, &b) != 0) return nullptr;
  s_held = b.index;
  const uint16_t* px = (const uint16_t*)s_buf[b.index];
  if (++s_frames % 8 == 0) autoWb(px, s_w, s_h);
  return px;
}

}  // namespace tab5cam
