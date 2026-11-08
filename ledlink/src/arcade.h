#pragma once
// The ARCADE looks (looks.h): the console and game effects of Pixelcade's LED
// firmware (pixelcade-rp2040, drivers/pixelcade/ledstrip_effects.cpp), ported
// so the jacket shows them the way a cabinet's LED strip does.
#include <stdint.h>

namespace arcade {

// The firmware's effect numbers, in the ARCADE looks' order (looks.h, from
// looks::ARCADE_FIRST): MAME and the consoles, then the games.
static const uint8_t EFFECT[] = {
    39,                                                                          // MAME
    46, 47, 48, 49, 50, 51, 52, 53,                                              // Nintendo
    54, 55, 56, 57, 58, 59, 60,                                                  // Sega
    61, 62, 63, 64, 65, 66,                                                      // PlayStation, Xbox
    67, 68, 69, 70, 71,                                                          // Atari
    72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82,                                  // the rest
    83, 84, 85, 86, 87, 88, 89,                                                  // handhelds
    100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,       // games
    114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135,
    192, 193, 194, 195, 196, 197, 198, 199,
};
static constexpr int COUNT = sizeof(EFFECT);
static constexpr int CONSOLE_COUNT = 45;  // MAME and the consoles; the rest are games

// Draws one frame of effect `effect` into strip[0..n): 0xRRGGBB per LED, as the
// firmware would on an n-LED strip `ms` milliseconds into the effect.
void draw(uint8_t effect, uint32_t ms, uint32_t* strip, uint32_t n);

}  // namespace arcade
