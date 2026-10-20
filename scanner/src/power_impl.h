#pragma once
// Battery level, charging, and powering off. Included ONCE, from main.cpp,
// after sound_impl.h (BusLock) -- on the Waveshare, SYS_EN is an I/O expander
// bit whose state is kept per file (see sound_impl.h).
//
// Waveshare 3.49: battery voltage on GPIO4 (x3 divider, Waveshare's ADC demo);
//   no charge-status pin, so charging is inferred from the voltage, like the
//   costume. PWR key on GPIO16 (low = pressed); powering off = SYS_EN low
//   (expander P6), which only cuts power on battery.
// Tab5: INA226 battery monitor at 0x41 (2S pack: bus voltage / 2 per cell;
//   charge current reads negative), charger CHG_STAT on expander 0x44 bit 6,
//   power off = pulses on expander 0x44 bit 4 (PWROFF_PULSE) -- all from
//   M5Unified's Tab5 code. The power button itself isn't readable here.
#include "config.h"

namespace power {

struct Point { float v; int pct; };
// Li-ion cell voltage -> charge, under load.
static const Point CURVE[] = {
    {4.10f, 100}, {4.00f, 90}, {3.90f, 78}, {3.80f, 62}, {3.70f, 45},
    {3.60f, 25},  {3.50f, 12}, {3.40f, 5},  {3.30f, 0},
};

static int toPercent(float v) {
  if (v >= CURVE[0].v) return 100;
  for (size_t i = 1; i < sizeof(CURVE) / sizeof(CURVE[0]); i++) {
    if (v >= CURVE[i].v) {
      const Point& hi = CURVE[i - 1];
      const Point& lo = CURVE[i];
      return lo.pct + (int)((v - lo.v) / (hi.v - lo.v) * (hi.pct - lo.pct) + 0.5f);
    }
  }
  return 0;
}

static float s_volts = 0;  // smoothed, per cell
static int s_percent = -1;
static bool s_charging = false;
static uint32_t s_nextSample = 0;

#ifdef SCANNER_BOARD_TAB5
static constexpr uint8_t INA226_ADDR = 0x41, IOE2_ADDR = 0x44;

static bool inaRead(uint8_t reg, int16_t* out) {
  uint8_t b[2];
  BusLock l;
  if (!lgfx::i2c::transactionWriteRead(tab5_i2c_port, INA226_ADDR, &reg, 1, b, 2, 400000).has_value()) return false;
  *out = (int16_t)((b[0] << 8) | b[1]);
  return true;
}

// Per-cell voltage, or 0 if the monitor didn't answer.
static float readVolts() {
  int16_t raw;
  return inaRead(0x02, &raw) ? raw * 0.00125f / 2 : 0;
}

static bool readCharging(float* mA) {
  int16_t shunt;
  *mA = inaRead(0x01, &shunt) ? -shunt * 2.5e-6f / 0.005f * 1000 : 0;  // + = charging
  uint8_t reg = 0x0F, in = 0;  // input status
  BusLock l;
  lgfx::i2c::transactionWriteRead(tab5_i2c_port, IOE2_ADDR, &reg, 1, &in, 1, 400000);
  return in & (1 << 6);  // CHG_STAT
}

void begin() {
  s_volts = readVolts();
  s_percent = s_volts < 2.5f ? -1 : toPercent(s_volts);
}

void tick(uint32_t now) {
  if ((int32_t)(now - s_nextSample) < 0) return;
  s_nextSample = now + 2000;
  float v = readVolts(), mA;
  bool chg = readCharging(&mA);
  if (v < 2.5f) {
    s_percent = -1;
    return;
  }
  s_volts = s_percent < 0 ? v : s_volts + (v - s_volts) * 0.2f;
  int p = toPercent(s_volts);
  if (s_percent < 0 || abs(p - s_percent) >= 2 || p == 0 || p == 100) s_percent = p;
  // CHG_STAT toggles while the charger tops off a full battery: change only
  // after 3 readings (6 s) in a row agree.
  static int same = 0;
  static bool last = false;
  same = chg == last ? same + 1 : 0;
  last = chg;
  bool changed = same >= 2 && chg != s_charging;
  static uint32_t nextLog = 0;
  if (changed || (int32_t)(now - nextLog) >= 0) {
    if (changed) s_charging = chg;
    nextLog = now + 30000;
    Serial.printf("[power] %s, %.2f V/cell, %d%%, %.0f mA\n", chg ? "charging" : "on battery", v, s_percent, mA);
  }
}

static void cutPower() {
  for (int i = 0; i < 10; i++) {
    {
      BusLock l;
      lgfx::i2c::writeRegister8(tab5_i2c_port, IOE2_ADDR, 0x05, (i & 1) ? 0x10 : 0x00, 0xEF, 400000);
    }
    delay(50);
  }
}

static bool keyDown() { return false; }

#else  // Waveshare 3.49

static constexpr int BAT_PIN = 4, PWR_KEY_PIN = 16;
// Charge detection without a status pin (same idea as the costume): plugging
// in steps the voltage up, unplugging steps it down; a one-minute trend backs
// that up.
static constexpr float STEP_V = 0.06f, TREND_UP_V = 0.020f, TREND_DOWN_V = 0.015f, BOOT_CHARGING_V = 4.17f;
static constexpr int N = 16;
static float s_hist[N];
static int s_head = 0;
static float s_minuteAvg = 0, s_minuteSum = 0;
static int s_minuteN = 0;

static float readVolts() {
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(BAT_PIN);
  return mv / 8.0f / 1000.0f * 3;
}

static float histAt(int ago) { return s_hist[(s_head - 1 - ago + 2 * N) % N]; }
static float histMean(int from, int to) {
  float sum = 0;
  for (int i = from; i <= to; i++) sum += histAt(i);
  return sum / (to - from + 1);
}

static void setCharging(bool c, float before, float after, const char* why) {
  if (c == s_charging) return;
  s_charging = c;
  s_volts = after;
  Serial.printf("[power] %s (%.3f -> %.3f V, %s)\n", c ? "charging" : "on battery", before, after, why);
}

void begin() {
  pinMode(PWR_KEY_PIN, INPUT_PULLUP);
  analogReadResolution(12);
  s_volts = readVolts();
  for (int i = 0; i < N; i++) s_hist[i] = s_volts;
  s_minuteAvg = s_volts;
  if (s_volts >= BOOT_CHARGING_V) setCharging(true, s_volts, s_volts, "boot");
  s_percent = s_volts < 2.5f ? -1 : toPercent(s_volts);
}

void tick(uint32_t now) {
  if ((int32_t)(now - s_nextSample) < 0) return;
  s_nextSample = now + 1000;
  float v = readVolts();
  s_hist[s_head] = v;
  s_head = (s_head + 1) % N;
  if (v < 2.5f) {
    s_percent = -1;
    return;
  }
  float recent = histMean(0, 2), earlier = histMean(9, 12);
  if (recent - earlier >= STEP_V) setCharging(true, earlier, recent, "step");
  else if (earlier - recent >= STEP_V) setCharging(false, earlier, recent, "step");
  s_minuteSum += v;
  if (++s_minuteN == 60) {
    float avg = s_minuteSum / 60;
    if (!s_charging && avg - s_minuteAvg >= TREND_UP_V) setCharging(true, s_minuteAvg, avg, "trend");
    else if (s_charging && s_minuteAvg - avg >= TREND_DOWN_V) setCharging(false, s_minuteAvg, avg, "trend");
    s_minuteAvg = avg;
    s_minuteSum = 0;
    s_minuteN = 0;
  }
  s_volts += (v - s_volts) * 0.1f;
  int p = toPercent(s_volts);
  if (s_percent < 0 || abs(p - s_percent) >= 2 || p == 0 || p == 100) s_percent = p;
  static uint32_t nextLog = 0;
  if ((int32_t)(now - nextLog) >= 0) {
    nextLog = now + 30000;
    Serial.printf("[power] %.3f V, %d%%, %s\n", v, s_percent, s_charging ? "charging" : "on battery");
  }
}

static void cutPower() {
  BusLock l;
  waveshare349::expanderSet(waveshare349::kExioSysEn, false);
}

static bool keyDown() { return digitalRead(PWR_KEY_PIN) == LOW; }

#endif

int percent() { return s_percent; }
bool charging() { return s_charging; }
float volts() { return s_volts; }

// The PWR key held for POWER_HOLD_MS. The press that switched the board on is
// ignored: the key has to be seen released first.
static bool s_keyArmed = false;
static uint32_t s_keySince = 0;
float keyHold(uint32_t now) {  // 0..1 while held, 0 otherwise
  if (!keyDown()) {
    s_keyArmed = true;
    s_keySince = 0;
    return 0;
  }
  if (!s_keyArmed) return 0;
  if (!s_keySince) s_keySince = now;
  return min(1.0f, (now - s_keySince) / (float)POWER_HOLD_MS);
}

}  // namespace power
