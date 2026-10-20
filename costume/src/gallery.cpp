#include "gallery.h"

#include <esp_heap_caps.h>

#include "config.h"

namespace gallery {

static uint8_t* s_store = nullptr;  // GALLERY_SIZE thumbnails
static int s_numbers[GALLERY_SIZE];
static const char* s_verdicts[GALLERY_SIZE];
static int s_count = 0, s_total = 0, s_head = 0;

bool begin() {
  s_store = (uint8_t*)heap_caps_malloc((size_t)GALLERY_SIZE * THUMB_W * THUMB_H, MALLOC_CAP_SPIRAM);
  return s_store != nullptr;
}

int add(const camera::Photo& p, const char* verdict) {
  if (!s_store || !p.pixels) return 0;
  uint8_t* dst = s_store + (size_t)s_head * THUMB_W * THUMB_H;
  for (int y = 0; y < THUMB_H; y++) {
    const uint8_t* a = p.pixels + (size_t)(y * 2) * p.w;
    const uint8_t* b = a + p.w;
    for (int x = 0; x < THUMB_W; x++) {
      dst[y * THUMB_W + x] = (a[2 * x] + a[2 * x + 1] + b[2 * x] + b[2 * x + 1] + 2) >> 2;
    }
  }
  s_numbers[s_head] = ++s_total;
  s_verdicts[s_head] = verdict;
  s_head = (s_head + 1) % GALLERY_SIZE;
  if (s_count < GALLERY_SIZE) s_count++;
  return s_total;
}

void clear() {
  s_count = s_total = s_head = 0;
}
int count() { return s_count; }
int total() { return s_total; }

Entry entry(int i) {
  // i = 0 is the oldest held entry.
  int slot = (s_head - s_count + i + GALLERY_SIZE) % GALLERY_SIZE;
  return {s_store + (size_t)slot * THUMB_W * THUMB_H, s_numbers[slot], s_verdicts[slot]};
}

}  // namespace gallery
