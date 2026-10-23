# LAB-7 Monitor (iPad)

A stationary iPad at the party: the lab's containment monitor for one room.
UIKit, **iOS 12+** -- an original iPad Air (iOS 12.5.8) works.

| Screen | When |
|---|---|
| **Monitor** | Default. Clock, midnight countdown, the AI's status and threat level (from the costume's Bluetooth beacon), its signal strength in this room, containment attempts, the scoreboard and an evidence slideshow (from your photo service), a live checkpoint camera, a news crawl. |
| **Humanity checkpoint** ("ARE YOU AN AI?") | A guest steps up close to the screen -- or a group a step further back: full-screen camera, a box on every face, then "HOLD STILL -- PHOTO IN 3-2-1", a flash, and their **LAB-7 ID card** (the same card as the iPhone and Tab5 scanners), or for 2+ people a **group card** with every face tagged HUMAN -- or, now and then, one of them AI DETECTED. Behind the guests, the photo swaps the real room for a scene that follows the result -- an AI lands in the AI's world (the giant red eye, code rain, the master plan), humans in the lab (server vault, containment chamber) or under the eye -- with a neon rim of light round each person (iPadOS person cut-out, iPadOS 15+; the photo stays as it is if no one is found). The result screen shows a **QR code**: guests scan it with their phone to open their card on your photo service's `/p/<id>` page ("still uploading" until it's there), where SAVE TO PHOTOS opens the phone's share sheet (iPhone: Save Image). Tap the screen to move on, or it does after 25 s. The card goes to your photo service as LAB-7 evidence (`set=lab`, labelled `VERIFIED HUMAN #C017` / `COMPROMISED #C017`): it shows up on the Halloween page with the scanner's, sorted and counted on the scoreboard. A copy stays on the iPad (Documents/photos). |
| **Intrusion** | The costume's signal gets strong (it's in this room): the screen is "hacked" -- glitches, "ROGUE.AI DETECTED IN SECTOR 3 / I'M IN YOUR HOUSE NOW." |
| **Cleared** | It left: "ANOMALY CLEARED. IT'S STILL OUT THERE." A takeover lasts at most 25 s ("SIGNAL LOST -- THE AI LOST INTEREST... FOR NOW"), then can't happen again for 2 minutes unless the costume leaves first (out of range for 10 s): standing next to the iPad doesn't lock guests out of the checkpoint. `intrusionMaxSeconds` / `intrusionCooldownSeconds` in Secrets.plist. |
| **Beam** | Anyone fires CONTAIN at the costume (it reports being contained): "CONTAINMENT BEAM FIRED", then "CONTAINMENT FAILED". |
| **Midnight** | The costume's midnight takeover (or the iPad's clock at 00:00). |

Nothing to pair or change on the costume or scanners: the monitor only listens.

## Settings

`LabMonitor/Secrets.plist` (never committed -- copy `Secrets.example.plist`):
your photo service URL and upload key, the site shown on the cards, the room
name, how often guests come out COMPROMISED, and how strong the costume's
signal must be to count as "in this room" (`intrusionDbm`, default -62).
Without it the monitor runs with no evidence feed, and scans stay on the iPad.

At the party: **hold the screen for 3 seconds** to change the room name and the
"in this room" signal strength (it shows the costume's signal right now --
stand where the AI would be "in the room" and read it off).

## Set up the iPad

- Plugged in, Auto-Lock off (the app also keeps the screen on), volume up.
- Lock it into the app: Settings > General > Accessibility > Guided Access, then
  triple-click the Home button in the app.
- Camera at about face height; guests need to come within ~1 m to start a scan.

## Build

Xcode, `LabMonitor.xcodeproj`, your own team and bundle id. Current Xcode has no
developer disk image for iOS 12, so it can't run/debug on such an iPad: build
for "Any iOS Device", then install the .app with `ios-deploy --bundle` (or as an
.ipa with Apple Configurator). The iPad must be registered in your developer
account (Certificates, IDs & Profiles > Devices) for the signing profile.

Screenshots without hardware: run on an iPad simulator with the launch argument
`-demo calm` (or `checkpoint`, `countdown`, `card`, `intrusion`, `cleared`,
`beam`, `failed`, `midnight`).
