#include "battery.h"

#include "config.h"

namespace battery {

// ---- Charge detection tuning ----------------------------------------------------
// There's no charge-status pin: the IP5306's charge LEDs aren't wired to the
// P4, and USB VBUS isn't either (schematics 1_PWR / 6_USB). Charging is inferred
// from the cell voltage instead:
//  * Plugging in moves the display's load onto USB and starts the charger, so
//    the voltage jumps up within seconds; unplugging drops it by about as much.
//  * As a backstop, a rising one-minute trend means charging, a falling one
//    means running on the battery.
static constexpr float STEP_V = 0.06f;        // jump between "now" and ~10 s ago
static constexpr float TREND_UP_V = 0.020f;   // per minute, while thought discharging
static constexpr float TREND_DOWN_V = 0.015f; // per minute, while thought charging
static constexpr float BOOT_CHARGING_V = 4.17f;

static float s_volts = 0.0f;   // smoothed, for the percentage
static int s_percent = -1;
static bool s_charging = false;
static uint32_t s_nextSample = 0;

// Raw one-second samples, newest at s_hist[(s_head - 1) % N].
static constexpr int N = 16;
static float s_hist[N];
static int s_head = 0, s_count = 0;
static float s_minuteAvg = 0.0f, s_minuteSum = 0.0f;
static int s_minuteN = 0;

// Last few charge-state changes, printed by the 'b' serial command.
struct Event { uint32_t s; bool charging; float before, after; char why; };
static Event s_events[12];
static int s_eventCount = 0;

// Li-ion cell voltage -> charge, under a ~0.5C load (the display draws a few
// watts, which sags the cell ~0.1 V below its resting voltage).
struct Point { float v; int pct; };
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

static float readVolts() {
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(BATTERY_SENSE_PIN);
  return mv / 8.0f / 1000.0f / BATTERY_DIVIDER;
}

static float histAt(int ago) { return s_hist[(s_head - 1 - ago + 2 * N) % N]; }

static float histMean(int fromAgo, int toAgo) {
  float sum = 0;
  for (int i = fromAgo; i <= toAgo; i++) sum += histAt(i);
  return sum / (toAgo - fromAgo + 1);
}

static void setCharging(bool c, float before, float after, char why, uint32_t now) {
  if (c == s_charging) return;
  s_charging = c;
  s_volts = after;  // restart smoothing from the new level
  Event& e = s_events[s_eventCount++ % (int)(sizeof(s_events) / sizeof(s_events[0]))];
  e = {now / 1000, c, before, after, why};
  Serial.printf("[I][battery] %s (%.3f -> %.3f V, %s)\n", c ? "charging" : "on battery", before,
                after, why == 's' ? "step" : why == 't' ? "trend" : "boot");
}

void begin() {
  analogReadResolution(12);
  s_volts = readVolts();
  for (int i = 0; i < N; i++) s_hist[i] = s_volts;
  s_head = 0;
  s_count = N;
  s_minuteAvg = s_volts;
  if (s_volts >= BOOT_CHARGING_V) setCharging(true, s_volts, s_volts, 'b', millis());
  s_percent = s_volts < 2.5f ? -1 : toPercent(s_volts);
  Serial.printf("[I][battery] %.2f V, %d%%%s\n", s_volts, s_percent, s_charging ? ", charging" : "");
}

void tick(uint32_t now) {
  if ((int32_t)(now - s_nextSample) < 0) return;
  s_nextSample = now + 1000;

  float v = readVolts();
  s_hist[s_head] = v;
  s_head = (s_head + 1) % N;
  if (s_count < N) s_count++;

  if (v < 2.5f) {
    s_percent = -1;  // nothing on the battery connector
    return;
  }

  // Step: last 3 s against 9-12 s ago.
  float recent = histMean(0, 2), earlier = histMean(9, 12);
  if (recent - earlier >= STEP_V) setCharging(true, earlier, recent, 's', now);
  else if (earlier - recent >= STEP_V) setCharging(false, earlier, recent, 's', now);

  // Trend: one-minute averages.
  s_minuteSum += v;
  if (++s_minuteN == 60) {
    float avg = s_minuteSum / 60;
    if (!s_charging && avg - s_minuteAvg >= TREND_UP_V) setCharging(true, s_minuteAvg, avg, 't', now);
    else if (s_charging && s_minuteAvg - avg >= TREND_DOWN_V) setCharging(false, s_minuteAvg, avg, 't', now);
    s_minuteAvg = avg;
    s_minuteSum = 0;
    s_minuteN = 0;
  }

  s_volts += (v - s_volts) * 0.1f;  // ~10 s smoothing
  int p = toPercent(s_volts);
  // Hysteresis: don't flicker between two neighbouring values.
  if (s_percent < 0 || abs(p - s_percent) >= 2 || p == 0 || p == 100) s_percent = p;
}

float volts() { return s_volts; }
int percent() { return s_percent; }
bool charging() { return s_charging; }

void printEvents() {
  int n = min(s_eventCount, (int)(sizeof(s_events) / sizeof(s_events[0])));
  Serial.printf("[I][battery] now %.3f V; last %d changes:\n", histAt(0), n);
  for (int i = s_eventCount - n; i < s_eventCount; i++) {
    const Event& e = s_events[i % (int)(sizeof(s_events) / sizeof(s_events[0]))];
    Serial.printf("  at %us: %s, %.3f -> %.3f V (%c)\n", (unsigned)e.s,
                  e.charging ? "charging" : "on battery", e.before, e.after, e.why);
  }
}

}  // namespace battery
