// The console and game effects of Pixelcade's LED firmware
// (pixelcade-rp2040, drivers/pixelcade/ledstrip_effects.cpp), drawn on one
// strip exactly as there: same colours, timings, and sizes in LEDs. looks.cpp
// lays the strip along the jacket, hem -> collar -> shoulder -> cuff, the same
// on both sides.
//
// Differences from the firmware: everything is drawn from the time alone (the
// firmware moves Pac-Man, Donkey Kong, BurgerTime and Arkanoid a step per
// frame; here a "frame" is the firmware's default 40 ms), the effects run at
// full intensity (the jacket's brightness setting scales them), Ms. Pac-Man's
// dots and Arkanoid's bricks aren't capped at the firmware's 64 / 32 LEDs, and
// Arkanoid's bricks come back each time the ball returns to the paddle.
#include "arcade.h"

#include <Arduino.h>

#include <initializer_list>

namespace arcade {

static uint32_t* S;  // the strip being drawn
static uint32_t N;   // its length ("numPixels")
static uint32_t E;   // ms into the effect ("elapsed")

static constexpr uint32_t FRAME_MS = 40;  // the firmware's frame at its default speed (220)

static void set(uint32_t i, uint8_t r, uint8_t g, uint8_t b) {
  if (i < N) S[i] = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
}
static void set(uint32_t i, uint32_t c) {
  if (i < N) S[i] = c;
}
static uint8_t dim(uint8_t c, uint8_t f) { return c * f / 255; }
static uint32_t hsv(uint16_t h, uint8_t s, uint8_t v) {  // the firmware's hsvToRgb
  h %= 360;
  uint8_t region = h / 60, rem = (h % 60) * 255 / 60;
  uint8_t p = (v * (255 - s)) >> 8, q = (v * (255 - ((s * rem) >> 8))) >> 8, t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;
  uint8_t r, g, b;
  switch (region) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
  return (uint32_t)r << 16 | (uint32_t)g << 8 | b;
}
static uint32_t rnd() {  // fastRandom
  static uint32_t x = 2463534242u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}
static float progress() { return (E % 1000) / 1000.0f; }  // through the firmware's default 1 s cycle

// Blocks of colours scrolling along: block LEDs each, one LED every ms (most
// of the console effects).
static void chase(uint32_t block, uint32_t ms, std::initializer_list<uint32_t> cols) {
  uint32_t n = cols.size(), step = E / ms;
  for (uint32_t i = 0; i < N; i++) set(i, cols.begin()[((i + step) / block) % n]);
}
// Scattered glints: about one LED in `every`, changing every ms.
static void sparkles(uint32_t ms, uint32_t every, uint32_t c) {
  uint32_t frame = E / ms;
  for (uint32_t i = 0; i < N; i++)
    if (((i * 2654435761u) ^ (frame * 40503u)) % every == 0) set(i, c);
}
// A bright band at pos, fading over `width` LEDs (head(dist) gives its colour), on base.
template <typename F>
static void band(uint32_t pos, int width, F head, uint32_t base) {
  for (uint32_t i = 0; i < N; i++) {
    int dist = abs((int)i - (int)pos);
    set(i, dist < width ? head(dist) : base);
  }
}
static uint32_t grey(uint8_t v) { return (uint32_t)v << 16 | (uint32_t)v << 8 | v; }

// Pac-Man and Ms. Pac-Man: dots every 3 LEDs, eaten as she passes, 2 LEDs a frame.
static void pac(uint32_t dot, uint32_t body, uint32_t behind) {
  uint32_t cycle = (N + 3) / 2, pos = (E / FRAME_MS % cycle) * 2;
  for (uint32_t i = 2; i < N; i += 3)
    if (i > pos) set(i, dot);
  set(pos, body);
  if (pos > 0) set(pos - 1, behind);
}

void draw(uint8_t effect, uint32_t ms, uint32_t* strip, uint32_t n) {
  S = strip;
  N = n ? n : 1;
  E = ms;
  for (uint32_t i = 0; i < n; i++) S[i] = 0;
  const float pi = (float)M_PI;

  switch (effect) {
    // ---- Arcade / consoles -------------------------------------------------------
    case 39: {  // MAME: incandescent marquee bulbs chasing
      uint32_t phase = (E / 150) % 4;
      for (uint32_t i = 0; i < N; i++) {
        uint32_t d = (i + phase) % 4;
        set(i, d == 0 ? 0xFFDC78 : d == 1 || d == 3 ? 0x3C2D0A : 0x080600);
      }
      break;
    }
    case 46: chase(4, 100, {0xC80000, 0xB4B4B4}); break;  // NES
    case 47:  // SNES
      for (uint32_t i = 0; i < N; i++) set(i, sinf(progress() * 3 * pi + i * 0.4f) > 0 ? 0x7800B4 : 0xFFFF00);
      break;
    case 48: chase(3, 60, {0xFFDC00, 0x005AFF, 0x00C83C, 0xE6001E}); break;  // N64
    case 49:  // GameCube
      chase(4, 80, {0x8A2BE2, 0x660099, 0x46006E});
      sparkles(130, 45, 0xFFFFFF);
      break;
    case 50:  // Wii
      for (uint32_t i = 0; i < N; i++) {
        float w = (sinf(E / 300.0f + i * 0.35f) + 1) / 2;
        set(i, 20 + w * 100, 90 + w * 100, 220 + w * 35);
      }
      break;
    case 51: chase(4, 90, {0x00C8DC, 0x0A1E50}); break;   // Wii U
    case 52: chase(4, 100, {0xFFDC00, 0xDC0000}); break;  // Famicom Disk System
    case 53:  // Virtual Boy: a red scan over dim red
      band((E / 16) % N, 4, [](int d) { return (uint32_t)(255 - d * 50) << 16; }, 0x280000);
      sparkles(150, 60, 0xFFFFFF);
      break;
    case 54:  // SG-1000
      for (uint32_t i = 0; i < N; i++)
        if ((i + E / 200) % 3 == 0) set(i, 0xC80000);
      break;
    case 55: chase(4, 90, {0xDC0000, 0x008CFF}); break;  // Master System
    case 56:  // Genesis
      for (uint32_t i = 0; i < N; i++)
        if ((i + E / 100) % 4 < 2) set(i, 0x008CFF);
      break;
    case 57: {  // Sega CD: teal glinting round like a spinning disc
      float angle = (E / 700.0f) * 2 * pi;
      for (uint32_t i = 0; i < N; i++) {
        uint8_t b = 60 + (sinf(angle + i * 2 * pi / N) + 1) / 2 * 195;
        set(i, 0, b, dim(b, 210));
      }
      break;
    }
    case 58:  // 32X
      for (uint32_t i = 0; i < N; i++)
        if (i % 6 == (E / 200) % 6) set(i, 0xFF8C00);
      break;
    case 59: chase(3, 70, {0xE61E3C, 0xFF8C00, 0xFFDC00, 0x00C85A, 0x008CFF, 0xA03CDC}); break;  // Saturn
    case 60:  // Dreamcast swirl
      for (uint32_t i = 0; i < N; i++) {
        float s = sinf(progress() * 4 * pi + i * 0.6f);
        if (s > 0.2f) set(i, dim(255, s * 255), dim(102, s * 255), 0);
      }
      break;
    case 61: chase(3, 70, {0x00C85A, 0xE6003C, 0x006EFF, 0xE600C8}); break;  // PlayStation
    case 62: {  // PlayStation 2
      float wave = sinf(progress() * 3 * pi);
      for (uint32_t i = 0; i < N; i++) {
        float w = sinf(wave + i * 0.3f);
        if (w > 0.3f) set(i, 0, 0, dim(255, w * 255));
      }
      break;
    }
    case 63:  // PlayStation 3: the buttons with a glossy highlight running through
      chase(4, 50, {0x00C85A, 0xE6003C, 0x006EFF, 0xE600C8});
      set((E / 12) % N, 0xFFFFFF);
      break;
    case 64: {  // PSP: the XMB highlight gliding back and forth
      uint32_t len = N > 1 ? (N - 1) * 2 : 1, pos = (E / 10) % len;
      band(pos < N ? pos : len - pos, 5, [](int d) { return grey(255 - d * 45) | 0xFF; }, 0x00143C);
      break;
    }
    case 65: chase(4, 60, {0x147814, 0x28C828, 0x50FF50}); break;  // Xbox
    case 66: {  // Xbox 360: four soft green glows drifting round the ring of light
      uint32_t spacing = N / 4, head = (E / 120) % N;
      for (uint32_t i = 0; i < N; i++) {
        uint8_t b = 20;
        for (uint32_t q = 0; q < 4; q++) {
          int fwd = (int)((head + q * spacing) % N) - (int)i;
          if (fwd < 0) fwd += N;
          int dist = min<int>(fwd, N - fwd);
          if (dist < 10 && 200 - dist * 18 > b) b = 200 - dist * 18;
        }
        set(i, 0, b, 0);
      }
      break;
    }
    case 67:  // Atari 2600: woodgrain with orange
      for (uint32_t i = 0; i < N; i++) set(i, (i + E / 150) % 5 == 0 ? 0xFF8C00 : 0x8B4513);
      break;
    case 68: {  // Atari 5200: a keypad glow scanning, a different key colour each pass
      static const uint32_t KEYS[3] = {0xFFFFFF, 0xFFAA00, 0xE61E1E};
      uint32_t travel = E / 15, key = KEYS[(travel / N) % 3];
      band(travel % N, 3, [&](int d) {
        uint8_t b = 220 - d * 60;
        return (uint32_t)dim(key >> 16, b) << 16 | (uint32_t)dim(key >> 8 & 255, b) << 8 | dim(key & 255, b);
      }, 0x28282D);
      break;
    }
    case 69:  // Atari 7800
      for (uint32_t i = 0; i < N; i++)
        if (i % 3 == ((E / 500) % 2 == 0 ? 0u : 1u)) set(i, 0xC80000);
      break;
    case 70: {  // Jaguar spots
      uint32_t ph = E / 200;
      for (uint32_t i = 0; i < N; i++) {
        if ((i + ph) % 7 == 0 || (i + ph) % 11 == 0) set(i, 0xFF0000);
        else if ((i + ph) % 5 == 0) set(i, 0x640000);
      }
      break;
    }
    case 71:  // Lynx: a red comet over grey
      band((E / 14) % N, 5, [](int d) { return (uint32_t)(255 - d * 45) << 16; }, 0x5A5A5F);
      break;
    case 72: chase(4, 90, {0xFFFFFF, 0xFF8C00, 0xDC0000}); break;  // PC Engine
    case 73: {  // PC Engine CD: four spots spinning round
      uint32_t spin = progress() * N;
      for (uint32_t k = 0; k < 4; k++) set((spin + k * N / 4) % N, k % 2 == 0 ? 0xFF8C00 : 0xFFFFFF);
      break;
    }
    case 74:  // SuperGrafx
      for (uint32_t i = 0; i < N; i++) {
        float w = sinf(progress() * 2 * pi + i * 0.5f);
        if (w > 0.5f) set(i, 0xFFFFFF);
        else if (w > 0) set(i, 0xFF8C00);
        else if (w > -0.5f) set(i, 0xC80000);
      }
      break;
    case 75: chase(3, 55, {0xE6001E, 0xFFD200, 0x00C83C, 0x006EFF}); break;  // 3DO
    case 76: chase(4, 90, {0x0078C8, 0xFFFFFF}); break;                      // CD-i
    case 77:  // Vectrex: green vector lines
      for (uint32_t i = 0; i < N; i++) {
        float l = sinf(progress() * 4 * pi + i * 0.8f);
        if (l > 0.7f) set(i, 0x28FF5A);
        else if (l > 0.3f) set(i, 0x0F6423);
      }
      break;
    case 78: chase(3, 90, {0xDC0000, 0xFF7800, 0xFFDC00, 0x00AA3C, 0x006EDC}); break;  // ColecoVision
    case 79: chase(4, 90, {0x8B4513, 0xFFD700}); break;                                // Intellivision
    case 80:  // Amiga CD32
      for (uint32_t i = 0; i < N; i++) set(i, (i + E / 150) % 4 < 2 ? 0xFF0000 : 0xFFFFFF);
      break;
    case 81:  // Channel F
      for (uint32_t i = 0; i < N; i++)
        if ((i + E / 300) % 3 == 0) set(i, 0xFF8C00);
      break;
    case 82: chase(4, 90, {0xFF7800, 0xDC0000}); break;                    // Astrocade
    case 83: chase(3, 120, {0x0F380F, 0x306230, 0x8BAC0F, 0x9BBC0F}); break;  // Game Boy
    case 84:  // Game Boy Color: rainbow
      for (uint32_t i = 0; i < N; i++) set(i, hsv((uint16_t)(progress() * 360 + i * 20) % 360, 255, 255));
      break;
    case 85:  // Game Boy Advance: a silver sheen over indigo
      band((E / 12) % N, 3, [](int d) { return grey(220 - d * 40); }, 0x5A288C);
      break;
    case 86: chase(4, 100, {0xC8C8D2, 0x0078FF}); break;  // Nintendo DS
    case 87: chase(4, 100, {0x00D2C8, 0xFFFFFF}); break;  // Nintendo 3DS
    case 88:  // Game Gear
      for (uint32_t i = 0; i < N; i++)
        if ((i + E / 200) % 5 < 2) set(i, 0xC80000);
      break;
    case 89: chase(4, 90, {0xFFC800, 0x232326}); break;  // Neo Geo Pocket

    // ---- Games -------------------------------------------------------------------
    case 100: pac(0x646464, 0xFFFF00, 0xC8C800); break;  // Pac-Man
    case 101: {  // Donkey Kong: girders, a rolling barrel, Mario
      uint32_t f = E / FRAME_MS % ((N + 5) / 3), barrel = f * 3, mario = f;
      for (uint32_t i = 0; i + 1 < N; i += 8) {
        set(i, 0x960000);
        set(i + 1, 0x960000);
      }
      for (uint32_t k = 0; k < 3; k++) set(barrel + k, 0x8B4513);
      set(mario, 0xFF0000);
      set(mario + 1, 0x0000FF);
      break;
    }
    case 102: {  // Galaga: three layers of stars falling, sparkling
      static const uint32_t STAR[4] = {0xFFDC00, 0x00DCFF, 0xE61E28, 0x00DC5A};
      const uint32_t count[3] = {4, 6, 9}, speed[3] = {160, 100, 60};
      const uint8_t base[3] = {90, 160, 255};
      for (uint32_t layer = 0; layer < 3; layer++)
        for (uint32_t star = 0; star < count[layer]; star++) {
          uint32_t seed = layer * 977u + star * 6151u, c = STAR[seed % 4];
          uint8_t b = base[layer] - ((seed ^ 0x9e3779b9u) % 96) / 2;
          if ((E / 90 + seed) % 47 == 0) b = 255;
          set((seed % N + E / speed[layer]) % N, dim(c >> 16, b), dim(c >> 8 & 255, b), dim(c & 255, b));
        }
      sparkles(150, 60, 0xFFFFFF);
      break;
    }
    case 103: {  // Frogger: grass, road, river, grass; cars, a log, the frog hopping
      uint32_t start = N / 6, road = N / 2, river = 5 * N / 6;
      for (uint32_t i = 0; i < N; i++) set(i, i < start || i >= river ? 0x006400 : i < road ? 0x323232 : 0x003264);
      uint32_t car = start + (E / 150) % (road > start ? road - start : 1);
      set(car, 0xFF0000);
      if (car + 1 < road) set(car + 1, 0xFFFF00);
      uint32_t log = road + (E / 200) % (river > road ? river - road : 1);
      for (uint32_t k = 0; k < 4 && log + k < river; k++) set(log + k, 0x8B4513);
      uint32_t hop = (E / 220) % (N * 2), frog = min(hop < N ? hop : N * 2 - 1 - hop, N - 1);
      set(frog, 0x00FF3C);
      if (frog > 0) set(frog - 1, 0x00781E);
      break;
    }
    case 104: pac(0x966496, 0xFFFF00, 0xFF6496); break;  // Ms. Pac-Man (pink bow)
    case 105: {  // Dig Dug: dirt, Dig Dug, a Pooka, the pump
      for (uint32_t i = 0; i < N; i++) set(i, 0x3C1E0F);
      uint32_t dd = (E / 200) % N, pooka = (E / 300 + N / 2) % N;
      set(dd, 0xFFFFFF);
      set(dd + 1, 0x0096FF);
      set(pooka, 0xFF0000);
      set(pooka + 1, 0xFFFF00);
      if ((E / 100) % 8 == 0) set((dd + 2) % N, 0xFFFFFF);
      break;
    }
    case 106: {  // Q*bert: cubes turning yellow, Q*bert, Coily
      uint32_t done = (E / 300) % N;
      for (uint32_t i = 0; i < N; i++) set(i, i <= done ? 0xFFFF00 : 0xFF8C00);
      set((E / 250) % N, 0xFF6400);
      set((E / 350 + N / 3) % N, 0x800080);
      break;
    }
    case 107: {  // Pole Position: the track, a race car with a trail
      for (uint32_t i = 0; i < N; i += 4) {
        set(i, 0x646464);
        set(i + 1, 0xC8C8C8);
      }
      uint32_t car = (uint32_t)(E * (220 / 255.0f) / 100) % N;
      set(car, 0xFF0000);
      set(car + 1, 0xFFFFFF);
      for (uint32_t k = 1; k <= 3; k++)
        if (car >= k) set(car - k, 100 - k * 30, 0, 0);
      break;
    }
    case 108: {  // BurgerTime: the ingredients falling, Peter Pepper at the end
      static const uint32_t FOOD[4] = {0xFFA500, 0x00FF00, 0xFF0000, 0x8B4513};
      uint32_t f = E / FRAME_MS;
      for (uint32_t k = 0; k < 4; k++) set((k * (N / 4) + f) % (N + 5), FOOD[k]);
      set(N - 1, 0xFFFFFF);
      if (N > 1) set(N - 2, 0xFF0000);
      break;
    }
    case 109: {  // Joust: knights flapping, lava at the end
      uint32_t knight = (E / 150) % N + max(0, (int)(sinf((E / 100.0f) * pi / 180) * 3));
      set(knight, 0xFFFF00);
      set(knight + 1, 0xFF8C00);
      set((E / 200 + N / 2) % N + max(0, (int)(sinf((E / 120.0f) * pi / 180 + pi) * 3)), 0xFF0000);
      for (uint32_t i = N > 3 ? N - 3 : 0; i < N; i++) set(i, 0xFF6400);
      break;
    }
    case 110: {  // Robotron: the player in the middle, robots swarming, lasers
      uint32_t center = N / 2;
      set(center, 0x00FFFF);
      static const uint32_t ROBOT[3] = {0xFF0000, 0xFFFF00, 0xFF00FF};
      for (uint32_t r = 0; r < 5; r++) set((E / (100 + r * 20) + r * (N / 5)) % N, ROBOT[r % 3]);
      if ((E / 30) % 5 == 0)
        for (uint32_t s = 0; s < 3; s++) set((center + s * 5) % N, 0xFFFFFF);
      break;
    }
    case 111: {  // Zaxxon: fortress walls, the ship, a laser barrier
      for (uint32_t i = 0; i < N / 4; i++) set(i, 0x646496);
      for (uint32_t i = 3 * N / 4; i < N; i++) set(i, 0x646496);
      uint32_t half = N / 2 ? N / 2 : 1, ship = (E / 100) % half + N / 4;
      set(ship, 0xFFFFFF);
      set(ship + 1, 0x00C8FF);
      uint32_t barrier = (E / 200 + N / 3) % half + N / 4;
      if (barrier < 3 * N / 4) set(barrier, 0xFF0000);
      break;
    }
    case 112: {  // Tron: grid, a cyan and an orange light cycle with trails
      for (uint32_t i = 0; i < N; i += 8) {
        set(i, 0x003250);
        set(i + 1, 0x003250);
      }
      uint32_t c1 = (E / 100) % N, c2 = (E / 120 + N / 2) % N;
      set(c1, 0x00FFFF);
      set(c2, 0xFF6400);
      for (uint32_t k = 1; k <= 7; k++) {
        uint8_t v = 255 - k * 30;
        if (c1 >= k) set(c1 - k, 0, v, v);
        if (c2 >= k) set(c2 - k, v, v / 3, 0);
      }
      break;
    }
    case 113: {  // Time Pilot: sky, planes from each era, missiles
      for (uint32_t i = 0; i < N; i++) {
        uint8_t sky = 30 + i * 50 / N;
        set(i, 0, sky / 2, sky);
      }
      set(N / 2, 0xC8C8C8);
      set((E / 200) % N, 0x8B4513);
      set((E / 150 + N / 3) % N, 0x648000);
      set((E / 100 + 2 * N / 3) % N, 0x808080);
      if ((E / 80) % 6 == 0) set((E / 80) % N, 0xFFFFFF);
      break;
    }
    case 114: {  // 1942: ocean, the P-38, Zeros, flak
      for (uint32_t i = 0; i < N; i++) set(i, 0x000050);
      set(N - 2, 0x648000);
      set(N - 1, 0x96B400);
      uint32_t span = N > 5 ? N - 5 : 1;
      for (uint32_t k = 0; k < 3; k++) {
        uint32_t z = (E / 180 + k * (N / 4)) % span;
        set(z, 0xFF0000);
        if (z + 1 + 3 < N) set(z + 1, 0xFFFFFF);
      }
      if ((E / 60) % 8 == 0 && N > 10) {
        uint32_t aa = rnd() % (N - 10);
        for (uint32_t k = 0; k < 3; k++) set(aa + k, 0xFFFF00);
      }
      break;
    }
    case 115: {  // Paperboy: street, houses, the paperboy, papers flying
      for (uint32_t i = 0; i < N; i += 3) set(i, 0x505050);
      for (uint32_t i = 1; i < N; i += 6) {
        set(i, 0xC86432);
        set(i + 1, 0x6496C8);
      }
      uint32_t pb = (E / 150) % N;
      set(pb, 0xFF0000);
      set(pb + 1, 0x0000FF);
      if ((E / 200) % 5 == 0) set((pb + 2) % N, 0xFFFFFF);
      break;
    }
    case 116: {  // Ghosts 'n Goblins: graveyard, ghosts, candles, spectral pulses, red eyes, mist
      for (uint32_t i = 0; i < N; i++) {
        uint32_t v = (E / 100 + i * 7) % 3;
        set(i, v == 0 ? 0x1E0032 : v == 1 ? 0x14003C : 0x280028);
      }
      for (uint32_t gh = 0; gh < 3; gh++) {
        uint32_t t = (E + gh * 1000) % 3000, pos = t * N / 3000, fade = (t * 4) % 1000;
        uint8_t v = fade < 500 ? fade * 255 / 500 : 255 - (fade - 500) * 255 / 500;
        if (v > 50) {
          set(pos, v, v, 255);
          if (pos > 0) set(pos - 1, v / 3, v / 3, v);
          set(pos + 1, v / 3, v / 3, v);
        }
      }
      const uint32_t candles[5] = {3, N / 4, N / 2, 3 * N / 4, N - 4};
      for (uint32_t k = 0; k < 5; k++) {
        uint32_t t = (E + k * 200) % 800, pos = candles[k];
        uint8_t fl = 180 + (t % 100) * 75 / 100;
        if (t < 50) fl /= 2;
        set(pos, fl, fl / 2, 0);
        if (pos > 0) set(pos - 1, fl / 3, fl / 6, 0);
        set(pos + 1, fl / 3, fl / 6, 0);
      }
      uint32_t pt = E % 2500;
      if (pt < 300) {
        uint32_t c = N / 2, rad = pt * 12 / 300;
        for (uint32_t r = 0; r <= rad; r++) {
          uint8_t v = 200 - r * 150 / (rad + 1);
          if (c >= r) set(c - r, 0, v, 0);
          set(c + r, 0, v, 0);
        }
      }
      uint32_t et = E % 4000;
      if (et > 3500 && et < 3800 && N > 3) {
        uint32_t e1 = (E / 100) % (N - 3);
        set(e1, 0xFF0000);
        set(e1 + 2, 0xFF0000);
      }
      for (uint32_t i = 0; i < N; i += 7) {
        uint32_t m = (E / 300 + i) % 20;
        if (m < 3) set(i, grey(60 - m * 20));
      }
      break;
    }
    case 117: {  // Gauntlet: dungeon walls, the four heroes, a monster
      for (uint32_t i = 0; i < N; i += 5) set(i, 0x464646);
      uint32_t h = (E / 180) % N;
      set(h, 0xFF0000);
      set(h + 1, 0x0000FF);
      set(h + 2, 0xFFFF00);
      set(h + 3, 0x00FF00);
      set((E / 220 + N / 2) % N, 0x800080);
      break;
    }
    case 118: {  // Commando: jungle, the soldier, enemies, muzzle flashes
      for (uint32_t i = 0; i < N; i++) set(i, 0x003200);
      uint32_t s = (E / 150) % N;
      set(s, 0x648000);
      set(s + 1, 0x8B4513);
      for (uint32_t k = 0; k < 3; k++) set((E / 200 + k * (N / 3)) % N, 0xC80000);
      if ((E / 50) % 6 == 0) set((s + 2) % N, 0xFFFF00);
      break;
    }
    case 119: {  // Rampage: buildings, George, Lizzie and Ralph, fires
      for (uint32_t i = 0; i < N; i += 4) {
        set(i, 0x646464);
        set(i + 1, 0x8B6445);
      }
      uint32_t third = N / 3 ? N / 3 : 1;
      set((E / 200) % third, 0x8B4513);
      set(third + (E / 180) % third, 0x00C800);
      set(2 * third + (E / 220) % third, 0x969696);
      if ((E / 100) % 5 == 0) set(rnd() % N, 0xFF6400);
      break;
    }
    case 120: {  // Bubble Bobble: rainbow platforms, Bub and Bob, bubbles
      for (uint32_t i = 0; i < N; i += 6) set(i, hsv((i * 60) % 360, 255, 100));
      set((E / 180) % N, 0x00FF00);
      set((E / 200 + N / 2) % N, 0x0000FF);
      for (uint32_t k = 0; k < 3; k++) set((E / 120 + k * (N / 3)) % N, 0xC8C8FF);
      if ((E / 300) % 8 == 0) set((E / 300 * 2) % N, 0xFF00FF);
      break;
    }
    case 121: {  // Arkanoid: bricks, the paddle, the ball bouncing between them
      static const uint32_t BRICK[4] = {0xFF0000, 0xFFA500, 0xFFFF00, 0x00FF00};
      uint32_t top = N > 3 ? N - 3 : 1, period = 2 * top;
      uint32_t t = (E / FRAME_MS + N / 2 + top) % period;  // LEDs travelled since the ball left the paddle
      uint32_t ball = t <= top ? top - t : t - top;
      for (uint32_t i = 0; i < N / 3; i++)
        if (t < top - i) set(i, BRICK[i % 4]);  // not reached yet on this trip
      set(N - 2, 0xC0C0C0);
      set(N - 1, 0xC0C0C0);
      set(ball, 0xFFFFFF);
      break;
    }
    case 122: {  // Out Run: sunset, the Ferrari, road markers, palms
      for (uint32_t i = 0; i < N; i++) {
        float r = (float)i / N;
        set(i, 255 - r * 100, 150 - r * 50, r * 100);
      }
      set(N - 3, 0xFF0000);
      set(N - 2, 0xFF0000);
      set(N - 1, 0xC80000);
      uint32_t half = N / 2 ? N / 2 : 1, m = (E / 50) % half;
      set(m, 0xFFFFFF);
      set(m + N / 2, 0xFFFFFF);
      if ((E / 300) % 8 == 0) set((E / 300) % (N / 4 ? N / 4 : 1), 0x009600);
      break;
    }
    case 123: {  // Rygar: ruins, Rygar, his diskarmor, enemies
      for (uint32_t i = 0; i < N; i += 4) set(i, 0x646450);
      uint32_t r = (E / 180) % N;
      set(r, 0x8B4513);
      set(r + 1, 0xFFD700);
      if ((E / 80) % 2 == 0) set((r + 3) % N, 0xC0C0C0);
      set((E / 220 + N / 2) % N, 0x960000);
      break;
    }
    case 124: {  // Double Dragon: the street, Billy and Jimmy, the gang, punches
      for (uint32_t i = 0; i < N; i++) set(i, 0x3C3C3C);
      uint32_t half = N / 2 ? N / 2 : 1, billy = (E / 200) % half, jimmy = (E / 180 + 3) % half;
      set(billy, 0x0000FF);
      set(billy + 1, 0x6464FF);
      set(jimmy, 0xFF0000);
      set(jimmy + 1, 0xFF6464);
      set((E / 250 + N / 2) % N, 0xFFFF00);
      set((E / 250 + N / 2 + 5) % N, 0x00FF00);
      if ((E / 100) % 6 == 0) set((billy + 2) % N, 0xFFFFFF);
      break;
    }
    case 125: {  // R-Type: the R-9, the Force pod, Bydo, the wave cannon
      for (uint32_t i = 0; i < N; i++) set(i, 0x280028);
      uint32_t ship = N / 4, force = ship + 3;
      set(ship, 0x0096FF);
      set(ship + 1, 0xFFFFFF);
      set(force, 0xFF8C00);
      for (uint32_t k = 0; k < 3; k++) set((E / 150 + N / 2 + k * 8) % N, 0xC80000);
      if ((E / 60) % 4 == 0)
        for (uint32_t i = force + 1; i < N && i < force + 10; i++) set(i, 0x00FFFF);
      break;
    }
    case 126: {  // Shinobi: the temple, Joe Musashi, ninjas, shuriken
      for (uint32_t i = 0; i < N; i++) set(i, 0x14003C);
      uint32_t ninja = (E / 150) % N;
      set(ninja, 0xFFFFFF);
      set(ninja + 1, 0xC8C8C8);
      for (uint32_t k = 0; k < 2; k++) set((E / 200 + (k + 1) * (N / 3)) % N, 0xC80000);
      if ((E / 80) % 5 == 0) set((ninja + 3) % N, 0xC0C0C0);
      break;
    }
    case 127: {  // Contra: jungle and alien base, Bill and Lance, aliens, the spread gun
      for (uint32_t i = 0; i < N; i++) set(i, i % 2 == 0 ? 0x002800 : 0x280028);
      uint32_t half = N / 2 ? N / 2 : 1, bill = (E / 160) % half;
      set(bill, 0x0000FF);
      set((E / 180 + 2) % half, 0xFF0000);
      for (uint32_t k = 0; k < 3; k++) set((E / 140 + N / 2 + k * 6) % N, k % 2 == 0 ? 0x800080 : 0x009600);
      if ((E / 40) % 3 == 0)
        for (uint32_t k = 0; k < 3; k++) set((bill + 3 + k) % N, 0xFFFF00);
      break;
    }
    case 128: {  // Street Fighter: the dojo, Ryu, an opponent, a hadoken
      for (uint32_t i = 0; i < N; i++) set(i, 0x8B4513);
      uint32_t ryu = N / 3, opp = 2 * N / 3, who = (E / 2000) % 3;
      set(ryu, 0xFFFFFF);
      set(ryu + 1, 0xC8C8C8);
      static const uint32_t OPP[3][2] = {{0xFF0000, 0xC80000}, {0x0000FF, 0x6464FF}, {0x009600, 0x006400}};
      set(opp, OPP[who][0]);
      set(opp + 1, OPP[who][1]);
      if ((E / 200) % 8 == 0) set((ryu + 3 + (E / 200) * 2) % N, 0x0096FF);
      break;
    }
    case 129: {  // Operation Wolf: camouflage, soldiers, the crosshair, muzzle flashes
      for (uint32_t i = 0; i < N; i++) set(i, i % 3 == 0 ? 0x005000 : i % 3 == 1 ? 0x007800 : 0x3C5014);
      for (uint32_t k = 0; k < 4; k++) set((E / 180 + k * (N / 4)) % N, k % 2 == 0 ? 0x8B4513 : 0xA0A064);
      set((E / 100) % N, 0xFF0000);
      if ((E / 60) % 4 == 0) set(rnd() % N, 0xFFFF00);
      break;
    }
    case 130: {  // After Burner: sky, the F-14 and its afterburner, enemy jets, missiles
      for (uint32_t i = 0; i < N; i++) {
        uint8_t sky = 100 + i * 100 / N;
        set(i, 0, sky / 2, sky);
      }
      uint32_t jet = N / 2;
      set(jet, 0x969696);
      set(jet + 1, 0x0064C8);
      for (uint32_t k = 1; k <= 4; k++)
        if (jet >= k) set(jet - k, k <= 2 ? 0xFF6400 : 0xFF0000);
      for (uint32_t k = 0; k < 2; k++) set((E / 120 + k * (N / 2)) % N, 0xC80000);
      if ((E / 100) % 6 == 0) set((jet + 3 + (E / 100) * 2) % N, 0xFFFFFF);
      break;
    }
    case 131: {  // NARC: night, the officers, dealers, sirens at both ends
      for (uint32_t i = 0; i < N; i++) set(i, 0x14003C);
      uint32_t half = N / 2 ? N / 2 : 1, cop = (E / 180) % half;
      set(cop, 0x0000C8);
      set(cop + 1, 0x000096);
      for (uint32_t k = 0; k < 3; k++) set((E / 220 + N / 2 + k * 5) % N, k % 2 == 0 ? 0xC80000 : 0xC8C800);
      bool red = (E / 150) % 2 == 0;
      set(0, red ? 0xFF0000 : 0x0000FF);
      set(N - 1, red ? 0x0000FF : 0xFF0000);
      break;
    }
    case 132: {  // Splatterhouse: the house, Rick, monsters, splatter
      for (uint32_t i = 0; i < N; i++) set(i, 0x1E0000);
      uint32_t rick = (E / 200) % N;
      set(rick, 0xFFFFFF);
      set(rick + 1, 0x000096);
      for (uint32_t k = 0; k < 3; k++) set((E / 250 + (k + 1) * (N / 4)) % N, k % 2 == 0 ? 0x009600 : 0x800080);
      if ((E / 100) % 8 == 0)
        for (uint32_t k = 0; k < 3; k++) set((rick + 2 + k) % N, 0x960000);
      break;
    }
    case 133: {  // Golden Axe: the land, the three heroes, skeletons, magic
      for (uint32_t i = 0; i < N; i++) set(i, i % 2 == 0 ? 0x64461E : 0x005000);
      uint32_t h = (E / 180) % (N / 2 ? N / 2 : 1);
      set(h, 0x0064C8);
      set(h + 2, 0xC80000);
      set(h + 4, 0x8B4513);
      for (uint32_t k = 0; k < 2; k++) set((E / 240 + N / 2 + k * 8) % N, 0xC8C8C8);
      if ((E / 150) % 7 == 0) set((h + 6) % N, 0xFFD700);
      break;
    }
    case 134: {  // Final Fight: Metro City, Cody, Guy and Haggar, Mad Gear, hits
      for (uint32_t i = 0; i < N; i++) set(i, 0x464646);
      uint32_t h = (E / 160) % (N / 2 ? N / 2 : 1);
      set(h, 0x0000C8);
      set(h + 2, 0xFFFFFF);
      set(h + 4, 0x009600);
      for (uint32_t k = 0; k < 3; k++) set((E / 200 + N / 2 + k * 6) % N, k % 2 == 0 ? 0xC80000 : 0xC8C800);
      if ((E / 80) % 5 == 0) set((h + 6) % N, 0xFFFFFF);
      break;
    }
    case 135: {  // TMNT: the sewer, the four turtles, the Foot, Shredder, pizza
      for (uint32_t i = 0; i < N; i++) set(i, 0x283C14);
      uint32_t t = (E / 180) % (N / 2 ? N / 2 : 1);
      set(t, 0x0000FF);
      set(t + 1, 0x800080);
      set(t + 2, 0xFF8C00);
      set(t + 3, 0xFF0000);
      for (uint32_t k = 0; k < 3; k++) set((E / 220 + N / 2 + k * 7) % N, 0x320064);
      set((E / 300 + 3 * N / 4) % N, 0x969696);
      if ((E / 400) % 10 == 0) {
        uint32_t p = (E / 400) % N;
        set(p, 0xFFFF00);
        set(p + 1, 0xFF0000);
      }
      break;
    }
    case 192: {  // Gorf: the player's ship, the Gorfian fleet, laser fire
      set(N - 1, 0xFFFFFF);
      if (N > 1) set(N - 2, 0xC8C8FF);
      if (N <= 8) break;
      uint32_t w = (E / 300) % (N - 8);
      set(w, 0xFFFF00);
      if (w + 1 < N - 3) set(w + 1, 0xFFC800);
      for (uint32_t k = 0; k < 3; k++)
        if (w + 3 + k * 2 < N - 3) set(w + 3 + k * 2, 0xFF0000);
      if ((E / 80) % 7 == 0) {
        uint32_t laser = (N - 3) - (E / 80) % (N - 5);
        if (laser < N - 2) set(laser, 0x00FFFF);
      }
      break;
    }
    case 193: {  // Centipede: mushrooms, the centipede, the shooter
      for (uint32_t i = 2; i + 5 < N; i += 4) {
        set(i, 0x8B4513);
        if (i + 1 + 5 < N) set(i + 1, 0xD2B48C);
      }
      if (N > 8) {
        uint32_t head = (E / 120) % (N - 8);
        for (uint32_t s = 0; s < 6; s++)
          if (head + s + 3 < N) set(head + s, s == 0 ? 0xFFFF00 : 0x00FF00);
      }
      set(N - 1, 0xFFFFFF);
      if (N > 1) set(N - 2, 0x0096FF);
      break;
    }
    case 194: {  // Millipede: a flower garden, the millipede in rainbow segments, DDT
      static const uint32_t FLOWER[3] = {0xFF00FF, 0xFFFF00, 0xFF6400};
      static const uint32_t SEG[6] = {0xFF0000, 0xFF8000, 0xFFFF00, 0x00FF00, 0x0000FF, 0xFF00FF};
      for (uint32_t i = 1; i + 5 < N; i += 3) {
        set(i, 0x006400);
        if (i + 1 + 5 < N) set(i + 1, FLOWER[(i / 3) % 3]);
      }
      if (N > 10) {
        uint32_t head = (E / 100) % (N - 8);
        for (uint32_t s = 0; s < 8; s++)
          if (head + s + 3 < N) set(head + s, SEG[(s + E / 50) % 6]);
        if ((E / 150) % 6 == 0) {
          uint32_t spray = rnd() % (N - 10);
          for (uint32_t k = 0; k < 3; k++)
            if (spray + k + 3 < N) set(spray + k, 0xFFFFFF);
        }
      }
      break;
    }
    case 195: {  // Asteroids: rocks drifting, the ship, shots
      for (uint32_t r = 0; r < 4; r++) {
        uint32_t pos = (E / (150 + r * 30) + r * (N / 4)) % N;
        if (r < 2) {
          set(pos, 0x64503C);
          set(pos + 1, 0x786450);
        } else {
          set(pos, 0x50463C);
        }
      }
      uint32_t ship = N / 2 + (int)(sinf((E / 200.0f) * pi / 180) * 3);
      set(ship, 0xFFFFFF);
      if ((E / 60) % 8 == 0)
        for (uint32_t s = 0; s < 3; s++) set((ship + s * 5) % N, 0x00FFFF);
      break;
    }
    case 196: {  // Defender: mountains and humanoids, the Defender, a lander, lasers
      for (uint32_t i = N > 4 ? N - 4 : 0; i < N; i++) set(i, 0x503C28);
      for (uint32_t h = 0; h < 3; h++)
        if (N >= 3) set(N - 3 + h, 0xC8C8C8);
      if (N <= 8) break;
      uint32_t d = (E / 100) % (N - 8);
      set(d, 0x00FF00);
      if (d + 1 < N - 6) set(d + 1, 0x00C800);
      set((E / 180 + N / 2) % (N - 6), 0xFF0000);
      if ((E / 70) % 5 == 0) set((d + 2) % (N - 6), 0xFFFF00);
      break;
    }
    case 197: {  // Stargate: the warp tunnel, the portal, the ship with a rainbow trail
      for (uint32_t i = 0; i < N; i++) {
        uint8_t v = 50 + i * 150 / N;
        set(i, v / 3, v / 2, v);
      }
      for (uint32_t k = 0; k < 4; k++) set(N / 2 - 2 + k, 0x00FFFF);
      uint32_t ship = (E / 80) % N;
      set(ship, 0xFFFFFF);
      for (uint32_t k = 1; k <= 6; k++) {
        if (ship < k) break;
        uint8_t v = 150 - k * 20;
        uint32_t c;
        switch ((k + E / 20) % 6) {
          case 0: c = (uint32_t)v << 16; break;
          case 1: c = (uint32_t)v << 16 | (uint32_t)(v / 2) << 8; break;
          case 2: c = (uint32_t)v << 16 | (uint32_t)v << 8; break;
          case 3: c = (uint32_t)v << 8; break;
          case 4: c = v; break;
          default: c = (uint32_t)v << 16 | v; break;
        }
        set(ship - k, c);
      }
      break;
    }
    case 198: {  // Tempest: a neon tunnel, flippers and tankers climbing it, the claw, the superzapper
      uint32_t half = N > 1 ? N / 2 : 1;
      uint16_t hue = (E / 30) % 360;
      for (uint32_t i = 0; i < half; i++) {
        uint32_t c = hsv(hue, 255, 60 + i * 195 / half);
        set(i, c);
        set(N - 1 - i, c);
      }
      uint32_t flipper = (E / 12) % half, tanker = (E / 16 + half / 2) % half;
      set(half - 1 - flipper, 0xFF0000);
      set(half + flipper, 0xFF0000);
      set(half - 1 - tanker, 0x0064FF);
      set(half + tanker, 0x0064FF);
      set((E / 400) % 2 == 0 ? 0 : N - 1, 0xFFFF00);
      if (E % 4000 < 80) {
        set(0, 0xFFFFFF);
        set(N - 1, 0xFFFFFF);
      }
      break;
    }
    case 199: {  // Wizard of Wor: the dungeon, worriors, the Wizard, magic bolts
      for (uint32_t i = 0; i < N; i += 5) {
        set(i, 0x000096);
        set(i + 1, 0x0000C8);
      }
      if (N <= 8) break;
      uint32_t player = (E / 180) % (N - 8);
      set(player, 0xFFFF00);
      set((E / 220 + N / 3) % (N - 6), 0xFF0000);
      set((E / 200 + 2 * N / 3) % (N - 6), 0x00FFFF);
      if ((E / 300) % 20 < 10) set(N / 2, 0xFF00FF);
      if ((E / 90) % 6 == 0) set((player + 2) % (N - 6), 0xFFFFFF);
      break;
    }
    default: break;
  }
}

}  // namespace arcade
