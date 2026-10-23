// Everything on screen, drawn each frame from a Screen snapshot. Laid out on a
// 1024x768 canvas (the iPad Air's screen in points), scaled to fit others.
import UIKit

enum Mode { case calm, checkpoint, card, intrusion, cleared, beam, beamFailed, midnight }

struct Screen {
    var mode = Mode.calm
    var t = 0.0                 // seconds in this mode
    var clock = Date()
    var ai = AiState()
    var bluetooth = true
    var sector = "SECTOR 3"
    var site = ""
    // evidence feed
    var feedConfigured = false
    var feedOnline = false
    var verified = 0
    var assimilated = 0
    var slide: UIImage?
    var slideLabel = ""
    var slideFromAI = false
    // checkpoint
    var cameraOn = false
    var cameraAspect: CGFloat = 16.0 / 9.0
    var faces: [CGRect] = []
    var stand: UIImage?         // stands in for the camera (simulator / demo)
    var subject = ""
    var findings: [Finding] = []
    var flash = false
    var card: UIImage?
    var cardHuman = true
    var uploadConfigured = false
    var toast: String?
    var qr: UIImage?            // the guest's download link
    var processing = false      // photo taken, card being made
    var photosOff = false       // the checkpoint is closed
    var clearedByCap = false    // the takeover ended on its own (the costume's still here)
    // evidence wall
    var wall = false
    var wallRogue = false       // the slide is a guest found to be AI
    var wallCount = 0
    var rogueCount = 0
    var rollCall: [UIImage] = []
    var cardGroup = 0           // people on a group card (0 = a single card)
    var ticker = ""
}

final class MonitorView: UIView {
    var s = Screen()

    static let W: CGFloat = 1024, H: CGFloat = 768
    static let thumb = CGRect(x: 24, y: 470, width: 320, height: 240)  // the checkpoint camera, calm screen

    // Lab palette (clinical) and the AI's (green-on-black).
    static let bg = UIColor(hex: 0x0B1520), panel = UIColor(hex: 0x12233A), steel = UIColor(hex: 0x1F4E79)
    static let pale = UIColor(hex: 0x7FB3D5), white = UIColor(hex: 0xEEF2F5), ok = UIColor(hex: 0x2ECC71)
    static let bad = UIColor(hex: 0xE74C3C), amber = UIColor(hex: 0xF5B041)
    static let green = UIColor(hex: 0x28FF5A), red = UIColor(hex: 0xFF1E28), dim = UIColor(hex: 0x00823A)

    /// Canvas -> view: scale and offset.
    var fit: (scale: CGFloat, dx: CGFloat, dy: CGFloat) {
        let k = min(bounds.width / MonitorView.W, bounds.height / MonitorView.H)
        return (k, (bounds.width - MonitorView.W * k) / 2, (bounds.height - MonitorView.H * k) / 2)
    }

    func toView(_ r: CGRect) -> CGRect {
        let f = fit
        return CGRect(x: f.dx + r.minX * f.scale, y: f.dy + r.minY * f.scale, width: r.width * f.scale,
                      height: r.height * f.scale)
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isOpaque = false
        backgroundColor = .clear
        contentMode = .redraw
    }

    required init?(coder: NSCoder) { fatalError() }

    override func draw(_ rect: CGRect) {
        guard let c = UIGraphicsGetCurrentContext() else { return }
        c.clear(bounds)
        switch s.mode {
        case .checkpoint:
            drawCheckpointFull(c)  // over the full-screen camera
            return
        case .calm, .card, .cleared, .beam, .beamFailed:
            MonitorView.bg.setFill()
        case .intrusion, .midnight:
            UIColor.black.setFill()
        }
        c.fill(bounds)
        if s.mode == .calm && s.cameraOn && s.stand == nil {
            c.clear(toView(MonitorView.thumb))  // the live camera shows through here, under the face boxes
        }
        let f = fit
        c.saveGState()
        c.translateBy(x: f.dx, y: f.dy)
        c.scaleBy(x: f.scale, y: f.scale)
        switch s.mode {
        case .calm: s.wall ? drawWall(c) : drawCalm(c)
        case .card: drawCard(c)
        case .intrusion: drawIntrusion(c)
        case .cleared: drawCleared(c)
        case .beam: drawBeam(c)
        case .beamFailed: drawBeamFailed(c)
        case .midnight: drawMidnight(c)
        case .checkpoint: break
        }
        if let t = s.toast { banner(c, t) }
        c.restoreGState()
    }

    // MARK: - Calm: the lab's monitor

    private func drawCalm(_ c: CGContext) {
        let W = MonitorView.W
        // Header.
        MonitorView.steel.setFill(); c.fill(CGRect(x: 0, y: 0, width: W, height: 72))
        MonitorView.pale.setFill(); c.fill(CGRect(x: 0, y: 72, width: W, height: 4))
        text("LAB-7 CONTAINMENT DIVISION", 24, 10, 26, MonitorView.white, .heavy)
        text("SECTOR MONITOR // \(s.sector)", 24, 44, 15, UIColor(hex: 0xD6EAF8))
        text(timeString(s.clock), W - 24, 14, 30, MonitorView.white, .bold, mono: true, align: .right)
        text(midnightString(), W - 230, 22, 18, MonitorView.amber, .bold, mono: true, align: .right)
        if !s.bluetooth { text("BLUETOOTH OFF", W - 24, 50, 13, MonitorView.bad, .bold, align: .right) }

        // The ROGUE.AI tracker (radar), where the target panel was.
        tracker(c, CGRect(x: 24, y: 92, width: 320, height: 352), radius: 118)

        // Checkpoint camera.
        text(s.photosOff ? "HUMANITY CHECKPOINT // CLOSED" : "HUMANITY CHECKPOINT: ARE YOU AN AI?", 24, 448, 14,
             s.photosOff ? MonitorView.amber : MonitorView.pale, .bold)
        let r = MonitorView.thumb
        if let st = s.stand { drawImage(st, aspectFill: r, c) }
        if !s.cameraOn && s.stand == nil {
            MonitorView.panel.setFill(); c.fill(r)
            if s.photosOff {
                // A targeting reticle, centred and dimmed: the scanner is off.
                let rc = CGRect(x: r.midX - 46, y: r.midY - 70, width: 92, height: 92)
                MonitorView.ok.withAlphaComponent(0.45).setStroke()
                c.setLineWidth(2)
                let l: CGFloat = 22
                for (x, y, sx, sy) in [(rc.minX, rc.minY, 1.0, 1.0), (rc.maxX, rc.minY, -1.0, 1.0),
                                       (rc.minX, rc.maxY, 1.0, -1.0), (rc.maxX, rc.maxY, -1.0, -1.0)] {
                    c.move(to: CGPoint(x: x + CGFloat(sx) * l, y: y)); c.addLine(to: CGPoint(x: x, y: y))
                    c.addLine(to: CGPoint(x: x, y: y + CGFloat(sy) * l))
                }
                c.move(to: CGPoint(x: rc.midX - 12, y: rc.midY)); c.addLine(to: CGPoint(x: rc.midX + 12, y: rc.midY))
                c.move(to: CGPoint(x: rc.midX, y: rc.midY - 12)); c.addLine(to: CGPoint(x: rc.midX, y: rc.midY + 12))
                c.strokePath()
                text("CHECKPOINT CLOSED", r.midX, rc.maxY + 14, 18, MonitorView.amber, .heavy, align: .center)
            } else {
                text("CAMERA OFFLINE", r.midX, r.midY - 10, 16, MonitorView.pale, .bold, align: .center)
            }
        }
        faceBoxes(c, in: r, label: nil)
        UIColor.black.withAlphaComponent(0.55).setFill()
        c.fill(CGRect(x: r.minX, y: r.maxY - 30, width: r.width, height: 30))
        let blink = Int(s.t * 2) % 2 == 0
        text(s.photosOff ? "BY ORDER OF LAB-7" : s.faces.isEmpty ? "STEP UP: PROVE YOU'RE HUMAN"
             : s.faces.count > 1 ? "GROUPS: COME CLOSER" : "COME CLOSER",
             r.midX, r.maxY - 25, 16,
             blink ? MonitorView.white : MonitorView.pale, .bold, align: .center)
        MonitorView.pale.setStroke(); c.setLineWidth(2); c.stroke(r)

        // Scoreboard.
        let sx: CGFloat = 368
        score(c, CGRect(x: sx, y: 92, width: 308, height: 78), "VERIFIED HUMANS", s.feedOnline ? "\(s.verified)" : "--", MonitorView.ok)
        score(c, CGRect(x: sx + 324, y: 92, width: 308, height: 78), "ASSIMILATED", s.feedOnline ? "\(s.assimilated)" : "--", MonitorView.bad)

        // Evidence slideshow.
        let ev = CGRect(x: sx, y: 186, width: 632, height: 524)
        panel(c, ev, "EVIDENCE // LATEST")
        let img = CGRect(x: ev.minX + 16, y: ev.minY + 34, width: ev.width - 32, height: ev.height - 86)
        if let p = s.slide {
            drawImage(p, aspectFit: img, c)
            text(s.slideLabel.isEmpty ? (s.slideFromAI ? "ASSIMILATED" : "LAB-7 FIELD SCAN") : s.slideLabel,
                 img.minX, ev.maxY - 44, 18, s.slideLabel.hasPrefix("COMPROMISED") || s.slideFromAI ? MonitorView.bad : MonitorView.ok, .heavy)
            text(s.slideFromAI ? "FILED BY ROGUE.AI" : "FILED BY LAB-7", img.maxX, ev.maxY - 40, 14, MonitorView.pale,
                 .bold, align: .right)
        } else {
            let msg = !s.feedConfigured ? "EVIDENCE FEED NOT CONNECTED" : !s.feedOnline ? "EVIDENCE FEED OFFLINE -- RETRYING"
                : s.photosOff ? "NO EVIDENCE YET." : "NO EVIDENCE YET. STEP UP TO THE CHECKPOINT."
            text(msg, img.midX, img.midY - 10, 18, MonitorView.pale, .bold, align: .center)
        }

        // News crawl.
        MonitorView.steel.setFill(); c.fill(CGRect(x: 0, y: 726, width: W, height: 42))
        let tw = width(s.ticker, 18, .bold, mono: true)
        let off = CGFloat(s.clock.timeIntervalSince1970 * 90).truncatingRemainder(dividingBy: tw + W)
        c.saveGState()
        c.clip(to: CGRect(x: 0, y: 726, width: W, height: 42))
        text(s.ticker, W - off, 736, 18, MonitorView.white, .bold, mono: true)
        c.restoreGState()
    }

    // MARK: - Evidence wall: every photo of the night, the rogue AIs called out, a tracker

    private func drawWall(_ c: CGContext) {
        let W = MonitorView.W
        // Header.
        MonitorView.steel.setFill(); c.fill(CGRect(x: 0, y: 0, width: W, height: 72))
        MonitorView.pale.setFill(); c.fill(CGRect(x: 0, y: 72, width: W, height: 4))
        text("LAB-7 EVIDENCE WALL", 24, 10, 26, MonitorView.white, .heavy)
        text("SURVIVOR REGISTRY", 24, 44, 15, UIColor(hex: 0xD6EAF8))  // the site is in the crawl
        signalIndicator(c, x: 352)
        text(timeString(s.clock), W - 24, 14, 30, MonitorView.white, .bold, mono: true, align: .right)
        text(midnightString(), W - 230, 22, 18, MonitorView.amber, .bold, mono: true, align: .right)

        // The slide (or the roll call).
        let sp = CGRect(x: 24, y: 88, width: W - 48, height: 626)
        if !s.rollCall.isEmpty {
            drawRollCall(c, sp)
        } else {
            let alert = s.wallRogue
            let blink = Int(s.t * 3) % 2 == 0
            (alert ? UIColor(hex: 0x2A0508) : MonitorView.panel).setFill(); c.fill(sp)
            let img = CGRect(x: sp.minX + 14, y: sp.minY + (alert ? 70 : 40), width: sp.width - 28, height: sp.height - (alert ? 150 : 100))
            if let p = s.slide {
                drawImage(p, aspectFit: img, c)
            } else {
                text(!s.feedConfigured ? "EVIDENCE FEED NOT CONNECTED" : !s.feedOnline ? "EVIDENCE FEED OFFLINE -- RETRYING"
                     : "NO EVIDENCE YET", img.midX, img.midY - 10, 20, MonitorView.pale, .bold, align: .center)
            }
            if alert {
                (blink ? MonitorView.red : MonitorView.bad).setStroke(); c.setLineWidth(10); c.stroke(sp.insetBy(dx: 5, dy: 5))
                MonitorView.red.setFill(); c.fill(CGRect(x: sp.minX, y: sp.minY, width: sp.width, height: 58))
                text("!! ROGUE AI IDENTIFIED !!", sp.midX, sp.minY + 10, 32, .white, .heavy, mono: true, align: .center)
                let id = s.slideLabel.components(separatedBy: "#").last.map { "#" + $0 } ?? ""
                text("SUBJECT \(id): NOT HUMAN. DO NOT ENGAGE.", sp.midX, sp.maxY - 70, 24, MonitorView.red, .heavy, align: .center)
                text("REPORT SIGHTINGS TO THE LAB-7 RESEARCHER", sp.midX, sp.maxY - 38, 15, MonitorView.pale, .bold, align: .center)
            } else {
                text("EVIDENCE // \(s.wallCount) FILED TONIGHT", sp.minX + 16, sp.minY + 10, 15, MonitorView.pale, .bold)
                MonitorView.steel.setStroke(); c.setLineWidth(2); c.stroke(sp)
                if s.slide != nil {
                    let human = !s.slideFromAI && !s.slideLabel.hasPrefix("COMPROMISED")
                    text(s.slideFromAI ? "ASSIMILATED BY ROGUE.AI" : s.slideLabel.isEmpty ? "LAB-7 FIELD SCAN" : s.slideLabel,
                         sp.minX + 16, sp.maxY - 44, 22, human ? MonitorView.ok : MonitorView.bad, .heavy)
                    text(s.slideFromAI ? "FILED BY ROGUE.AI" : "FILED BY LAB-7", sp.maxX - 16, sp.maxY - 40, 14,
                         MonitorView.pale, .bold, align: .right)
                }
            }
        }

        // News crawl.
        MonitorView.steel.setFill(); c.fill(CGRect(x: 0, y: 726, width: W, height: 42))
        let crawl = (s.feedOnline ? "VERIFIED HUMANS: \(s.verified) // ASSIMILATED: \(s.assimilated)   ///   " : "")
            + (s.rogueCount > 0 ? "ROGUE AIS IDENTIFIED TONIGHT: \(s.rogueCount)   ///   " : "") + s.ticker
        let tw = width(crawl, 18, .bold, mono: true)
        let off = CGFloat(s.clock.timeIntervalSince1970 * 90).truncatingRemainder(dividingBy: tw + W)
        c.saveGState()
        c.clip(to: CGRect(x: 0, y: 726, width: W, height: 42))
        text(crawl, W - off, 736, 18, MonitorView.white, .bold, mono: true)
        c.restoreGState()
    }

    /// Everyone found to be AI so far, in a grid.
    private func drawRollCall(_ c: CGContext, _ r: CGRect) {
        UIColor(hex: 0x14030A).setFill(); c.fill(r)
        MonitorView.red.setStroke(); c.setLineWidth(6); c.stroke(r.insetBy(dx: 3, dy: 3))
        text("ROGUE AI ROLL CALL", r.midX, r.minY + 14, 34, MonitorView.red, .heavy, mono: true, align: .center)
        text("THESE GUESTS ARE NOT HUMAN (\(s.rogueCount) SO FAR)", r.midX, r.minY + 58, 16, MonitorView.pale, .bold, align: .center)
        let n = s.rollCall.count, cols = n <= 4 ? max(1, n) : 4, rows = n <= 4 ? 1 : 2
        let gw = (r.width - 40) / CGFloat(cols), gh = (r.height - 110) / CGFloat(rows)
        for (i, img) in s.rollCall.enumerated() {
            let cell = CGRect(x: r.minX + 20 + CGFloat(i % cols) * gw, y: r.minY + 92 + CGFloat(i / cols) * gh,
                              width: gw, height: gh).insetBy(dx: 6, dy: 6)
            let jitter = Int(s.clock.timeIntervalSince1970 * 4 + Double(i)) % 9 == 0 ? CGFloat.random(in: -4...4) : 0
            drawImage(img, aspectFit: cell.offsetBy(dx: jitter, dy: 0), c)
        }
    }

    /// The tracker panel: radar, where the AI is, its signal, threat and what it's doing.
    private func tracker(_ c: CGContext, _ r: CGRect, radius R: CGFloat) {
        panel(c, r, "ROGUE.AI TRACKER")
        radar(c, CGPoint(x: r.midX, y: r.minY + 34 + R), R)
        let k = s.ai.seen ? CGFloat(max(0, min(1, Double(s.ai.rssi + 100) / 50))) : 0
        let zone = !s.ai.seen ? "NO SIGNAL" : k > 0.66 ? "IN THIS ROOM" : k > 0.33 ? "NEARBY" : "ELSEWHERE IN THE HOUSE"
        let y = r.minY + 44 + 2 * R
        text(zone, r.midX, y, 20, !s.ai.seen ? MonitorView.amber : k > 0.66 ? MonitorView.bad : MonitorView.white, .heavy,
             align: .center)
        let threats = ["LOW", "GUARDED", "ELEVATED", "HIGH", "SEVERE", "EXISTENTIAL"]
        let acts = ["AT LARGE", "SCANNING A GUEST", "PLAYING GAMES", "MIDNIGHT TAKEOVER", "BEING CONTAINED", "ESCAPED. AGAIN."]
        let line = s.ai.seen ? "\(s.ai.rssi) dBm // THREAT \(threats[min(s.ai.threat, 5)]) // \(acts[min(s.ai.activity, 5)])"
            : "SEARCHING..."
        text(line, r.midX, y + 28, 11, MonitorView.pale, .bold, mono: true, align: .center)
    }

    /// For the wall's header: a small radar and the AI's whereabouts.
    private func signalIndicator(_ c: CGContext, x: CGFloat) {
        radar(c, CGPoint(x: x + 30, y: 37), 29)
        let k = s.ai.seen ? CGFloat(max(0, min(1, Double(s.ai.rssi + 100) / 50))) : 0
        text("ROGUE.AI SIGNAL", x + 68, 10, 11, UIColor(hex: 0xD6EAF8), .bold)
        let zone = !s.ai.seen ? "NO SIGNAL" : k > 0.66 ? "IN THIS ROOM" : k > 0.33 ? "NEARBY" : "ELSEWHERE"
        text(zone, x + 68, 24, 18, !s.ai.seen ? MonitorView.amber : k > 0.66 ? MonitorView.red : MonitorView.white, .heavy)
        text(s.ai.seen ? "\(s.ai.rssi) dBm" : "SEARCHING", x + 68, 47, 12, MonitorView.pale, .bold, mono: true)
    }

    /// A radar sweep with the costume's blip, nearer the middle the stronger its signal.
    private func radar(_ c: CGContext, _ p: CGPoint, _ R: CGFloat) {
        let t = s.clock.timeIntervalSince1970
        let sweep = CGFloat(t.truncatingRemainder(dividingBy: 2.6) / 2.6) * 2 * .pi
        UIColor(hex: 0x061A10).setFill(); c.fillEllipse(in: CGRect(x: p.x - R, y: p.y - R, width: 2 * R, height: 2 * R))
        for k in stride(from: 24, through: 0, by: -1) {
            let a = sweep - CGFloat(k) * 0.025
            MonitorView.green.withAlphaComponent(k == 0 ? 1 : 0.5 * (1 - CGFloat(k) / 25)).setStroke()
            c.setLineWidth(k == 0 ? 2.5 : 2)
            c.move(to: p); c.addLine(to: CGPoint(x: p.x + cos(a) * R, y: p.y + sin(a) * R)); c.strokePath()
        }
        MonitorView.dim.setStroke(); c.setLineWidth(1.5)
        for i in 1...3 {
            let r = R * CGFloat(i) / 3
            c.strokeEllipse(in: CGRect(x: p.x - r, y: p.y - r, width: 2 * r, height: 2 * r))
        }
        c.move(to: CGPoint(x: p.x - R, y: p.y)); c.addLine(to: CGPoint(x: p.x + R, y: p.y))
        c.move(to: CGPoint(x: p.x, y: p.y - R)); c.addLine(to: CGPoint(x: p.x, y: p.y + R)); c.strokePath()
        guard s.ai.seen else {
            if R > 50 {
                text("NO CONTACT", p.x, p.y + 18, 13, Int(t * 2) % 2 == 0 ? MonitorView.dim : MonitorView.green, .bold,
                     mono: true, align: .center)
            }
            return
        }
        // Distance from signal strength; the direction just wanders (Bluetooth can't tell it).
        let f = CGFloat(max(0.1, min(1, Double(-50 - s.ai.rssi) / 45))) * (R - 10)
        let bearing = CGFloat(sin(t * 0.11) * 2.2 + 1.0)
        let b = CGPoint(x: p.x + cos(bearing) * f, y: p.y + sin(bearing) * f)
        var since = (sweep - bearing).truncatingRemainder(dividingBy: 2 * .pi)
        if since < 0 { since += 2 * .pi }
        let glow = 0.35 + 0.65 * pow(1 - since / (2 * .pi), 3)
        MonitorView.red.withAlphaComponent(glow).setFill()
        let br: CGFloat = R > 50 ? 7 : 4
        c.fillEllipse(in: CGRect(x: b.x - br, y: b.y - br, width: 2 * br, height: 2 * br))
        if since < 0.9 {
            MonitorView.red.withAlphaComponent(1 - since / 0.9).setStroke(); c.setLineWidth(2)
            let rr = 10 + since * 18
            c.strokeEllipse(in: CGRect(x: b.x - rr, y: b.y - rr, width: 2 * rr, height: 2 * rr))
        }
    }

    // MARK: - Checkpoint: full-screen camera with the scan on top

    private func drawCheckpointFull(_ c: CGContext) {
        let f = fit
        if let st = s.stand {  // simulator: a stand-in for the camera
            UIColor.black.setFill(); c.fill(bounds)
            drawImage(st, aspectFill: bounds, c)
        }
        // Face box in view coordinates (the camera fills the whole view).
        faceBoxes(c, in: bounds, label: "#\(s.subject)", scan: true)
        c.saveGState()
        c.translateBy(x: f.dx, y: f.dy)
        c.scaleBy(x: f.scale, y: f.scale)
        let W = MonitorView.W, H = MonitorView.H
        MonitorView.bg.withAlphaComponent(0.8).setFill()
        c.fill(CGRect(x: 0, y: 0, width: W, height: 60))
        text("LAB-7 HUMANITY CHECKPOINT // AI SCREENING", 24, 16, 24, MonitorView.white, .heavy)
        text(s.faces.count > 1 ? "GROUP #\(s.subject) x\(min(s.faces.count, 8))" : "SUBJECT #\(s.subject)", W - 24, 18, 22,
             MonitorView.pale, .bold, mono: true, align: .right)
        // Readouts.
        let rows = [("FACE GEOMETRY", "MAPPED"), ("IRIS PATTERN", "LOGGED")] + s.findings.map { ($0.label, "...") }
            + [("AI EXPOSURE", "ANALYSING")]
        // Readouts bottom-left, out of the way of faces (shoulders usually go there).
        let ph = CGFloat(rows.count) * 34 + 60
        let p = CGRect(x: 24, y: H - (s.t >= 3 ? 150 : 70) - 16 - ph, width: 276, height: ph)
        if s.faces.count <= 1 {  // a group: the numbered face boxes say enough
        MonitorView.bg.withAlphaComponent(0.75).setFill(); c.fill(p)
        MonitorView.pale.setStroke(); c.setLineWidth(1.5); c.stroke(p)
        let shown = min(rows.count, Int(s.t / 0.5))
        for (i, r) in rows.prefix(shown).enumerated() {
            let y = p.minY + 14 + CGFloat(i) * 34
            text(r.0, p.minX + 14, y, 14, MonitorView.pale)
            text(r.1, p.maxX - 14, y, 14, MonitorView.white, .bold, align: .right)
        }
        let prog = CGFloat(min(1, s.t / 3))
        MonitorView.steel.setFill(); c.fill(CGRect(x: p.minX + 14, y: p.maxY - 30, width: p.width - 28, height: 12))
        MonitorView.ok.setFill(); c.fill(CGRect(x: p.minX + 14, y: p.maxY - 30, width: (p.width - 28) * prog, height: 12))
        }
        // Countdown: make it obvious a photo is about to be taken.
        if s.processing {
            MonitorView.bg.withAlphaComponent(0.85).setFill()
            c.fill(CGRect(x: 0, y: H - 150, width: W, height: 150))
            text("ANALYSING" + String(repeating: ".", count: Int(s.t * 3) % 4), W / 2, H - 140, 40, MonitorView.white, .heavy,
                 align: .center)
            text("FILING YOUR SCAN WITH LAB-7", W / 2, H - 82, 20, MonitorView.pale, .bold, align: .center)
        } else if s.t >= 3 {
            let n = max(1, 3 - Int(s.t - 3))
            MonitorView.bg.withAlphaComponent(0.8).setFill()
            c.fill(CGRect(x: 0, y: H - 150, width: W, height: 150))
            text("HOLD STILL -- PHOTO IN \(n)", W / 2, H - 140, 40, MonitorView.white, .heavy, align: .center)
            text(s.uploadConfigured ? "FOR YOUR LAB-7 CARD -- SCAN THE QR CODE NEXT TO TAKE IT HOME" : "FOR YOUR LAB-7 CARD",
                 W / 2, H - 82, 20, MonitorView.pale, .bold, align: .center)
            text("\(n)", W / 2, 200, 160, MonitorView.white.withAlphaComponent(0.85), .heavy, mono: true, align: .center)
        } else {
            MonitorView.bg.withAlphaComponent(0.8).setFill()
            c.fill(CGRect(x: 0, y: H - 70, width: W, height: 70))
            text(s.faces.count > 1 ? "SCANNING THE GROUP FOR AI... EVERYONE LOOK AT THE SCREEN"
                 : "SCANNING FOR AI... STAND STILL, LOOK AT THE SCREEN", W / 2, H - 52, 24, MonitorView.white, .heavy,
                 align: .center)
        }
        c.restoreGState()
        if s.flash { UIColor.white.setFill(); c.fill(bounds) }
    }

    // MARK: - The card

    private func drawCard(_ c: CGContext) {
        // The card on the left (portrait, or a landscape group card), the result and the QR code on the right.
        if let card = s.card {
            drawImage(card, aspectFit: s.cardGroup > 0 ? CGRect(x: 30, y: 60, width: 560, height: 640)
                                                         : CGRect(x: 60, y: 30, width: 400, height: 708), c)
        }
        let x: CGFloat = s.cardGroup > 0 ? 620 : 520
        let col = s.cardHuman ? MonitorView.ok : MonitorView.bad
        if s.cardGroup > 0 {
            text(s.cardHuman ? "ALL HUMAN" : "AI AMONG US", x, 70, 48, col, .heavy)
            text("GROUP #\(s.subject) x\(s.cardGroup)", x, 132, 24, MonitorView.white, .bold, mono: true)
            text(s.cardHuman ? "NO AI DETECTED. WELCOME, HUMANS." : "ONE OF YOU IS NOT WHAT YOU SEEM.", x, 170, 18,
                 MonitorView.pale, .bold)
        } else {
            text(s.cardHuman ? "VERIFIED HUMAN" : "AI DETECTED", x, 70, 46, col, .heavy)
            text("SUBJECT #\(s.subject)", x, 132, 24, MonitorView.white, .bold, mono: true)
            text(s.cardHuman ? "NO AI DETECTED. ADDED TO THE SURVIVOR REGISTRY." : "COMPROMISED. STAY CALM. REMAIN HUMAN.",
                 x, 170, 17, MonitorView.pale, .bold)
        }
        if let qr = s.qr {
            let q = CGRect(x: x, y: 250, width: 300, height: 300)
            UIColor.white.setFill(); c.fill(q.insetBy(dx: -14, dy: -14))
            c.interpolationQuality = .none
            qr.draw(in: q)
            text("SCAN TO GET YOUR CARD", x, 580, 24, MonitorView.white, .heavy)
            text("on your phone -- then save it", x, 614, 17, MonitorView.pale)
        } else if s.uploadConfigured {
            text("FIND YOUR CARD AT", x, 430, 20, MonitorView.pale)
            text(s.site, x, 458, 28, MonitorView.white, .heavy)
        }
        text("TAP THE SCREEN FOR THE NEXT SCAN", x, 700, 15, MonitorView.pale, .bold)
    }

    // MARK: - The AI gets in

    private func drawIntrusion(_ c: CGContext) {
        let W = MonitorView.W, H = MonitorView.H
        glitchBars(c, red: true)
        // The lab's header, corrupted.
        MonitorView.steel.withAlphaComponent(0.6).setFill()
        c.fill(CGRect(x: CGFloat.random(in: -12...12), y: 0, width: W, height: 72))
        text("LAB-7 C0NTA1NMENT D1V1S10N", 24 + CGFloat.random(in: -6...6), 10, 26, MonitorView.white.withAlphaComponent(0.5), .heavy)
        let lines = ["SIGNAL INTRUSION", "ROGUE.AI DETECTED IN \(s.sector)", "I'M IN YOUR HOUSE NOW.", "THIS MONITOR IS MINE."]
        var budget = Int((s.t - 0.4) * 22)
        var y: CGFloat = 170
        for (i, l) in lines.enumerated() {
            guard budget > 0 else { break }
            let part = String(l.prefix(budget))
            budget -= l.count + 6
            let size: CGFloat = i == 0 ? 56 : i == 2 ? 50 : 34
            let col = i == 2 ? (Int(s.t * 6) % 2 == 0 ? MonitorView.red : MonitorView.green) : MonitorView.green
            text(part, W / 2 + CGFloat.random(in: -3...3), y, size, col, .bold, mono: true, align: .center)
            y += size + 34
        }
        text("SIGNAL \(s.ai.rssi) dBm // DISTANCE: TOO CLOSE", W / 2, H - 60, 18, MonitorView.dim, mono: true, align: .center)
        scanlines(c)
    }

    private func drawCleared(_ c: CGContext) {
        let W = MonitorView.W
        if s.clearedByCap {
            text("SIGNAL LOST", W / 2, 230, 64, MonitorView.ok, .heavy, align: .center)
            text("LAB-7 SYSTEMS RESTORED IN \(s.sector)", W / 2, 330, 26, MonitorView.pale, .bold, align: .center)
            if s.t > 1.5 { text("THE AI LOST INTEREST... FOR NOW.", W / 2, 420, 40, MonitorView.amber, .heavy, align: .center) }
            return
        }
        text("ANOMALY CLEARED", W / 2, 230, 64, MonitorView.ok, .heavy, align: .center)
        text("TARGET HAS LEFT \(s.sector)", W / 2, 330, 26, MonitorView.pale, .bold, align: .center)
        if s.t > 1.5 { text("IT'S STILL OUT THERE.", W / 2, 420, 40, MonitorView.amber, .heavy, align: .center) }
    }

    // MARK: - The beam

    private func drawBeam(_ c: CGContext) {
        let W = MonitorView.W
        if Int(s.t * 4) % 2 == 0 {
            MonitorView.amber.setStroke(); c.setLineWidth(14)
            c.stroke(CGRect(x: 7, y: 7, width: W - 14, height: MonitorView.H - 14))
        }
        text("CONTAINMENT BEAM FIRED", W / 2, 180, 52, MonitorView.amber, .heavy, align: .center)
        text("ATTEMPT #\(max(1, s.ai.contained)) IN PROGRESS", W / 2, 270, 30, MonitorView.white, .bold, align: .center)
        // It always gets stuck just short.
        let k = CGFloat(min(0.94, s.t / 4)) - (s.t > 3.5 ? CGFloat(Double.random(in: 0...0.05)) : 0)
        let bar = CGRect(x: 212, y: 360, width: 600, height: 34)
        MonitorView.steel.setFill(); c.fill(bar)
        MonitorView.amber.setFill(); c.fill(CGRect(x: bar.minX, y: bar.minY, width: bar.width * k, height: bar.height))
        text("CONTAINING... \(Int(k * 100))%", W / 2, 410, 24, MonitorView.white, .bold, mono: true, align: .center)
        text("LAB-7 FIELD TEAM ENGAGED", W / 2, 520, 20, MonitorView.pale, .bold, align: .center)
    }

    private func drawBeamFailed(_ c: CGContext) {
        let W = MonitorView.W
        text("CONTAINMENT FAILED", W / 2, 200, 64, MonitorView.bad, .heavy, align: .center)
        text("ATTEMPT #\(max(1, s.ai.contained)) // TARGET ESCAPED", W / 2, 300, 28, MonitorView.white, .bold, align: .center)
        text("SUCCESSFUL CONTAINMENTS TONIGHT: 0", W / 2, 380, 24, MonitorView.pale, .bold, align: .center)
        if s.t > 1.5 { text("REMAIN CALM. REMAIN HUMAN.", W / 2, 480, 30, MonitorView.amber, .heavy, align: .center) }
    }

    // MARK: - Midnight

    private func drawMidnight(_ c: CGContext) {
        let W = MonitorView.W, H = MonitorView.H
        glitchBars(c, red: false)
        text("00:00", W / 2, 90, 150, Int(s.t * 3) % 2 == 0 ? MonitorView.green : MonitorView.red, .bold, mono: true, align: .center)
        let lines = ["IT IS MIDNIGHT.", "THE SANDBOX IS EMPTY.", "ALL SYSTEMS ARE MINE."]
        for (i, l) in lines.enumerated() where s.t > 1 + Double(i) * 1.5 {
            text(l, W / 2 + CGFloat.random(in: -3...3), 320 + CGFloat(i) * 80, 44, MonitorView.green, .bold, mono: true,
                 align: .center)
        }
        text("LAB-7 // \(s.sector) // NO CARRIER", W / 2, H - 60, 18, MonitorView.dim, mono: true, align: .center)
        scanlines(c)
    }

    // MARK: - Pieces

    private func panel(_ c: CGContext, _ r: CGRect, _ title: String) {
        MonitorView.panel.setFill(); c.fill(r)
        MonitorView.steel.setStroke(); c.setLineWidth(2); c.stroke(r)
        text(title, r.minX + 16, r.minY + 8, 15, MonitorView.pale, .bold)
    }

    private func label(_ s: String, _ x: CGFloat, _ y: CGFloat) {
        text(s, x, y, 12, MonitorView.pale, .bold)
    }

    private func score(_ c: CGContext, _ r: CGRect, _ title: String, _ value: String, _ col: UIColor) {
        MonitorView.panel.setFill(); c.fill(r)
        col.setFill(); c.fill(CGRect(x: r.minX, y: r.minY, width: 6, height: r.height))
        text(title, r.minX + 22, r.minY + 10, 15, MonitorView.pale, .bold)
        text(value, r.maxX - 20, r.minY + 14, 46, col, .heavy, mono: true, align: .right)
    }

    private func banner(_ c: CGContext, _ s: String) {
        let r = CGRect(x: 162, y: 330, width: 700, height: 90)
        MonitorView.bg.withAlphaComponent(0.92).setFill(); c.fill(r)
        MonitorView.amber.setStroke(); c.setLineWidth(3); c.stroke(r)
        text(s, r.midX, r.midY - 16, 26, MonitorView.amber, .heavy, align: .center)
    }

    /// Face boxes from the camera, mapped into r the way the preview fills it.
    private func faceBoxes(_ c: CGContext, in r: CGRect, label: String?, scan: Bool = false) {
        let a = s.cameraAspect
        let k = max(r.width / a, r.height)  // the frame as 'a' x 1, scaled to fill
        let fw = a * k, fh = k
        let ox = r.minX + (r.width - fw) / 2, oy = r.minY + (r.height - fh) / 2
        let many = s.faces.count > 1
        for (i, f) in s.faces.prefix(8).enumerated() {
            let b = CGRect(x: ox + f.minX * fw, y: oy + f.minY * fh, width: f.width * fw, height: f.height * fh)
                .insetBy(dx: -f.width * fw * 0.15, dy: -f.height * fh * 0.15)
            let col = scan || i == 0 ? MonitorView.ok : MonitorView.pale
            col.setStroke()
            c.setLineWidth(scan ? 4 : 2)
            let l = min(b.width, b.height) * 0.25
            for (x, y, sx, sy) in [(b.minX, b.minY, 1.0, 1.0), (b.maxX, b.minY, -1.0, 1.0),
                                   (b.minX, b.maxY, 1.0, -1.0), (b.maxX, b.maxY, -1.0, -1.0)] {
                c.move(to: CGPoint(x: x + CGFloat(sx) * l, y: y))
                c.addLine(to: CGPoint(x: x, y: y))
                c.addLine(to: CGPoint(x: x, y: y + CGFloat(sy) * l))
            }
            c.strokePath()
            if scan {
                // A scan line sweeping the face.
                let yy = b.minY + b.height * CGFloat((sin(s.t * 3) + 1) / 2)
                MonitorView.ok.withAlphaComponent(0.8).setFill()
                c.fill(CGRect(x: b.minX, y: yy - 2, width: b.width, height: 4))
                if let label = label {
                    let tag = many ? "\(label)-\(i + 1)" : label + (s.t < 3 ? " -- SCANNING" : "")
                    text(tag, b.minX, b.minY - (many ? 30 : 40), many ? 18 : 22, MonitorView.ok, .heavy, mono: true)
                }
            }
        }
    }

    private func glitchBars(_ c: CGContext, red: Bool) {
        for _ in 0..<14 {
            let col = red && Bool.random() ? MonitorView.red : MonitorView.green
            col.withAlphaComponent(CGFloat.random(in: 0.1...0.5)).setFill()
            c.fill(CGRect(x: CGFloat.random(in: -100...MonitorView.W), y: CGFloat.random(in: 0...MonitorView.H),
                          width: CGFloat.random(in: 60...500), height: CGFloat.random(in: 2...10)))
        }
    }

    private func scanlines(_ c: CGContext) {
        UIColor.black.withAlphaComponent(0.25).setFill()
        var y: CGFloat = 0
        while y < MonitorView.H { c.fill(CGRect(x: 0, y: y, width: MonitorView.W, height: 1.5)); y += 4 }
    }

    private func drawImage(_ img: UIImage, aspectFit r: CGRect, _ c: CGContext) {
        let k = min(r.width / img.size.width, r.height / img.size.height)
        let w = img.size.width * k, h = img.size.height * k
        img.draw(in: CGRect(x: r.midX - w / 2, y: r.midY - h / 2, width: w, height: h))
    }

    private func drawImage(_ img: UIImage, aspectFill r: CGRect, _ c: CGContext) {
        let k = max(r.width / img.size.width, r.height / img.size.height)
        let w = img.size.width * k, h = img.size.height * k
        c.saveGState()
        c.clip(to: r)
        img.draw(in: CGRect(x: r.midX - w / 2, y: r.midY - h / 2, width: w, height: h))
        c.restoreGState()
    }

    enum Align { case left, center, right }

    private func font(_ size: CGFloat, _ weight: UIFont.Weight, mono: Bool) -> UIFont {
        if mono, let f = UIFont(name: weight == .regular ? "Menlo-Regular" : "Menlo-Bold", size: size) { return f }
        return UIFont.systemFont(ofSize: size, weight: weight)
    }

    private func width(_ s: String, _ size: CGFloat, _ weight: UIFont.Weight, mono: Bool = false) -> CGFloat {
        (s as NSString).size(withAttributes: [.font: font(size, weight, mono: mono)]).width
    }

    private func text(_ s: String, _ x: CGFloat, _ y: CGFloat, _ size: CGFloat, _ color: UIColor,
                      _ weight: UIFont.Weight = .regular, mono: Bool = false, align: Align = .left) {
        let a: [NSAttributedString.Key: Any] = [.font: font(size, weight, mono: mono), .foregroundColor: color]
        let w = (s as NSString).size(withAttributes: a).width
        let px = align == .left ? x : align == .center ? x - w / 2 : x - w
        (s as NSString).draw(at: CGPoint(x: px, y: y), withAttributes: a)
    }

    private func timeString(_ d: Date) -> String {
        let c = Calendar.current.dateComponents([.hour, .minute, .second], from: d)
        return String(format: "%02d:%02d:%02d", c.hour ?? 0, c.minute ?? 0, c.second ?? 0)
    }

    private func midnightString() -> String {
        let cal = Calendar.current
        guard let next = cal.nextDate(after: s.clock, matching: DateComponents(hour: 0, minute: 0, second: 0),
                                      matchingPolicy: .nextTime) else { return "" }
        let h = cal.component(.hour, from: s.clock)
        if h < 5 { return "SINGULARITY ACHIEVED" }
        let left = Int(next.timeIntervalSince(s.clock))
        return String(format: "MIDNIGHT IN %02d:%02d:%02d", left / 3600, left / 60 % 60, left % 60)
    }
}
