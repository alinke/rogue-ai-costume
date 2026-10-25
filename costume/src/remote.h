#pragma once
// The armband's operator console (ARM-7, ../../armband): commands sent over
// Bluetooth, so the wearer can run the costume from their forearm instead of
// reaching the chest screen's corner.
//
// The armband advertises (manufacturer data, like the scanners' beacon):
//   0xFFFF "RCT1" target, seq, command, key (2 bytes, little-endian)
// target 0 = any costume, else COSTUME_ID; a new seq = a new press (the same
// one repeats for a few seconds so a scan window can't miss it); key =
// ARM_LINK_KEY, so a stray advert can't drive the costume. The costume
// confirms by putting the seq it carried out at the end of its own "RAI1"
// beacon (byte 10), which the armband shows as a tick.
#include <Arduino.h>

#ifndef ARM_LINK_KEY
#define ARM_LINK_KEY 0x7A37
#endif
#ifndef COSTUME_ID
#if defined(BOARD_ELECROW_P4_10)
#define COSTUME_ID 2
#elif defined(BOARD_ELECROW_7)
#define COSTUME_ID 3
#else
#define COSTUME_ID 1  // the Guition (main costume)
#endif
#endif

namespace remote {

// Keep in step with ../../armband/src/radio.h.
enum Command : uint8_t {
  CMD_NONE, CMD_SCAN, CMD_KILL_SWITCH, CMD_QR, CMD_NEXT, CMD_COUNTDOWN, CMD_MIDNIGHT, CMD_BOOTH,
  CMD_GALLERY, CMD_PAUSE, CMD_RESUME, CMD_MUTE, CMD_UNMUTE, CMD_BRIGHT_DOWN, CMD_BRIGHT_UP,
};

void run(uint8_t cmd, uint32_t now);  // from loop()

}  // namespace remote
