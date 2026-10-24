# LAB-7 on the App Store

Everything App Store Connect asks for, ready to paste. Bundle id
`com.alinke.rogueai.lab7`, version 1.0.0.

## 1. Before you submit

- Publish the two pages on pixelcade.org (WordPress, Custom HTML block):
  - `web/lab7-privacy.html` at **pixelcade.org/lab7-privacy** (the app links
    there; contact support@pixelcade.org).
  - `web/lab7-join.local.html` (your filled-in copy of `web/lab7-join.html`)
    at **pixelcade.org/lab7-join**.
- Screenshots: 6.9"/6.7" iPhone, 1290 x 2796 (scanner, evidence wall, agent
  dossier, join). Taken in demo mode: silhouettes only, no real guests, no
  third-party logos.

## 2. App information

| Field | Value |
|---|---|
| Name | LAB-7: Rogue AI Hunt |
| Subtitle (30) | Track it. Contain it. Survive. |
| Category | Games › Family (secondary: Entertainment) |
| Age rating | 9+ (Infrequent/Mild Cartoon or Fantasy Violence: "containment beam"); everything else None. User-generated content: Yes. |
| Price | Free |
| Privacy policy URL | https://pixelcade.org/lab7-privacy/ |
| Support URL | https://pixelcade.org/lab7-help (public help page, no event link; `web/lab7-help.html`) |
| Copyright | 2026 Pixelcade |

**Promotional text (170)**
A rogue AI has escaped its sandbox and is crashing your party. Track its signal, fire the CONTAIN beam, and prove you're still human.

**Description**
A rogue AI has broken out of its sandbox. It has found a body. It's at the party.

LAB-7 turns your iPhone into a field scanner for a live, in-person party game. Someone at the party is wearing the ROGUE.AI costume, a hooded figure with a glowing chest terminal that broadcasts its signal over Bluetooth. Hunt it down.

• SCANNER: a signal meter climbs, and the phone buzzes like a Geiger counter, as you get closer to the entity.
• CONTAIN: when the signal is strong, fire the containment beam. The costume reacts on its own screen. (It always escapes. Keep trying.)
• HUMANITY TEST: photograph a guest and LAB-7 issues an official ID card: VERIFIED HUMAN… or COMPROMISED.
• AGENT DOSSIER: your double-agent identity and the override code for the entity's kill-switch keypad.
• EVIDENCE WALL: join your party's event to see every ID card and the TOP AGENTS leaderboard live.
• MIDNIGHT PROTOCOL: a countdown, and an alert when the entity's big moment arrives.

No account, no ads, no tracking. Demo mode lets you try the scanner without the costume.

**Keywords (100)**
halloween,party,game,ai,robot,scanner,costume,bluetooth,hunt,containment,id card,lab,agent,sci-fi

## 3. App privacy ("Data Types")

- **Photos or Videos**: collected, *not* linked to the user, not used for
  tracking. Purpose: App Functionality. (Only when the user joins an event.)
- **Device ID**: collected (a random per-install id, not the hardware id), not
  linked, not tracking. Purpose: App Functionality.
- **Gameplay Content** (the leaderboard name and scores): collected, not
  linked, not tracking. Purpose: App Functionality.
- Everything else: not collected. No third-party SDKs.

These match `LabScanner/PrivacyInfo.xcprivacy`.

## 4. App Review notes

> LAB-7 is the companion app for a live party game: one guest wears a costume with an
> ESP32 Bluetooth beacon (the "rogue AI"); the app tracks its signal and sends it a
> CONTAIN beacon. Without the costume, please turn on **Demo mode**: SCANNER tab › gear
> icon (top right) › "Demo mode (no costume nearby)". The signal then rises and falls on
> its own; press CONTAIN when it says SIGNAL STRONG. The EVIDENCE tab shows a sample
> wall in demo mode.
>
> **Joining an event** (the party's photo wall and leaderboard): open
> https://pixelcade.org/lab7-join on the device and tap JOIN THE EVENT, or paste the
> link from that page in AGENT › JOIN AN EVENT. The app explains what joining does and
> requires agreeing to the house rules before anything is uploaded.
>
> **User-generated content (Guideline 1.2):** joined users can upload humanity-test ID
> cards to the event's wall. House rules must be accepted first; every photo has REPORT
> (hidden for everyone at once, reviewed by the host) and HIDE PHONE (blocks all photos
> from that device); the host moderates and removes content from an admin page; photos
> without a detected face are held back automatically.
>
> Bluetooth: scanning for the costume's advertisement and advertising a service UUID
> (the beam). Camera: the humanity test only. No login.

Export compliance: **Uses encryption: Yes, exempt** (standard HTTPS only).
Adding `ITSAppUsesNonExemptEncryption = NO` to the build settings answers it
automatically (already set).

## 5. Build and upload

From the command line (bump CURRENT_PROJECT_VERSION first -- App Store Connect
already has builds up to 4 of 1.0.0; ExportOptions.plist keeps Xcode from
renumbering the build behind your back):

    xcodebuild -project LabScanner.xcodeproj -scheme LabScanner -configuration Release \
      -destination 'generic/platform=iOS' -archivePath /tmp/LabScanner.xcarchive -allowProvisioningUpdates archive
    xcodebuild -exportArchive -archivePath /tmp/LabScanner.xcarchive -exportOptionsPlist ExportOptions.plist \
      -exportPath /tmp/lab7-export -allowProvisioningUpdates

Or from Xcode:

1. Xcode › open `ios/LabScanner.xcodeproj`, scheme LabScanner, destination
   **Any iOS Device (arm64)**.
2. Product › Archive. In the Organizer: Distribute App › App Store Connect › Upload.
3. In App Store Connect › the app › the new build appears after processing
   (~10 min); add it to version 1.0.0, fill in the above, Submit for Review.

TestFlight first if you want: the same upload, then a public TestFlight link
(no email addresses needed).

## 6. After the party

`ROTATE_GUEST=1 ./deploy.sh` (in `cloud/`) changes the guest key: the join
link stops working and old installs can't upload any more, while the costume
and the scanners keep theirs. Clear the wall from the operator console or
`POST /clear`.
