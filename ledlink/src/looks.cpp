// The jacket's own looks (looks.h), drawn through the costume's LED code
// (leds::setRenderer): this file only decides each LED's colour; leds.cpp
// still does the chain layout, brightness, current limit and output.
//
// Coordinates (leds::Pixel): d runs out from the chest -- the front 0 (hem)
// to 1 (collar), the sleeves 1 (shoulder) to 2 (cuff); side -1 is the
// wearer's left, +1 the right. T is "look time": seconds scaled by the speed
// setting, so every look speeds up and slows down together.
#include "looks.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

#include "arcade.h"
#include "gen/leds.h"
#include "jacket_link.h"

namespace looks {

using jacket::Turn;

// ---- Settings (saved) -----------------------------------------------------------------
static volatile uint8_t s_look = ROGUE_AI;
static volatile uint8_t s_bright = 60, s_speed = 100;
static volatile uint32_t s_color = 0x00DCFF;
static volatile bool s_on = true;
static volatile uint32_t s_dirtyAt = 0;  // a setting changed: saved a few seconds later

// ---- Live state -----------------------------------------------------------------------
static volatile uint8_t s_turn = jacket::TURN_OFF;
static volatile uint32_t s_turnSince = 0;
static volatile uint32_t s_brakeUntil = 0;
static volatile uint8_t s_bass = 0, s_mid = 0, s_treble = 0, s_level = 0;
static volatile uint32_t s_musicAt = 0, s_beatAt = 0;
static volatile uint8_t s_bpm = 0;
static volatile uint32_t s_heartAt = 0;
static volatile uint32_t s_lookSince = 0;

static constexpr uint32_t TURN_MAX_MS = 30000;   // a forgotten turn signal cancels itself
static constexpr uint32_t BRAKE_HOLD_MS = 1500;  // the app re-sends while it's slowing
static constexpr uint32_t LIVE_LOST_MS = 2000;   // music / heart rate stopped arriving

// ---- Per-frame values the renderer reads ------------------------------------------
static float T = 0;            // look time (speed-scaled)
static float R = 0;            // real seconds since boot
static float s_beat = 0;       // 0..1, flares on a beat and decays
static float s_bassF = 0, s_midF = 0, s_trebF = 0, s_levelF = 0;  // smoothed 0..1
static float s_peakL = 0, s_peakR = 0;
static int s_beatCount = 0;
static float s_heart = 0;      // seconds into the current heartbeat
static float s_heartPeriod = 1;
static bool s_turnOn = false;  // turn signal's blink phase
static float s_turnPhase = 0;  // 0..1 through one blink
static bool s_braking = false;

// Fireworks (NEW_YEAR, JULY_4TH): a few bursts at once, each a ring spreading
// along one side of the jacket.
struct Burst { float at, d; int8_t side; uint32_t color; };
static Burst s_bursts[4];
static float s_nextBurst = 0;

// ARCADE looks: the firmware effect is drawn once a frame on one strip running
// up the front and on down the sleeve (the front's LEDs, then the sleeve's),
// and each side shows it. The part lengths come from the pixels themselves.
static_assert(arcade::COUNT == LOOK_COUNT - ARCADE_FIRST, "looks.h and arcade.h list the same ARCADE looks");
static_assert(arcade::CONSOLE_COUNT == ARCADE_GAMES - ARCADE_FIRST, "looks.h and arcade.h split consoles / games alike");
static constexpr int STRIP_MAX = 512;
static uint32_t s_strip[STRIP_MAX];
static int s_trimN = 78, s_sleeveN = 113;

// ---- Colour helpers ---------------------------------------------------------------
static uint32_t rgb(float r, float g, float b) {
  auto c = [](float v) { return (uint32_t)(constrain(v, 0.0f, 1.0f) * 255); };
  return (c(r) << 16) | (c(g) << 8) | c(b);
}
static uint32_t scale(uint32_t c, float k) {
  k = constrain(k, 0.0f, 1.0f);
  return ((uint32_t)(((c >> 16) & 255) * k) << 16) | ((uint32_t)(((c >> 8) & 255) * k) << 8) | (uint32_t)((c & 255) * k);
}
static uint32_t mix(uint32_t a, uint32_t b, float f) {
  f = constrain(f, 0.0f, 1.0f);
  auto ch = [&](int sh) { return (uint32_t)(((a >> sh) & 255) * (1 - f) + ((b >> sh) & 255) * f) << sh; };
  return ch(16) | ch(8) | ch(0);
}
static uint32_t add(uint32_t a, uint32_t b) {
  auto ch = [&](int sh) { return (uint32_t)min<int>(255, ((a >> sh) & 255) + ((b >> sh) & 255)) << sh; };
  return ch(16) | ch(8) | ch(0);
}
// Hue 0..1 -> fully saturated colour.
static uint32_t hue(float h) {
  h = fmodf(h, 1.0f);
  if (h < 0) h += 1;
  float x = h * 6;
  int i = (int)x;
  float f = x - i;
  switch (i) {
    case 0: return rgb(1, f, 0);
    case 1: return rgb(1 - f, 1, 0);
    case 2: return rgb(0, 1, f);
    case 3: return rgb(0, 1 - f, 1);
    case 4: return rgb(f, 0, 1);
    default: return rgb(1, 0, 1 - f);
  }
}
// Colour from a palette, smoothly, at position x (wraps).
static uint32_t palette(const uint32_t* p, int n, float x) {
  x = fmodf(x, 1.0f);
  if (x < 0) x += 1;
  float f = x * n;
  int i = (int)f;
  return mix(p[i % n], p[(i + 1) % n], f - i);
}
// Repeatable noise: 0..1 for (LED, a time slot).
static float hash(int i, int k) {
  uint32_t h = (uint32_t)i * 374761393u + (uint32_t)k * 668265263u + 0x9E3779B9u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}
// A twinkle for LED i: 0 most of the time, a short flash now and then. rate: per second.
static float twinkle(int i, float t, float rate) {
  float slot = t * rate;
  int k = (int)floorf(slot);
  float f = slot - k;
  if (hash(i, k) > 0.12f) return 0;
  return powf(sinf(f * PI), 3);
}
static float pulse(float x, float width) {  // 1 at x = 0, falling to 0 at |x| = width
  return max(0.0f, 1.0f - fabsf(x) / width);
}
static float frac(float x) { return x - floorf(x); }

// Distance along the whole jacket on one side: 0 at the hem, 1 at the collar,
// then (on the sleeve) 1 at the shoulder to 2 at the cuff -- one continuous run
// from the hem up over the shoulder and down the arm.
static float runPos(const leds::Pixel& p) { return p.type == LEDS_SLEEVE ? p.d : p.u; }

// ---- The looks ----------------------------------------------------------------------
static uint32_t fireColor(float heat) {  // black -> red -> orange -> yellow
  heat = constrain(heat, 0.0f, 1.0f);
  if (heat < 0.4f) return rgb(heat / 0.4f, 0, 0);
  if (heat < 0.8f) return rgb(1, (heat - 0.4f) / 0.4f * 0.55f, 0);
  return rgb(1, 0.55f + (heat - 0.8f) / 0.2f * 0.35f, (heat - 0.8f) / 0.2f * 0.2f);
}

static uint32_t fireworks(const leds::Pixel& p, uint32_t base) {
  float x = runPos(p);
  uint32_t c = base;
  for (const Burst& b : s_bursts) {
    float age = R - b.at;
    if (b.at <= 0 || age < 0 || age > 1.6f) continue;
    if (p.side != b.side) continue;
    float rad = age * 0.55f;
    float ring = pulse(fabsf(x - b.d) - rad, 0.12f);
    float fade = 1 - age / 1.6f;
    float glitter = age > 0.5f ? (hash(p.index, (int)(R * 20)) < 0.5f ? 1.0f : 0.3f) : 1.0f;
    if (fabsf(x - b.d) < rad) ring = max(ring, 0.15f * fade);  // the inside glows a little
    c = add(c, scale(b.color, ring * fade * glitter));
  }
  return c;
}

static uint32_t render(const leds::Pixel& p) {
  const float x = runPos(p);  // 0 hem .. 1 collar/shoulder .. 2 cuff
  const int i = p.index;
  uint32_t c = 0;
  switch (s_look) {
    case LIGHT_CYCLE: {
      // A faint cyan grid line all along, light cycles racing out from the
      // chest with a bright head and a long trail, and an orange rival now
      // and then going the other way.
      c = rgb(0, 0.05f, 0.12f);
      float head = frac(T * 0.45f + (p.side > 0 ? 0 : 0.5f)) * 2.2f;
      float behind = head - x;
      if (behind >= 0 && behind < 0.9f) c = add(c, behind < 0.03f ? 0xC0FFFF : scale(0x00C8FF, powf(1 - behind / 0.9f, 2)));
      float rival = 2.2f - frac(T * 0.31f + 0.25f) * 2.4f;
      float ahead = x - rival;
      if (ahead >= 0 && ahead < 0.5f) c = add(c, ahead < 0.03f ? 0xFFD080 : scale(0xFF5000, powf(1 - ahead / 0.5f, 2)));
      break;
    }
    case CYBERPUNK: {
      static const uint32_t P[] = {0xFF00A0, 0x8000FF, 0x00E0FF, 0x8000FF};
      c = scale(palette(P, 4, x * 0.5f - T * 0.12f), 0.85f);
      int slot = (int)(T * 7);
      if (hash(i / 6, slot) < 0.06f) c = 0xFFE000;          // a glitch block
      if (hash(0, (int)(T * 1.3f)) < 0.1f && frac(T * 1.3f) < 0.12f && hash(i, slot) < 0.5f) c = 0xFFFFFF;  // the odd full glitch
      break;
    }
    case CODE_RAIN: {
      // Drops falling: down the sleeves (shoulder -> cuff) and down the front
      // (collar -> hem), each LED column with its own speed.
      float fall = p.type == LEDS_SLEEVE ? p.d - 1 : 1 - p.u;  // 0 at the top
      float col = hash(i / 8 + p.side * 97, 1);
      float head = frac(T * (0.35f + 0.4f * col) + col);
      float behind = head * 1.4f - fall;
      if (behind >= 0 && behind < 0.45f) c = behind < 0.025f ? 0xD0FFD0 : scale(0x00FF40, powf(1 - behind / 0.45f, 1.5f));
      c = add(c, scale(0x003000, 0.5f + 0.5f * hash(i, (int)(T * 3))));
      break;
    }
    case SYNTHWAVE: {
      static const uint32_t P[] = {0xFF2080, 0x9020FF, 0x2010A0, 0xFF6020, 0xFF2080};
      c = palette(P, 5, x * 0.35f - T * 0.08f);
      break;
    }
    case RAINBOW: c = hue(x * 0.5f - T * 0.25f); break;
    case FIRE: {
      // Flames climb from the cuffs (sleeves) and the hem (front): heat falls
      // off with height, flickering.
      float up = p.type == LEDS_SLEEVE ? 2 - p.d : p.u;  // 0 at the flame's base
      float n = 0.6f * hash(i, (int)(T * 14)) + 0.4f * hash(i / 4, (int)(T * 5));
      float heat = (1.15f - up * 1.1f) * (0.55f + 0.6f * n);
      c = fireColor(heat);
      break;
    }
    case AURORA: {
      static const uint32_t P[] = {0x00FF60, 0x00C0A0, 0x6020FF, 0x00FF90, 0x0060FF};
      float w = 0.5f + 0.5f * sinf(x * 4 + T * 0.7f) * sinf(x * 1.7f - T * 0.4f);
      c = scale(palette(P, 5, x * 0.3f + T * 0.05f), 0.15f + 0.85f * w * w);
      break;
    }
    case OCEAN: {
      float w = 0.5f + 0.5f * sinf(x * 4.5f - T * 1.8f);
      c = mix(0x001060, 0x0090FF, w);
      if (w > 0.97f && hash(i, (int)(T * 8)) < 0.3f) c = 0xC0F0FF;  // white caps
      break;
    }
    case SCANNER: {
      float pos = 1 + sinf(T * 2.0f);  // 0..2 along the run, both sides together
      float k = pulse(x - pos, 0.25f);
      c = scale(0xFF0000, powf(k, 1.5f));
      break;
    }
    case SOLID: c = s_color; break;
    case BREATHE: c = scale(s_color, 0.12f + 0.88f * powf(0.5f + 0.5f * sinf(T * 1.6f), 2)); break;
    case SPARKLE: c = add(scale(s_color, 0.12f), scale(0xFFFFFF, twinkle(i, T, 1.4f) * 0.8f)); c = mix(c, s_color, 0.2f); break;
    case COMET: {
      float head = frac(T * 0.6f) * 2.6f;
      float behind = head - x;
      c = behind >= 0 && behind < 0.7f ? scale(s_color, powf(1 - behind / 0.7f, 2.5f)) : 0;
      break;
    }
    case CHRISTMAS: {
      // Red and green blocks marching out, warm-white twinkles over them.
      bool red = ((int)floorf(x * 8 - T * 1.2f) & 1);
      c = scale(red ? 0xFF0000 : 0x00C000, 0.75f);
      float tw = twinkle(i, T, 1.0f);
      if (tw > 0) c = mix(c, 0xFFE0A0, tw);
      break;
    }
    case CANDY_CANE: {
      float s = frac(x * 6 + (p.side > 0 ? 0.25f : 0) - T * 0.6f);
      c = s < 0.5f ? 0xFF0000 : 0xFFFFFF;
      c = scale(c, s < 0.5f ? 1.0f : 0.7f);
      break;
    }
    case WINTER: {
      c = mix(0x0020A0, 0x40C0FF, 0.5f + 0.5f * sinf(x * 3 - T * 0.5f));
      c = scale(c, 0.6f);
      // Snow falling: down the sleeves and the front.
      float fall = p.type == LEDS_SLEEVE ? p.d - 1 : 1 - p.u;
      float col = hash(i / 10 + p.side * 31, 7);
      float flake = frac(T * (0.18f + 0.2f * col) + col) * 1.3f;
      if (fabsf(fall - flake) < 0.03f) c = 0xFFFFFF;
      break;
    }
    case HANUKKAH: {
      // Blue, with white "candles" along the front that flicker like flames.
      c = scale(0x0040FF, 0.6f + 0.2f * sinf(x * 5 - T));
      bool candle = p.type == LEDS_TRIM && fabsf(frac(p.u * 9 + 0.5f) - 0.5f) > 0.36f;  // 9 short bands
      if (candle) c = mix(0xFFFFFF, 0xFFC060, 0.3f * hash(i, (int)(T * 10)));
      if (p.type == LEDS_SLEEVE) c = add(c, scale(0xFFFFFF, twinkle(i, T, 0.7f) * 0.7f));
      break;
    }
    case NEW_YEAR: {
      c = scale(hash(i, 3) < 0.5f ? 0xFFB000 : 0xC0C0C0, 0.3f + 0.7f * twinkle(i, T, 3.0f));
      c = add(c, scale(0xFFFFFF, twinkle(i + 1000, T, 2.0f) * 0.7f));
      c = fireworks(p, c);
      break;
    }
    case VALENTINE: {
      // Lub-dub every 1 s (look time), rippling out from the chest; pink between.
      float beat = frac(T * 0.9f);
      float k = max(pulse(beat - 0.05f - x * 0.08f, 0.08f), 0.6f * pulse(beat - 0.25f - x * 0.08f, 0.08f));
      c = mix(scale(0xFF1060, 0.35f), 0xFF0000, k);
      if (k < 0.1f) c = add(c, scale(0xFF80C0, twinkle(i, T, 0.8f) * 0.6f));
      break;
    }
    case ST_PATRICK: {
      static const uint32_t P[] = {0x00FF00, 0x008020, 0x40FF40, 0x00A000};
      c = palette(P, 4, x * 0.6f - T * 0.15f);
      float tw = twinkle(i, T, 1.2f);
      if (tw > 0) c = mix(c, 0xFFC000, tw);
      break;
    }
    case EASTER: {
      static const uint32_t P[] = {0xFF80C0, 0xA0FFA0, 0x80C0FF, 0xFFFF80, 0xC090FF};
      c = scale(palette(P, 5, x * 0.4f - T * 0.1f), 0.8f);
      break;
    }
    case JULY_4TH: {
      // Stripes waving out like a flag, fireworks over them.
      float s = frac(x * 4.5f - T * 0.5f + 0.08f * sinf(x * 6 + T * 2));
      c = s < 0.33f ? 0xFF0000 : s < 0.66f ? 0xD0D0D0 : 0x0020FF;
      c = scale(c, 0.55f);
      c = fireworks(p, c);
      break;
    }
    case HALLOWEEN: {
      float n = hash(i / 3, (int)(T * 6));
      c = n < 0.7f ? scale(0xFF5000, 0.5f + 0.5f * n) : scale(0x8000FF, 0.8f);
      if (hash(0, (int)(T * 2)) < 0.05f && frac(T * 2) < 0.15f) c = 0xFFFFFF;  // lightning
      break;
    }
    case THANKSGIVING: {
      static const uint32_t P[] = {0xFF6000, 0xC02000, 0xFFA000, 0x802000, 0xFF8000};
      c = scale(palette(P, 5, x * 0.5f + T * 0.05f), 0.55f + 0.25f * hash(i / 5, (int)(T * 2)));
      // Leaves drifting down the sleeves and the front, tumbling (flickering).
      float fall = p.type == LEDS_SLEEVE ? p.d - 1 : 1 - p.u;
      float col = hash(i / 12 + p.side * 13, 5);
      float leaf = frac(T * (0.12f + 0.12f * col) + col) * 1.3f;
      if (fabsf(fall - leaf) < 0.05f) c = hash(i, (int)(T * 6)) < 0.5f ? 0xFFC000 : 0xFF2000;
      break;
    }
    case PRIDE: {
      static const uint32_t P[] = {0xFF0000, 0xFF6000, 0xFFE000, 0x00C000, 0x0040FF, 0x8000C0};
      float s = frac(x * 1.2f - T * 0.2f) * 6;
      c = scale(P[(int)s % 6], 0.85f);
      break;
    }
    case BIKE:
      // White forward (the front trim), amber side-marker running lights on the sleeves.
      if (p.type == LEDS_TRIM) c = 0xFFFFFF;
      else c = scale(0xFF8000, 0.45f + 0.55f * powf(pulse(frac(p.d * 1.5f - T * 0.8f) - 0.5f, 0.5f), 2));
      break;
    case BIKE_FLASH: {
      // Two quick flashes a second (daytime visibility), white front, amber sleeves.
      float f = frac(R * 1.0f);
      bool on = f < 0.08f || (f > 0.16f && f < 0.24f);
      c = on ? (p.type == LEDS_TRIM ? 0xFFFFFF : 0xFFA000) : (p.type == LEDS_TRIM ? 0x303030 : 0x301800);
      break;
    }
    case MUSIC: {
      if (millis() - s_musicAt > LIVE_LOST_MS) {  // no music arriving: a slow violet "listening" breath
        c = scale(0x8000FF, 0.1f + 0.25f * (0.5f + 0.5f * sinf(R * 2)));
        break;
      }
      if (p.type == LEDS_SLEEVE) {
        // Level meters, shoulder -> cuff: the left sleeve the bass, the right the mids.
        float lvl = p.side < 0 ? s_bassF : s_midF, peak = p.side < 0 ? s_peakL : s_peakR;
        float y = p.d - 1;  // 0 at the shoulder
        if (y < lvl) c = y < 0.55f ? mix(0x00FF40, 0xFFE000, y / 0.55f) : mix(0xFFE000, 0xFF0000, (y - 0.55f) / 0.45f);
        if (fabsf(y - peak) < 0.025f) c = 0xFFFFFF;
      } else {
        // The front: each beat shoots a pulse up from the hem in a new colour,
        // treble sparkles over a glow that follows the overall level.
        float h = s_beatCount * 0.13f;
        c = scale(hue(h + p.u * 0.2f), 0.1f + 0.3f * s_levelF);
        float since = (millis() - s_beatAt) / 1000.0f;
        c = add(c, scale(hue(h), pulse(p.u - since * 2.2f, 0.18f) * (1 - since)));
        if (hash(i, (int)(R * 25)) < s_trebF * 0.35f) c = add(c, 0xA0A0A0);
      }
      break;
    }
    case MUSIC_BEAT: {
      if (millis() - s_musicAt > LIVE_LOST_MS) {
        c = scale(0x8000FF, 0.1f + 0.25f * (0.5f + 0.5f * sinf(R * 2)));
        break;
      }
      uint32_t col = hue(s_beatCount * 0.17f + x * 0.1f);
      c = scale(col, 0.12f + 0.25f * s_bassF + 0.75f * s_beat);
      break;
    }
    case HEARTBEAT: {
      if (millis() - s_heartAt > LIVE_LOST_MS * 3) {  // no heart rate: a faint red breath
        c = scale(0xFF0000, 0.05f + 0.15f * (0.5f + 0.5f * sinf(R * 1.5f)));
        break;
      }
      // Colour by zone; each beat (lub-dub) ripples out from the chest.
      int b = s_bpm;
      uint32_t zone = b < 100 ? 0x00A0FF : b < 125 ? 0x00FF40 : b < 145 ? 0xFFE000 : b < 165 ? 0xFF7000 : 0xFF0000;
      float t = s_heart - x * 0.12f;
      float k = max(pulse(t - 0.06f, 0.09f), 0.6f * pulse(t - 0.28f, 0.09f));
      c = scale(zone, 0.12f + 0.88f * k);
      break;
    }
    default:
      if (s_look >= ARCADE_FIRST) {
        // One LED of the strip per LED of the jacket: the front from the hem,
        // then the sleeve from the shoulder.
        int k = (int)lroundf((p.type == LEDS_SLEEVE ? p.d - 1 : p.u) * (p.count - 1));
        if (p.type == LEDS_SLEEVE) {
          s_sleeveN = p.count;
          k += s_trimN;
        } else {
          s_trimN = p.count;
        }
        c = s_strip[constrain(k, 0, STRIP_MAX - 1)];
      }
      break;
  }

  // ---- Overlays: the brake light and the turn signals win over any look ----------
  if (s_braking) return 0xFF0000;
  if (s_turn != jacket::TURN_OFF) {
    bool mine = s_turn == jacket::TURN_HAZARD || (s_turn == jacket::TURN_LEFT ? p.side < 0 : p.side > 0);
    if (mine) {
      // A sequential sweep out to the end (the front up to the collar, the
      // sleeve down to the cuff), then dark: like a car's.
      float reach = p.type == LEDS_SLEEVE ? p.d - 1 : p.u;
      return s_turnOn && reach <= s_turnPhase / 0.4f ? 0xFF8000 : 0;  // the sweep takes 0.4 s, holds 0.1 s
    }
    return scale(c, 0.2f);  // the other side stays on, dimmed, so the signal stands out
  }
  return c;
}

// ---- Settings: save / restore ---------------------------------------------------------
static void save() {
  Preferences pr;
  pr.begin("jacket", false);
  pr.putUChar("look", s_look);
  pr.putUChar("bright", s_bright);
  pr.putUChar("speed", s_speed);
  pr.putUInt("color", s_color);
  pr.putBool("on", s_on);
  pr.end();
  Serial.printf("[looks] saved: %s, %u%%, speed %u%%\n", INFO[s_look].name, (unsigned)s_bright, (unsigned)s_speed);
}

void begin() {
  Preferences pr;
  pr.begin("jacket", true);
  s_look = min<int>(pr.getUChar("look", ROGUE_AI), LOOK_COUNT - 1);
  s_bright = constrain(pr.getUChar("bright", 60), 10, 100);
  s_speed = constrain(pr.getUChar("speed", 100), 10, 250);
  s_color = pr.getUInt("color", 0x00DCFF);
  s_on = pr.getBool("on", true);
  pr.end();
  s_lookSince = millis();
  Serial.printf("[looks] %s, %u%%, speed %u%%\n", INFO[s_look].name, (unsigned)s_bright, (unsigned)s_speed);
}

static void changed() { s_dirtyAt = millis() | 1; }

void command(uint8_t cmd, uint8_t a, uint8_t b, uint8_t c) {
  uint32_t now = millis();
  switch (cmd) {
    case jacket::CMD_LOOK:
      if (a < LOOK_COUNT) { s_look = a; s_lookSince = now; s_on = true; changed(); }
      break;
    case jacket::CMD_NEXT: {
      int n = (s_look + (a == 255 ? LOOK_COUNT - 1 : 1)) % LOOK_COUNT;
      s_look = n;
      s_lookSince = now;
      s_on = true;
      changed();
      break;
    }
    case jacket::CMD_BRIGHT: s_bright = constrain(a, 10, 100); changed(); break;
    case jacket::CMD_SPEED: s_speed = constrain(a, 10, 250); changed(); break;
    case jacket::CMD_COLOR: s_color = ((uint32_t)a << 16) | ((uint32_t)b << 8) | c; changed(); break;
    case jacket::CMD_POWER: s_on = a != 0; changed(); break;
    case jacket::CMD_TURN:
      s_turn = a <= jacket::TURN_HAZARD ? a : 0;
      s_turnSince = now;
      break;
    case jacket::CMD_BRAKE: s_brakeUntil = a ? now + BRAKE_HOLD_MS : 0; break;
    default: return;
  }
  Serial.printf("[looks] command %u (%u %u %u)\n", (unsigned)cmd, (unsigned)a, (unsigned)b, (unsigned)c);
}

void music(uint8_t bass, uint8_t mid, uint8_t treble, uint8_t level, bool beat) {
  s_bass = bass;
  s_mid = mid;
  s_treble = treble;
  s_level = level;
  uint32_t now = millis();
  s_musicAt = now;
  if (beat && now - s_beatAt > 150) {
    s_beatAt = now;
    s_beatCount++;
    s_beat = 1;
  }
}

void heartRate(uint8_t bpm) {
  if (bpm < 30) return;
  s_bpm = bpm;
  s_heartAt = millis();
}

bool following() { return s_look == ROGUE_AI; }
uint8_t look() { return s_look; }

void status(uint8_t o[10], uint8_t ackSeq, bool app, bool costume) {
  uint32_t now = millis();
  o[0] = s_look;
  o[1] = s_bright;
  o[2] = s_speed;
  o[3] = (s_on ? jacket::F_ON : 0) | (costume ? jacket::F_COSTUME : 0) |
         (now - s_musicAt < LIVE_LOST_MS ? jacket::F_MUSIC : 0) | (now - s_heartAt < LIVE_LOST_MS * 3 ? jacket::F_HEART : 0) |
         (s_braking ? jacket::F_BRAKE : 0) | (app ? jacket::F_APP : 0);
  o[4] = ackSeq;
  o[5] = s_color >> 16;
  o[6] = s_color >> 8;
  o[7] = s_color;
  o[8] = s_turn;
  o[9] = now - s_heartAt < LIVE_LOST_MS * 3 ? s_bpm : 0;
}

static uint32_t renderOff(const leds::Pixel&) { return 0; }

void tick(uint32_t now) {
  static uint32_t last = now;
  float dt = min(0.2f, (now - last) / 1000.0f);
  last = now;
  T += dt * s_speed / 100.0f;
  R = now / 1000.0f;

  if (s_dirtyAt && now - s_dirtyAt > 3000) {
    s_dirtyAt = 0;
    save();
  }
  if (s_turn != jacket::TURN_OFF && now - s_turnSince > TURN_MAX_MS) s_turn = jacket::TURN_OFF;
  s_braking = s_brakeUntil && (int32_t)(now - s_brakeUntil) < 0;
  // ~85 blinks a minute, each a 0.5 s sweep then dark.
  s_turnPhase = fmodf((now - s_turnSince) / 1000.0f, 0.7f);
  s_turnOn = s_turnPhase < 0.5f;

  // Music: smooth the levels so the meters move fluidly; peaks fall slowly.
  auto ease = [&](float& v, uint8_t target, float up, float down) {
    float t = target / 255.0f;
    v += (t - v) * (t > v ? up : down);
  };
  ease(s_bassF, s_bass, 0.6f, 0.15f);
  ease(s_midF, s_mid, 0.6f, 0.15f);
  ease(s_trebF, s_treble, 0.7f, 0.25f);
  ease(s_levelF, s_level, 0.5f, 0.1f);
  s_peakL = max(s_peakL - dt * 0.35f, s_bassF);
  s_peakR = max(s_peakR - dt * 0.35f, s_midF);
  s_beat *= expf(-dt * 6);

  // Heartbeat: the phase runs at the measured rate.
  if (s_bpm) {
    s_heartPeriod = 60.0f / s_bpm;
    s_heart += dt;
    if (s_heart >= s_heartPeriod) s_heart = fmodf(s_heart, s_heartPeriod);
  }

  // Fireworks: a new burst every 0.6-1.8 s (look time), on a random side.
  if ((s_look == NEW_YEAR || s_look == JULY_4TH) && T >= s_nextBurst) {
    s_nextBurst = T + 0.6f + 1.2f * hash(1, (int)(R * 10));
    int oldest = 0;
    for (int k = 1; k < 4; k++)
      if (s_bursts[k].at < s_bursts[oldest].at) oldest = k;
    static const uint32_t NY[] = {0xFFC000, 0xFFFFFF, 0xFF40C0, 0x40C0FF};
    static const uint32_t US[] = {0xFF0000, 0xFFFFFF, 0x0040FF, 0xFF0000};
    s_bursts[oldest] = Burst{R, 0.3f + 1.4f * hash(2, (int)(R * 10)), (int8_t)(hash(3, (int)(R * 10)) < 0.5f ? -1 : 1),
                             (s_look == NEW_YEAR ? NY : US)[(int)(hash(4, (int)(R * 10)) * 4) & 3]};
  }

  if (s_look >= ARCADE_FIRST)
    arcade::draw(arcade::EFFECT[s_look - ARCADE_FIRST], (uint32_t)(T * 1000), s_strip, min(s_trimN + s_sleeveN, STRIP_MAX));

  // Who draws: the costume's own show (ROGUE AI, unless a turn/brake is on),
  // our looks, or nothing (switched dark).
  bool overlay = s_braking || s_turn != jacket::TURN_OFF;
  if (!s_on && !overlay) {
    leds::setRenderer(renderOff);
  } else if (s_look == ROGUE_AI && !overlay) {
    leds::setRenderer(nullptr);
  } else {
    leds::setRenderer(render);
  }
  // Brightness: the jacket's own setting -- the costume's beacon sets it for
  // ROGUE AI (main.cpp). The brake and turn signals go bright whatever it is.
  if (s_look != ROGUE_AI || overlay) leds::setBrightness(overlay ? max<int>(s_bright, 85) : s_bright);
}

}  // namespace looks
