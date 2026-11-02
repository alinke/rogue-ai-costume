# JACKET -- the LED jacket's iPhone remote

Runs the jacket's LEDs (LEDLINK, `../ledlink`) when you wear it without the
costume: pick a look, ride with turn signals and an automatic brake light,
light it to the music around you, or pulse it with your heart rate. The
armband (`../armband`) has the same looks on its JACKET screen.

| Tab | |
|---|---|
| **Themes** | Rogue AI (the costume's show), Light Cycle, Cyberpunk, Code Rain, Synthwave, Rainbow, Fire, Aurora, Ocean, Scanner, and Solid / Breathe / Sparkle / Comet in any colour |
| **Holidays** | Christmas, Candy Cane, Winter, Hanukkah, New Year (fireworks), Valentine, St Patrick, Easter, July 4th (fireworks), Halloween, Thanksgiving, Pride |
| **Bike** | Left / Hazard / Right turn signals (an amber sweep down that side; they cancel after 30 s), hold-to-brake, an **automatic brake light** from GPS speed, and the Bike (white front, amber sleeves) and Day Flash looks |
| **Live** | **Music** (the sleeves are level meters, beats shoot up the front) and **Music Beat** (a new colour on every beat), from the iPhone's microphone; **Heartbeat**, coloured by heart-rate zone |

Brightness and speed are along the bottom; the power button switches the lights off.

## Heart rate

- **Bluetooth sensors** (Live › Find a heart-rate sensor): anything with the
  standard Heart Rate service -- a **Fitbit Charge 6** or **Pixel Watch 2-4**
  with heart-rate sharing turned on, Polar / Wahoo / Garmin chest straps and arm
  bands. Older Fitbits (Versa, Sense, Inspire) don't share it. The app
  reconnects to the last sensor by itself.
- **Apple Watch**: the watch app (target `JacketWatch`) runs a workout session
  for a live heart rate and sends it to the phone (not saved to Health). Open
  JACKET on the watch, tap Start.

## Build

Open `JacketRemote.xcodeproj` in Xcode, set your team, plug in the iPhone
(iOS 17+), and run:

- **JacketRemote** -- iPhone only. Use this without an Apple Watch: it doesn't
  need Xcode's watchOS platform.
- **JacketRemoteWatch** -- the same app with the Apple Watch app inside. Needs
  the watchOS platform (Xcode › Settings › Components) and HealthKit on your
  team. Same bundle ID, so install one or the other.

Background modes: Bluetooth (the jacket stays connected with the screen
locked), location (the automatic brake light while riding) and audio (the
music modes keep listening).

Screenshots without a jacket: launch with `-tab themes|holidays|bike|live` and
`-look N`.

## How it talks to the jacket

`../ledlink/src/jacket_link.h`: it connects to LEDLINK's GATT service
`524A4B31-0000-4C41-4237-000000000000`, writes commands (cmd a b c) and live
data (`M` bass mid treble level beat, ~45/s; `H` bpm), and gets LEDLINK's
status by notification. Look numbers: `../ledlink/src/looks.h` (mirrored in
`Looks.swift`).
