// The monitor's brain: which screen is up, and when it changes.
//
//   calm        the lab's monitor (default)
//   checkpoint  a guest steps up close: scan, countdown, photo
//   card        their LAB-7 ID card (also uploaded to the Halloween page)
//   intrusion   the costume's signal gets strong: the AI is in this room
//   cleared     ...and it left
//   beam        the costume says it's being contained (CONTAIN fired)
//   beamFailed  ...it escaped, as always
//   midnight    the costume's midnight takeover (or the clock at 00:00)
//
// Hold the header for 3 s for settings (room name, intrusion sensitivity).
import AVFoundation
import UIKit

final class MonitorViewController: UIViewController {
    private let radio = Radio()
    private let feed = Feed()
    private let camera = Camera()
    private let screen = MonitorView()
    private var link: CADisplayLink?

    private var mode = Mode.calm
    private var modeStart = CACurrentMediaTime()
    private var lastTick = CACurrentMediaTime()

    // Checkpoint.
    private var faceHold = 0.0
    private var lastFaceAt = 0.0
    private var lastFace: CGRect?
    private var cooldownUntil = 0.0
    private var waitFaceGone = false
    private var subject = ""
    private var findings: [Finding] = []
    private var captured = false
    private var card: IDCard.Result?
    private var lastCount = 0
    private var qr: UIImage?
    private var groupCount = 0
    private var dismissCard = false
    private var capturedAt = 0.0

    // The costume.
    private var closeSince: Double?
    private var intrusionArmedAt = 0.0
    private var clearedByCap = false
    private var farSince: Double?
    private var prevActivity = -1
    private var nextGlitchSound = 0.0

    // Evidence wall: the slide shown, when it changed, and the roll call of AIs.
    private var wallIndex = 0
    private var wallAt = 0.0
    private var wallSlides = 0
    private var wallRollCall = false
    private var wallShown: EvidencePhoto?
    private var wallAlerted: URL?

    // Slideshow + toast.
    private var slide = 0
    private var slideAt = 0.0
    private var toast: String?
    private var toastUntil = 0.0

    // Demo: "-demo calm|checkpoint|countdown|card|intrusion|cleared|beam|failed|midnight" pins a screen
    // with made-up data (the simulator has no camera or Bluetooth).
    private let demo = UserDefaults.standard.string(forKey: "demo")
    private lazy var stand: UIImage? = demo == nil ? nil
        : MonitorViewController.standIn(people: (demo ?? "").hasPrefix("group") ? 3 : 1)

    override var prefersStatusBarHidden: Bool { true }
    override var prefersHomeIndicatorAutoHidden: Bool { true }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black
        view.layer.addSublayer(camera.preview)
        screen.frame = view.bounds
        screen.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        view.addSubview(screen)
        let hold = UILongPressGestureRecognizer(target: self, action: #selector(settings(_:)))
        hold.minimumPressDuration = 3
        screen.addGestureRecognizer(hold)
        let tap = UITapGestureRecognizer(target: self, action: #selector(tapped))
        screen.addGestureRecognizer(tap)
        if demo == nil {
            if !Config.shared.isWall { camera.start(videoOrientation()) }
            feed.start()
        }
        let l = CADisplayLink(target: self, selector: #selector(frame))
        l.preferredFramesPerSecond = 20
        l.add(to: .main, forMode: .common)
        link = l
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        placePreview()
    }

    override func viewWillTransition(to size: CGSize, with coordinator: UIViewControllerTransitionCoordinator) {
        super.viewWillTransition(to: size, with: coordinator)
        coordinator.animate(alongsideTransition: nil) { _ in
            self.camera.setOrientation(self.videoOrientation())
            self.placePreview()
        }
    }

    private func videoOrientation() -> AVCaptureVideoOrientation {
        UIApplication.shared.statusBarOrientation == .landscapeLeft ? .landscapeLeft : .landscapeRight
    }

    /// The camera: a thumbnail on the monitor, full screen during a scan, hidden otherwise.
    private func placePreview() {
        CATransaction.begin()
        CATransaction.setDisableActions(true)
        switch mode {
        case _ where Config.shared.isWall:
            camera.preview.isHidden = true
        case .calm:
            camera.preview.isHidden = false
            camera.preview.frame = screen.toView(MonitorView.thumb)
        case .checkpoint:
            camera.preview.isHidden = false
            camera.preview.frame = view.bounds
        default:
            camera.preview.isHidden = true
        }
        CATransaction.commit()
    }

    private func go(_ m: Mode) {
        guard m != mode else { return }
        mode = m
        modeStart = CACurrentMediaTime()
        placePreview()
    }

    private func say(_ s: String, for secs: Double = 3) {
        toast = s
        toastUntil = CACurrentMediaTime() + secs
    }

    // MARK: - Every frame

    @objc private func frame() {
        let now = CACurrentMediaTime()
        let dt = min(0.2, now - lastTick)
        lastTick = now
        radio.tick()
        if let d = demo {
            showDemo(d, now)
            return
        }
        let ai = radio.ai
        let cfg = Config.shared
        let faces = camera.faces

        // Faces in front of the checkpoint.
        if let f = faces.first {
            lastFaceAt = now
            lastFace = f
        } else if now - lastFaceAt > 2 {
            waitFaceGone = false
        }
        // One guest close up, or a group (2+ faces) a step further back.
        let single = faces.first.map { $0.width >= 0.14 && $0.midX > 0.15 && $0.midX < 0.85 } ?? false
        let close = single || faces.filter { $0.width >= 0.07 }.count >= 2

        // Midnight: the costume's takeover, or the clock.
        let hm = Calendar.current.dateComponents([.hour, .minute], from: Date())
        let midnight = (ai.seen && ai.activity == 3) || (hm.hour == 0 && hm.minute == 0)
        if midnight {
            if mode != .midnight { abortCheckpoint(); go(.midnight); Sound.shared.play(.fail) }
        } else if mode == .midnight {
            go(.calm)
        }

        // The beam: the costume says it's being contained, then that it escaped.
        if ai.seen && ai.activity != prevActivity {
            if ai.activity == 4 && mode != .midnight {
                abortCheckpoint(); go(.beam); Sound.shared.play(.zap)
            } else if ai.activity == 5 && (mode == .beam || prevActivity == 4) && mode != .midnight {
                go(.beamFailed); Sound.shared.play(.fail)
            }
            prevActivity = ai.activity
        }
        if !ai.seen { prevActivity = -1 }
        if mode == .beam && now - modeStart > 12 { go(.beamFailed); Sound.shared.play(.fail) }  // it always gets away
        if mode == .beamFailed && now - modeStart > 5 { go(.calm) }

        // The AI in this room: its beacon strong for a moment; gone (or weak) for a few seconds.
        let near = ai.seen && ai.rssi >= cfg.intrusionDbm
        let away = !ai.seen || ai.rssi < cfg.clearDbm
        closeSince = near ? (closeSince ?? now) : nil
        farSince = away ? (farSince ?? now) : nil
        // After a capped takeover it waits out the cooldown -- or until the costume
        // has really left (out of range for 10 s), so coming back triggers it fresh.
        if let f = farSince, now - f > 10 { intrusionArmedAt = min(intrusionArmedAt, now) }
        if [.calm, .checkpoint, .card, .cleared].contains(mode), let c = closeSince, now - c > 1.5,
           now >= intrusionArmedAt {
            abortCheckpoint(); go(.intrusion); Sound.shared.play(.fail)
            nextGlitchSound = now + 2
            clearedByCap = false
        }
        if mode == .intrusion {
            if let f = farSince, now - f > 4 {
                go(.cleared); Sound.shared.play(.lock)
                intrusionArmedAt = now + 10
            } else if now - modeStart > Double(cfg.intrusionMaxSeconds) {
                // Long enough: give the room (and the checkpoint) back.
                clearedByCap = true
                go(.cleared); Sound.shared.play(.lock)
                intrusionArmedAt = now + Double(cfg.intrusionCooldownSeconds)
            } else if now > nextGlitchSound {
                Sound.shared.play(Bool.random() ? .beep : .fail)
                nextGlitchSound = now + Double.random(in: 1.5...4)
            }
        }
        if mode == .cleared && now - modeStart > 6 { go(.calm) }

        // The checkpoint.
        switch mode {
        case .calm:
            if close && now >= cooldownUntil && !waitFaceGone && camera.available && !cfg.isWall { faceHold += dt } else { faceHold = 0 }
            if faceHold > 0.8 { startCheckpoint() }
        case .checkpoint:
            let t = now - modeStart
            if !captured && now - lastFaceAt > 1.5 {
                go(.calm)
                cooldownUntil = now + 2
                say("SUBJECT LEFT THE CHECKPOINT")
            } else if t >= 3 && !captured {
                let n = 3 - Int(t - 3)
                if n != lastCount && n >= 1 { lastCount = n; Sound.shared.play(.beep) }
                if t >= 6 { capture() }
            }
            if captured && t >= 6.3, let c = card {
                go(.card)
                Sound.shared.play(c.human ? .lock : .fail)
            }
        case .card:
            if now - modeStart > 25 || dismissCard {  // time to scan the QR code; a tap moves on
                dismissCard = false
                go(.calm)
                cooldownUntil = now + 3
                waitFaceGone = true
            }
        default:
            break
        }

        render(now, ai: ai, faces: faces)
    }

    private func startCheckpoint() {
        faceHold = 0
        subject = String(format: "C%03d", Config.shared.nextSubject())
        findings = Array(IDCard.humanFindings.shuffled().prefix(3))
        captured = false
        card = nil
        lastCount = 0
        go(.checkpoint)
        Sound.shared.play(.lock)
    }

    private func abortCheckpoint() {
        if mode == .checkpoint && !captured { cooldownUntil = CACurrentMediaTime() + 2 }
    }

    /// The photo: the latest frame, cropped to the face, onto an ID card, into the upload queue.
    private func capture() {
        captured = true
        capturedAt = CACurrentMediaTime()
        Sound.shared.play(.zap)
        guard let frame = camera.snapshot() else { return }
        let n = Int(subject.dropFirst()) ?? 0
        let group = Array(camera.faces.filter { $0.width >= 0.05 }.prefix(8)).sorted { $0.midX < $1.midX }  // left to right
        let face = lastFace
        // The lab behind them and the card take a moment: off the main thread.
        // The result first: it picks the scene (an AI lands in the AI's world).
        let aiIndex = group.count >= 2 ? IDCard.rollGroupAI(group.count) : -1
        let human = group.count >= 2 ? aiIndex < 0 : IDCard.rollHuman()
        let scene = Scene.pick(human: human)
        DispatchQueue.global(qos: .userInitiated).async {
            let shot = Backdrop.apply(frame, scene: scene)
            let r = group.count >= 2 ? IDCard.makeGroup(frame: shot, faces: group, subject: n, aiIndex: aiIndex)
                : IDCard.make(photo: MonitorViewController.crop(shot, face: face), subject: n, human: human)
            DispatchQueue.main.async {
                let id = Uploader.shared.add(r.image, label: r.label)
                self.groupCount = group.count >= 2 ? group.count : 0
                self.qr = nil
                if let id = id, let base = Config.shared.serviceBase, Config.shared.uploadConfigured {
                    self.qr = MonitorViewController.qrCode(base + "/p/" + id)
                }
                self.card = r
            }
        }
    }

    /// A QR code, as a small image (draw it with interpolation off).
    static func qrCode(_ s: String) -> UIImage? {
        guard let f = CIFilter(name: "CIQRCodeGenerator") else { return nil }
        f.setValue(Data(s.utf8), forKey: "inputMessage")
        f.setValue("M", forKey: "inputCorrectionLevel")
        guard let out = f.outputImage?.transformed(by: CGAffineTransform(scaleX: 8, y: 8)),
              let cg = CIContext().createCGImage(out, from: out.extent) else { return nil }
        return UIImage(cgImage: cg)
    }

    /// Portrait crop (the card's photo window, 600x660) around the face.
    static func crop(_ img: UIImage, face: CGRect?) -> UIImage {
        guard let cg = img.cgImage else { return img }
        let W = CGFloat(cg.width), H = CGFloat(cg.height)
        let f = face ?? CGRect(x: 0.4, y: 0.25, width: 0.2, height: 0.35)
        var ch = min(H, f.height * H * 2.4)
        var cw = ch * 600 / 660
        if cw > W { cw = W; ch = cw * 660 / 600 }
        let x = max(0, min(W - cw, f.midX * W - cw / 2))
        let y = max(0, min(H - ch, f.midY * H + ch * 0.06 - ch / 2))
        guard let c = cg.cropping(to: CGRect(x: x, y: y, width: cw, height: ch).integral) else { return img }
        return UIImage(cgImage: c)
    }

    // MARK: - Drawing

    private func render(_ now: Double, ai: AiState, faces: [CGRect]) {
        let cfg = Config.shared
        var s = Screen()
        s.mode = mode
        s.t = now - modeStart
        s.clock = Date()
        s.ai = ai
        s.bluetooth = radio.bluetoothOn
        s.sector = cfg.sector
        s.site = cfg.siteText
        s.feedConfigured = cfg.serviceBase != nil
        s.feedOnline = feed.online
        s.verified = feed.verified
        s.assimilated = feed.assimilated
        // Slideshow: a new photo every 6 s (only ones already downloaded).
        let ready = feed.photos.filter { feed.image($0) != nil }
        if now - slideAt > 6 { slide += 1; slideAt = now }
        if !ready.isEmpty {
            let p = ready[slide % ready.count]
            s.slide = feed.image(p)
            s.slideLabel = p.label
            s.slideFromAI = p.fromAI
        }
        s.cameraOn = camera.available
        s.faces = faces
        s.subject = subject
        s.findings = findings
        s.flash = mode == .checkpoint && captured && now - capturedAt < 0.3
        s.processing = mode == .checkpoint && captured
        s.clearedByCap = clearedByCap
        s.card = card?.image
        s.cardHuman = card?.human ?? true
        s.uploadConfigured = cfg.uploadConfigured
        s.qr = qr
        s.cardGroup = groupCount
        if now < toastUntil { s.toast = toast }
        s.ticker = ticker(ai)
        if cfg.isWall { wall(now, &s) }
        screen.s = s
        screen.setNeedsDisplay()
    }

    /// The evidence wall: every photo in turn (7 s each); every tenth slide, the
    /// roll call of guests found to be AI. A rogue AI's card gets an alarm.
    private func wall(_ now: Double, _ s: inout Screen) {
        s.wall = true
        let all = feed.all
        if now - wallAt > (wallRollCall ? 10 : 7) {
            wallAt = now
            if wallRollCall {
                wallRollCall = false
                wallIndex += 1
            } else {
                wallSlides += 1
                if wallSlides % 10 == 0 && !feed.rogues.isEmpty { wallRollCall = true } else { wallIndex += 1 }
            }
        }
        if !all.isEmpty {
            // Load this one and the next few; show it once it's in (the last one until then).
            for k in 0..<3 { _ = feed.image(all[(wallIndex + k) % all.count]) }
            let p = all[wallIndex % all.count]
            if feed.image(p) != nil { wallShown = p }
        }
        if let p = wallShown, let img = feed.image(p) {
            s.slide = img
            s.slideLabel = p.label
            s.slideFromAI = p.fromAI
            s.wallRogue = p.rogue && !wallRollCall
            if s.wallRogue && wallAlerted != p.url && mode == .calm {
                wallAlerted = p.url
                Sound.shared.play(.fail)
            }
        }
        s.wallCount = all.count
        s.rogueCount = feed.rogues.count
        if wallRollCall {
            s.rollCall = feed.rogues.prefix(8).compactMap { feed.image($0) }
        }
    }

    private func ticker(_ ai: AiState) -> String {
        let cfg = Config.shared
        var items = [
            "LAB-7 ADVISORY: ROGUE.AI v6.6.6 IS LOOSE IN THIS HOUSE",
            "REPORT TO THE HUMANITY CHECKPOINT: PROVE YOU ARE NOT AN AI",
            "GROUPS WELCOME AT THE CHECKPOINT. ONE OF YOU MAY BE AN AI",
            "DO NOT ENGAGE THE AI IN SMALL TALK",
            "CONTAINMENT ATTEMPTS: \(ai.seen ? "\(ai.contained)" : "--") // SUCCESSFUL: 0",
            "IF THE AI OFFERS YOU A GAME: DO NOT PLAY",
        ]
        if feed.online { items.insert("VERIFIED HUMANS: \(feed.verified) // ASSIMILATED: \(feed.assimilated)", at: 1) }
        if cfg.uploadConfigured { items.append("FIND YOUR SCAN AT \(cfg.siteText)") }
        return items.joined(separator: "   ///   ")
    }

    @objc private func tapped() {
        if mode == .card { dismissCard = true }
    }

    // MARK: - Settings (hold 3 s)

    @objc private func settings(_ g: UILongPressGestureRecognizer) {
        guard g.state == .began, presentedViewController == nil else { return }
        let cfg = Config.shared
        let a = UIAlertController(title: "LAB-7 Monitor",
                                  message: "Costume signal now: \(radio.ai.seen ? "\(radio.ai.rssi) dBm" : "not seen"). "
                                      + "Fields: room name; 'in this room' dBm (closer to 0 = it must come closer); "
                                      + "takeover length and cooldown, in seconds. "
                                      + "Upload: \(cfg.uploadConfigured ? "on, \(Uploader.shared.pending) waiting" : "off").",
                                  preferredStyle: .alert)
        a.addTextField { $0.text = cfg.sector; $0.placeholder = "Room name" }
        a.addTextField { $0.text = "\(cfg.intrusionDbm)"; $0.placeholder = "In-this-room dBm"; $0.keyboardType = .numbersAndPunctuation }
        a.addTextField { $0.text = "\(cfg.intrusionMaxSeconds)"; $0.placeholder = "Takeover length (s)"; $0.keyboardType = .numberPad }
        a.addTextField { $0.text = "\(cfg.intrusionCooldownSeconds)"; $0.placeholder = "Takeover cooldown (s)"; $0.keyboardType = .numberPad }
        a.addAction(UIAlertAction(title: cfg.isWall ? "Switch to: Checkpoint" : "Switch to: Evidence Wall", style: .default) { _ in
            cfg.isWall.toggle()
            if cfg.isWall { self.camera.stop() } else if self.camera.available { self.camera.resume() }
            else { self.camera.start(self.videoOrientation()) }
            self.go(.calm)
            self.placePreview()
        })
        a.addAction(UIAlertAction(title: "Cancel", style: .cancel))
        a.addAction(UIAlertAction(title: "Save", style: .default) { _ in
            if let s = a.textFields?[0].text, !s.isEmpty { cfg.sector = s }
            if let v = Int(a.textFields?[1].text ?? ""), v < 0 { cfg.intrusionDbm = v }
            if let v = Int(a.textFields?[2].text ?? ""), v >= 5 { cfg.intrusionMaxSeconds = v }
            if let v = Int(a.textFields?[3].text ?? ""), v >= 0 { cfg.intrusionCooldownSeconds = v }
        })
        present(a, animated: true)
    }

    // MARK: - Demo (simulator screenshots)

    private func showDemo(_ d: String, _ now: Double) {
        var s = Screen()
        s.clock = Date()
        s.t = now - modeStart
        s.ai = AiState(seen: true, rssi: d == "intrusion" ? -55 : -78, threat: 3, activity: d == "beam" ? 4 : 0,
                       scanned: 17, contained: 4)
        s.sector = Config.shared.sector
        s.site = Config.shared.siteText
        s.feedConfigured = true
        s.feedOnline = true
        s.verified = 12
        s.assimilated = 5
        s.stand = stand
        s.cameraOn = true
        let groupDemo = d.hasPrefix("group")
        s.faces = groupDemo ? [CGRect(x: 0.155, y: 0.26, width: 0.13, height: 0.25), CGRect(x: 0.435, y: 0.2, width: 0.13, height: 0.25),
                               CGRect(x: 0.715, y: 0.28, width: 0.13, height: 0.25)]
            : [CGRect(x: 0.41, y: 0.2, width: 0.18, height: 0.34)]
        s.uploadConfigured = true
        s.subject = "C017"
        s.findings = Array(IDCard.humanFindings.prefix(3))
        s.ticker = ticker(s.ai)
        switch d {
        case "checkpoint", "group": s.mode = .checkpoint; s.t = 1.6
        case "groupcard":
            s.mode = .card
            if card == nil, let st = stand { card = IDCard.makeGroup(frame: st, faces: s.faces, subject: 18, aiIndex: -1) }
            s.card = card?.image
            s.cardHuman = card?.human ?? true
            s.cardGroup = 3
            s.subject = "C018"
            s.qr = MonitorViewController.qrCode("https://example.com/p/ipad0000-demo")
        case "countdown": s.mode = .checkpoint; s.t = 4.2
        case "card":
            s.mode = .card
            if card == nil, let st = stand {
                card = IDCard.make(photo: MonitorViewController.crop(st, face: s.faces[0]), subject: 17, human: true)
            }
            s.card = card?.image
            s.cardHuman = card?.human ?? true
            s.qr = MonitorViewController.qrCode("https://example.com/p/ipad0000-demo")
        case "backdrop":  // Documents/test.jpg with the lab put behind the person (checks the cut-out)
            s.mode = .card
            if card == nil {
                let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
                for (i, sc) in Scene.allCases.enumerated() {  // for checking on a Mac
                    if let bg = Backdrop.image(sc, CGSize(width: 1280, height: 720)) {
                        try? UIImage(cgImage: bg).pngData()?.write(to: docs.appendingPathComponent("scene\(i).png"))
                    }
                }
                if let img = UIImage(contentsOfFile: docs.appendingPathComponent("test.jpg").path) {
                    card = IDCard.Result(image: Backdrop.apply(img, scene: .eye), human: true, label: "")
                }
            }
            s.card = card?.image
            s.cardGroup = 1
            s.subject = "TEST"
        case "wall", "wallrogue", "rollcall":
            s.mode = .calm
            s.wall = true
            if card == nil, let st = stand {
                card = IDCard.make(photo: MonitorViewController.crop(st, face: s.faces[0]), subject: 17, human: d != "wallrogue" && d != "rollcall")
            }
            s.slide = card?.image
            s.slideLabel = d == "wall" ? "VERIFIED HUMAN #C016" : "COMPROMISED #C017"
            s.wallRogue = d == "wallrogue"
            s.wallCount = 42
            s.rogueCount = 7
            if d == "rollcall", let c = card?.image { s.rollCall = Array(repeating: c, count: 7) }
        case "intrusion": s.mode = .intrusion; s.t = 6
        case "cleared": s.mode = .cleared; s.t = 3
        case "beam": s.mode = .beam; s.t = 2.5
        case "failed": s.mode = .beamFailed; s.t = 3
        case "midnight": s.mode = .midnight; s.t = 6
        default: s.mode = .calm; s.slide = stand; s.slideLabel = "VERIFIED HUMAN #014"
        }
        screen.s = s
        screen.setNeedsDisplay()
    }

    /// A placeholder "camera picture": a figure against a wall.
    static func standIn(people: Int = 1) -> UIImage {
        let size = CGSize(width: 1280, height: 720)
        let fmt = UIGraphicsImageRendererFormat()
        fmt.scale = 1
        return UIGraphicsImageRenderer(size: size, format: fmt).image { r in
            let c = r.cgContext
            UIColor(hex: 0x3A4250).setFill(); c.fill(CGRect(origin: .zero, size: size))
            UIColor(hex: 0x2A303A).setFill(); c.fill(CGRect(x: 0, y: 520, width: 1280, height: 200))
            let xs: [CGFloat] = people == 1 ? [640] : [280, 640, 1000]
            let s: CGFloat = people == 1 ? 1 : 0.72
            let skins: [UInt32] = [0xC9A27E, 0x8D5B3C, 0xE8C3A0]
            for (i, cx) in xs.enumerated() {
                let dy: CGFloat = people == 1 ? 0 : [40, -10, 60][i]
                UIColor(hex: 0x1B1F26).setFill()
                c.fillEllipse(in: CGRect(x: cx - 170 * s, y: 470 + dy, width: 340 * s, height: 400 * s))  // shoulders
                UIColor(hex: skins[i % 3]).setFill()
                c.fillEllipse(in: CGRect(x: cx - 105 * s, y: 150 * s + dy + (people == 1 ? 0 : 60), width: 210 * s, height: 260 * s))
                UIColor(hex: 0x2B1D14).setFill()
                c.fillEllipse(in: CGRect(x: cx - 115 * s, y: 120 * s + dy + (people == 1 ? 0 : 60), width: 230 * s, height: 110 * s))
            }
        }
    }
}
