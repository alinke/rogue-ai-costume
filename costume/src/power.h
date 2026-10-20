#pragma once
// Power off (operator console): screen, LEDs, camera and Bluetooth off, then
// a dark idle until a tap on the screen (or the BOOT button) restarts it.
// The board's IP5306 has no shutdown pin; on battery it may cut the power
// itself once the draw is this low -- plugging in USB starts it again.
void powerOff();
