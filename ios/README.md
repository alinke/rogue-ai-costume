# LAB-7 scanner for iPhone

The researcher's scanner as an iPhone app: the same game as the ESP32
scanner (`../scanner/`) — a signal meter that climbs as you close in on the
costume, the **CONTAIN** beam (12 s recharge; the AI always escapes), and the
**humanity test** with the front camera, issuing LAB-7 ID cards (VERIFIED HUMAN
or COMPROMISED). Same beacons, so the costume can't tell it from the ESP32
scanner — and both can be used at once.

## Build and install

Xcode 26, iOS 17+, a real iPhone (the simulator has no Bluetooth or camera).

1. Open `LabScanner.xcodeproj`.
2. Target **LabScanner → Signing & Capabilities**: pick your team (and change the
   bundle id `com.alinke.rogueai.lab7` to one of yours).
3. Plug in the iPhone, select it, **Run**. Or share it with TestFlight.

No App Store needed for your own phone.

## Use

- **Keep the app open, screen on.** It keeps the screen awake itself. iOS only
  sends the CONTAIN beacon while the app is in front.
- **Tap the signal meter**: sound on / quiet (CONTAIN still sounds).
- **Humanity test camera**: the rear camera by default (point the phone at the
  guest); the flip button in the viewfinder switches to the front one (a
  mirrored selfie) and the app remembers the choice.
- **Gear** (top right): the name on the ID card, how often guests come out
  COMPROMISED, the distance thresholds (tune them at the party: read the dBm at
  "about 2 m" and "arm's length"), and the optional photo upload.
- Every card is also saved on the phone: **Files app › On My iPhone › LAB-7 › photos**.

## Photo upload (optional)

Gear › Photo upload: your photo service's URL and upload key (see
`../cloud/README.md`) and the site text printed on the card. Cards then go to
the landing page's LAB-7 survivor registry; without Wi-Fi/data they wait in a
queue and go later. Nothing is stored in the app's code — the settings live
on the phone.

## How CONTAIN reaches the costume

iOS doesn't let apps advertise manufacturer data (what the ESP32 scanner
uses), so the app advertises a service UUID carrying the same numbers:

    52534331-SSTT-4C41-4237-000000000000      SS = session (per launch), TT = CONTAIN counter

Measured: ~0.2-0.4 s from the press to the costume reacting. The costume
(`costume/src/researcher.cpp`) reads the newest UUID of each advertisement
(the BLE library appends them until the scan restarts -- reading the first one
made CONTAIN arrive 3-8 s late). It reads both forms and tracks iPhones by
session rather than Bluetooth address (iPhones change their address every
~15 minutes).

Simulator check of the ID card: launch with `-demoCard human` or `-demoCard compromised`.
