#pragma once
// Demo mode's preview: LEDLINK's own looks code, running in the app on a model
// jacket (a 78 + 77 LED front, two 113 LED sleeves), so what's shown is what
// the jacket would show. Swift calls these through the bridging header; the
// Android app compiles the same code (android-jacket/app/src/main/cpp).
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int jp_count(void);  // LEDs on the model
// Where LED i is: part 0 the front (t 0 hem .. 1 collar), 1 a sleeve (t 0
// shoulder .. 1 cuff); side -1 the wearer's left, +1 the right.
void jp_pixel(int i, int* part, int* side, float* t);
// The same commands and live data the jacket takes (ledlink/src/jacket_link.h).
void jp_command(uint8_t cmd, uint8_t a, uint8_t b, uint8_t c);
void jp_music(uint8_t bass, uint8_t mid, uint8_t treble, uint8_t level, uint8_t beat);
void jp_heart(uint8_t bpm);
// One frame, now: 0xRRGGBB per LED into out[jp_count()].
void jp_render(uint32_t* out);

#ifdef __cplusplus
}
#endif
