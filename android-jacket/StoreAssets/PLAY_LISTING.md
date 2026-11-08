# Google Play listing — Arcade Jacket

Package `org.pixelcade.arcadejacket`, version 1.0.0 (version code 2; code 1 was the first internal test). The same text
as the App Store listing (`../../ios-jacket/APP_STORE.md`), with the Apple Watch left
out (Android has none; Fitbit and Pixel Watch heart-rate sharing covers it).

## Before you publish

1. **Privacy policy**: re-paste `../../web/app-privacy.html` into
   pixelcade.org/app-privacy -- it now lists Android too.
2. **Signed bundle**: Android Studio › Build › Generate Signed App Bundle /
   APK › Android App Bundle, module `app`, with an upload key (create one and
   keep it in `android-jacket/key/` -- gitignored -- or reuse the Sidekick
   app's), **release**. The bundle lands in `app/release/app-release.aab`.
   Play App Signing re-signs it with Google's key.
3. **Create the app** in Play Console (All apps › Create app): name Arcade
   Jacket, App, Free. Upload the bundle to Internal testing first, then
   Production.

## App name (30 max)

Arcade Jacket

## Short description (80 max)

Light up your DIY LED jacket: arcade effects, bike signals, music and heartbeat.

(79 characters)

## Full description (4000 max)

Arcade Jacket is the remote for a DIY LED jacket: addressable LED strips sewn along the front and down the sleeves, driven by a small Gledopto ESP32 LED controller running the free, open-source LEDLINK firmware. It finds the controller over Bluetooth by itself, no pairing. No jacket yet? The app runs a live on-screen preview, so you can try every look first.

ARCADE
LED effects from Pixelcade's arcade marquees: one for each of 45 classic consoles and 44 arcade-style games, A-Z, with a search.

THEMES AND HOLIDAYS
Light Cycle, Cyberpunk, Code Rain, Synthwave, Fire, Aurora, Ocean and more, plus Christmas, New Year fireworks, Valentine's, St Patrick's, July 4th, Halloween, Thanksgiving and Pride. Solid, Breathe, Sparkle and Comet in any colour you pick.

BIKE
Left, right and hazard turn signals that sweep down that side like a car's, a hold-to-brake button, and an automatic brake light that turns the jacket red when GPS says you're slowing down. Wear it as well as proper bike lights, never instead of them.

LIVE
A music visualizer from the phone's microphone (nothing is recorded), and a heartbeat that pulses at your heart rate from a Bluetooth heart-rate sensor: a Fitbit Charge 6 or Pixel Watch with heart-rate sharing on, or a Polar, Wahoo or Garmin strap.

SETTINGS
Tell the controller how your strips are laid out -- zones, which parts are lit and which stay dark -- and check it with test patterns.

Requires: a Gledopto ESP32 LED controller with the LEDLINK firmware (free; web installer and build guide at alinke.github.io/rogue-ai-costume) and WS2811/WS2812B LED strips. A controller running WLED won't connect.

## Graphics in this folder

| Play Console slot | File(s) | Size |
|---|---|---|
| App icon | `icon-512.png` | 512×512 |
| Feature graphic | `feature-graphic-1024x500.png` | 1024×500 |
| Phone screenshots | `phone/01..05` | 1080×2160 (Play's max 2:1) |

Taken on a Pixel 6 in demo mode (`--ez demo true`, see `../README.md`), status
and navigation bars cropped off. Deliberately none of the Arcade tab: it lists
consoles by name. No tablet screenshots: the app is portrait phone-first;
Play shows the phone set.

## Other Play Console fields

- **Category**: Lifestyle. **Tags**: Wearables / Lighting, as offered.
- **Contact**: support@pixelcade.org; website https://pixelcade.org
- **Privacy policy URL**: https://pixelcade.org/app-privacy/
- **Ads**: no. **Price**: free. **Target audience**: 13+ (not designed for
  children).
- **Content rating** (IARC questionnaire): Utility / productivity-type app;
  no violence, no user interaction or sharing, no location sharing, no
  purchases.
- **Data safety**: no data collected, no data shared. Microphone audio,
  location and heart rate are processed on the device and sent only to the
  user's own LED controller over Bluetooth; nothing reaches Pixelcade or any
  server. "Is all of the user data encrypted in transit?" -- not applicable
  (no data leaves the device). No account, so no deletion request needed.
- **App access** (for review): no login. The app needs LED hardware;
  reviewers can use demo mode -- with no controller nearby, the jacket drawn on
  screen shows every look, turn signal and the brake.
- **Foreground service declaration** (Play asks for apps targeting Android 14+):
  - *Connected device*: keeps the Bluetooth link to the user's LED jacket while
    the screen is off (riding, at a concert).
  - *Location*: the optional automatic bike brake light, which uses GPS speed
    while riding with the phone in a pocket.
  - *Microphone*: the optional music looks, which light the jacket in time with
    music around the user with the screen off.
  Attach a short video: connect to the jacket, turn on the automatic brake light
  and a music look, lock the phone, show the notification and the jacket still
  reacting.
- **Health apps declaration**: if Play asks, the app reads heart rate only from
  a Bluetooth heart-rate sensor the user picks, to pulse the jacket; it is not a
  health app and stores nothing.
