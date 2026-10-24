# LAB-7 scanner for iPhone

> **Guests can have it too.** Besides the researcher's scanner, the app is
> built for party guests (App Store: see [APP_STORE.md](APP_STORE.md)): three
> tabs, **SCANNER**, **EVIDENCE** (the party's live photo wall and TOP AGENTS
> leaderboard) and **AGENT** (a double-agent identity whose override code works
> on the costume's kill-switch keypad, the leaderboard name, the midnight
> countdown). Guests join the party's event with a link -- see "Events" below.

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

## Events (guests)

A guest joins the party's event with a link (a web page that opens the app, or
a QR code):

    lab7://join?service=<photo service>&key=<GUEST_KEY>&event=<name>&mid=2026-10-31T00:00

The app shows what joining does and the house rules; once the guest agrees, it
uploads their humanity-test cards to the wall, sends their score to the
leaderboard (`POST /stats`) and schedules a local notification 15 minutes
before and at `mid`. `GUEST_KEY` is not the costume's upload key: it is
rate-limited and can be changed after the party (`cloud/README.md`). Every
photo on the wall has REPORT (hidden for everyone, the host reviews it) and
HIDE PHONE (blocks that phone's photos on this phone only).

`web/lab7-join.html` is the join page (pixelcade.org/lab7-join);
`web/lab7-privacy.html` the privacy policy the app links to.

Your own scanner phones still use `lab7://configure?service=...&key=<UPLOAD_KEY>&site=...&name=...`:
applied at once, no questions.

**Demo mode** (gear › Demo mode): a simulated costume signal and a sample
evidence wall (silhouettes), for App Review and trying it at home.

**Geiger haptics** (gear): the phone buzzes faster as the AI gets closer.

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

Simulator checks (Debug builds): `-demoCard human|compromised` (the ID card),
`-tab N`, `-invite <join link>` (the join sheet), `-join <join link>`,
`-leave`, `-noPrompt` (skip the notification permission prompt).
