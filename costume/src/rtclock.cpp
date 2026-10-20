#include "rtclock.h"

#include <Wire.h>

#include "config.h"

namespace rtclock {

static constexpr uint8_t ADDR = 0x32;
static constexpr uint8_t REG_SEC = 0x00, REG_FLAG = 0x0E, FLAG_VLF = 0x02;

static bool s_present = false, s_valid = false;
static int s_h = 0, s_m = 0, s_s = 0;
static uint32_t s_nextRead = 0;
static int32_t s_lastToTarget = -1;
static bool s_targetEvent = false;

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 15); }
static uint8_t bin2bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static bool readRegs(uint8_t reg, uint8_t* buf, int n) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ADDR, n) != n) return false;
  for (int i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static bool writeRegs(uint8_t reg, const uint8_t* buf, int n) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  for (int i = 0; i < n; i++) Wire.write(buf[i]);
  return Wire.endTransmission() == 0;
}

static void readTime() {
  uint8_t t[7], flag;
  if (!readRegs(REG_SEC, t, 7) || !readRegs(REG_FLAG, &flag, 1)) {
    s_present = false;
    return;
  }
  s_present = true;
  s_s = bcd2bin(t[0] & 0x7F);
  s_m = bcd2bin(t[1] & 0x7F);
  s_h = bcd2bin(t[2] & 0x3F);
  s_valid = !(flag & FLAG_VLF) && bcd2bin(t[6]) >= 24;
}

void begin() {
  readTime();
  Serial.printf("[I][clock] %s%s, %02d:%02d:%02d\n", s_present ? "RX8025T found" : "no clock chip",
                s_present ? (s_valid ? ", time set" : ", time NOT set (send T...)") : "", s_h, s_m, s_s);
}

void tick(uint32_t now) {
  if ((int32_t)(now - s_nextRead) < 0) return;
  s_nextRead = now + 1000;
  readTime();
  if (!s_valid) return;
  int32_t to = secondsToTarget();
  // The target passes when the countdown wraps from a few seconds to ~24 h.
  if (s_lastToTarget >= 0 && s_lastToTarget <= 5 && to > 86000) s_targetEvent = true;
  s_lastToTarget = to;
}

bool valid() { return s_present && s_valid; }
int hour() { return s_h; }
int minute() { return s_m; }
int second() { return s_s; }

int32_t secondsToTarget() {
  int32_t nowS = s_h * 3600 + s_m * 60 + s_s;
  int32_t target = COUNTDOWN_HOUR * 3600 + COUNTDOWN_MINUTE * 60;
  return ((target - nowS) % 86400 + 86400) % 86400;
}

int32_t secondsSinceTarget() { return (86400 - secondsToTarget()) % 86400; }

bool takeTargetEvent() {
  bool e = s_targetEvent;
  s_targetEvent = false;
  return e;
}

bool set(const char* text) {
  int y, mo, d, h, mi, s;
  if (sscanf(text, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6 || y < 2024) return false;
  uint8_t t[7] = {bin2bcd(s), bin2bcd(mi), bin2bcd(h), 1, bin2bcd(d), bin2bcd(mo), bin2bcd(y % 100)};
  uint8_t flag = 0;  // clears VLF: the time is good again
  bool ok = writeRegs(REG_SEC, t, 7) && writeRegs(REG_FLAG, &flag, 1);
  readTime();
  s_lastToTarget = -1;
  Serial.printf("[I][clock] set to %s: %s\n", text, ok ? "ok" : "FAILED");
  return ok;
}

}  // namespace rtclock
