# Arcade Jacket on the App Store

Everything App Store Connect asks for, ready to paste. Bundle ID
`com.alinke.arcadejacket` (Apple Watch app `com.alinke.arcadejacket.watchkitapp`),
version 1.0.0.

## 1. Before you submit

- **Privacy policy**: publish `../web/app-privacy.html` at
  **pixelcade.org/app-privacy** (WordPress, Custom HTML block) -- one policy for
  Pixelcade's device apps; it lists Arcade Jacket. The app's Settings › About
  links there. (pixelcade.org/privacy says Pixelcade's
  apps don't use the microphone or location, so it can't be this app's policy.)
- **Xcode**: the upload is the **JacketRemoteWatch** scheme (the iPhone app
  with the Apple Watch app inside; needs the watchOS platform, now installed).
  Archive it with Product › Archive, then Distribute App › App Store Connect.
- **HealthKit**: the watch app's App ID needs the HealthKit capability (Xcode
  turns it on with automatic signing).
- **Screenshots** (`AppStore/`, taken in demo mode in the simulator; deliberately
  none of the Arcade tab, which lists consoles by name):
  - `iphone-1206x2622/` -- the required "iPhone with Dynamic Island (medium
    display)" slot (6.1"/6.3"). `iphone-1320x2868/` is the same set at 6.9".
  - `watch/` -- 416 x 496 (Series 11 46mm): the Start screen and a session
    sending 128 BPM.
- **Demo video** for App Review: the jacket switching looks from the app, a
  turn signal and the brake, the Arcade tab, Settings › Number every LED.

## 2. App information

| Field | Value |
|---|---|
| Name | Arcade Jacket |
| Subtitle (30) | LED jacket remote and effects |
| Category | Lifestyle (secondary: Entertainment) |
| Age rating | 4+ (everything None) |
| Price | Free |
| Privacy policy URL | https://pixelcade.org/app-privacy/ |
| Support URL | https://github.com/alinke/rogue-ai-costume/tree/main/ios-jacket (or a pixelcade.org support page) |
| Copyright | 2026 Pixelcade |

**Promotional text (170)**
Light up your DIY LED jacket: arcade and console effects, holiday themes, bike turn signals with a brake light, a music visualizer and your heartbeat.

**Description**
Arcade Jacket is the remote for a DIY LED jacket: addressable LED strips sewn along the front and down the sleeves, driven by a small Gledopto ESP32 LED controller running the free, open-source LEDLINK firmware. It finds the controller over Bluetooth by itself, no pairing. No jacket yet? The app runs a live on-screen preview, so you can try every look first.

ARCADE
LED effects from Pixelcade's arcade marquees: one for each of 45 classic consoles and 44 arcade-style games, A-Z, with a search.

THEMES AND HOLIDAYS
Light Cycle, Cyberpunk, Code Rain, Synthwave, Fire, Aurora, Ocean and more, plus Christmas, New Year fireworks, Valentine's, St Patrick's, July 4th, Halloween, Thanksgiving and Pride. Solid, Breathe, Sparkle and Comet in any colour you pick.

BIKE
Left, right and hazard turn signals that sweep down that side like a car's, a hold-to-brake button, and an automatic brake light that turns the jacket red when GPS says you're slowing down. Wear it as well as proper bike lights, never instead of them.

LIVE
A music visualizer from the iPhone's microphone (nothing is recorded), and a heartbeat that pulses at your heart rate, from a Bluetooth heart-rate sensor or the Apple Watch app.

SETTINGS
Tell the controller how your strips are laid out -- zones, which parts are lit and which stay dark -- and check it with test patterns.

Requires: a Gledopto ESP32 LED controller with the LEDLINK firmware (free; web installer and build guide at alinke.github.io/rogue-ai-costume) and WS2811/WS2812B LED strips. A controller running WLED won't connect.

**Keywords (100)**
LED,jacket,wearable,lights,bike,turn signal,brake light,music visualizer,costume,neon,rave,retro

**What's New**
First release.

## 3. App Review notes

Arcade Jacket controls DIY hardware: an LED jacket driven by a Gledopto ESP32 controller with our open-source LEDLINK firmware. Without that hardware the app runs in demo mode -- the jacket drawn on screen runs the same effects code as the controller, so every control can be tried: pick a look (Arcade, Themes, Holidays), the turn signals and HOLD TO BRAKE (Bike tab), brightness and speed, and Settings (a sample LED layout). A video of the app with the real jacket is attached.

- Microphone: only for the Music and Music Beat looks (Live tab), to light the jacket in time with music in the room. Audio is analysed on the device and never recorded, stored or sent. The audio background mode keeps it listening with the screen locked while the wearer is at a concert.
- Location: only for the optional automatic brake light (Bike tab, off by default): the GPS speed tells when the rider slows down. "While Using the App" permission; location updates continue with the screen locked during a ride (the blue indicator shows). Location never leaves the device.
- Bluetooth: connects to the jacket's controller and, optionally, to a heart-rate sensor.
- HealthKit (Apple Watch app): reads the heart rate during a workout session the user starts, to pulse the jacket; the workout isn't saved, nothing is written to Health, and heart rate goes only to the jacket over Bluetooth.
- Nothing is collected or sent anywhere: no accounts, analytics or network calls.

## 4. App Privacy (nutrition label)

**Data Not Collected.** Microphone audio, location and heart rate are used on
the device and sent only to the user's own jacket over Bluetooth; nothing
reaches Pixelcade or anyone else.

## 5. Export compliance

`ITSAppUsesNonExemptEncryption = NO` is in the build (no encryption beyond the
system's).
