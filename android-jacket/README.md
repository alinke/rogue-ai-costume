# Arcade Jacket for Android

The Android version of the LED jacket's remote (`../ios-jacket` is the iPhone
app): the same tabs, looks, Bike and Live features, Settings with the LED layout
editor, and demo mode. It talks to LEDLINK (`../ledlink`) over Bluetooth with the
same protocol (`../ledlink/src/jacket_link.h`).

| | |
|---|---|
| `Looks.kt` | the looks table -- generated from `../ios-jacket/JacketRemote/Looks.swift` (same numbers, names and colours) |
| `JacketLink.kt` | Bluetooth to the jacket: commands, live data, status, the LED layout; a GATT queue (Android runs one operation at a time) |
| `HeartRate.kt` | a Bluetooth heart-rate sensor (Fitbit Charge 6 / Pixel Watch with sharing on, chest straps) |
| `MusicListener.kt` | the microphone, an FFT, bass / mids / treble and beats -- the same analysis as the iPhone app |
| `BikeBrake.kt` | the automatic brake light from GPS speed |
| `JacketService.kt` | the "connected to your jacket" notification: a foreground service (connected device, plus location / microphone when those are on) so it all keeps working with the screen off |
| `PreviewEngine.kt`, `src/main/cpp` | demo mode's preview: LEDLINK's own looks code, compiled with the NDK from the same `JacketPreview.cpp` the iPhone app uses |
| `ui/` | the main screen, Settings, layout help, "What you need" |

## Build

Android Studio, or from here (Gradle 8.11 needs JDK 21, not Studio's bundled 25):

```bash
export JAVA_HOME=~/Library/Java/JavaVirtualMachines/jbr-21.0.11/Contents/Home
./gradlew assembleDebug        # app/build/outputs/apk/debug/app-debug.apk
./gradlew bundleRelease        # unsigned; sign it in Studio for Play (StoreAssets/PLAY_LISTING.md)
```

Needs the NDK (27.0.11902837) for the preview. Android 8 (API 26) and up.

## Screenshots without a jacket

```bash
adb shell am start -n org.pixelcade.arcadejacket/.MainActivity --ez demo true \
  --ei look 2 [--ei turn 1] [--ez music true] [--ei bpm 72] [--es screen settings|need]
```

`demo true` keeps the app off Bluetooth, so it shows demo mode (force-stop the app
first if it's already connected). Play's listing set is in `StoreAssets/`.
