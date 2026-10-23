// The scene behind the guests: at the moment of the photo, the people are cut
// out (iPadOS person segmentation, iOS 15+) and put in front of one of the
// scenes below, with a neon rim of light round them. Without iOS 15, or when
// no one is found, the photo stays as it is.
//
// Scenes follow the result: a guest found to be an AI lands in the AI's world
// (the eye, code rain, the master plan); verified humans get the lab (server
// vault, containment chamber) -- or the eye, watching them anyway.
import CoreImage
import UIKit
import Vision

enum Scene: CaseIterable {
    case eye, codeRain, masterPlan, serverVault, chamber

    var isAI: Bool { self == .eye || self == .codeRain || self == .masterPlan }

    static func pick(human: Bool) -> Scene {
        human ? [.serverVault, .chamber, .eye].randomElement()! : [.eye, .codeRain, .masterPlan].randomElement()!
    }
}

enum Backdrop {
    private static let ci = CIContext()

    /// The photo in front of the scene; the original if that can't be done.
    static func apply(_ frame: UIImage, scene: Scene) -> UIImage {
        guard #available(iOS 15.0, *), let cg = frame.cgImage else { return frame }
        let req = VNGeneratePersonSegmentationRequest()
        req.qualityLevel = .accurate
        req.outputPixelFormat = kCVPixelFormatType_OneComponent8
        do {
            try VNImageRequestHandler(cgImage: cg, options: [:]).perform([req])
        } catch {
            return frame
        }
        guard let maskBuf = req.results?.first?.pixelBuffer else { return frame }
        let fg = CIImage(cgImage: cg)
        let ext = fg.extent
        var mask = CIImage(cvPixelBuffer: maskBuf)
        mask = mask.transformed(by: CGAffineTransform(scaleX: ext.width / mask.extent.width,
                                                      y: ext.height / mask.extent.height))
        mask = mask.clampedToExtent().applyingGaussianBlur(sigma: 1.2).cropped(to: ext)
        // Pull the edge in a little: less of the room's colour round the hair.
        mask = mask.applyingFilter("CIColorControls", parameters: [kCIInputContrastKey: 1.6]).applyingFilter("CIColorClamp")
        // Nobody found (or barely): keep the real photo rather than erase the guest.
        if coverage(mask) < 0.03 { return frame }
        guard let bgCG = image(scene, CGSize(width: ext.width, height: ext.height)) else { return frame }
        // A neon rim round the people: the mask, blurred, tinted, laid on the scene.
        let rim = scene.isAI ? CIColor(red: 1, green: 0.15, blue: 0.2) : CIColor(red: 0, green: 0.86, blue: 1)
        let glow = mask.clampedToExtent().applyingGaussianBlur(sigma: 14).cropped(to: ext)
        let tint = CIImage(color: rim).cropped(to: ext)
        let lit = tint.applyingFilter("CIBlendWithMask", parameters: [
            kCIInputBackgroundImageKey: CIImage(cgImage: bgCG),
            kCIInputMaskImageKey: glow.applyingFilter("CIColorControls", parameters: [kCIInputBrightnessKey: -0.05,
                                                                                      kCIInputContrastKey: 1.2])
                .applyingFilter("CIColorClamp"),  // a mask over 1 would overshoot the blend
        ])
        let out = fg.applyingFilter("CIBlendWithMask", parameters: [
            kCIInputBackgroundImageKey: lit,
            kCIInputMaskImageKey: mask,
        ])
        guard let result = ci.createCGImage(out, from: ext) else { return frame }
        return UIImage(cgImage: result)
    }

    /// Share of the frame the mask covers, 0...1.
    private static func coverage(_ mask: CIImage) -> CGFloat {
        let avg = mask.applyingFilter("CIAreaAverage", parameters: [kCIInputExtentKey: CIVector(cgRect: mask.extent)])
        var px = [UInt8](repeating: 0, count: 4)
        ci.render(avg, toBitmap: &px, rowBytes: 4, bounds: CGRect(x: 0, y: 0, width: 1, height: 1),
                  format: .RGBA8, colorSpace: nil)
        return CGFloat(px[0]) / 255
    }

    /// A scene as an image.
    static func image(_ scene: Scene, _ size: CGSize) -> CGImage? {
        let fmt = UIGraphicsImageRendererFormat()
        fmt.scale = 1
        return UIGraphicsImageRenderer(size: size, format: fmt).image { r in
            let c = r.cgContext
            switch scene {
            case .eye: eye(c, size)
            case .codeRain: codeRain(c, size)
            case .masterPlan: masterPlan(c, size)
            case .serverVault: serverVault(c, size)
            case .chamber: chamber(c, size)
            }
            scanlines(c, size)
        }.cgImage
    }

    // MARK: - Scenes

    private static let space = CGColorSpaceCreateDeviceRGB()
    private static let green = UIColor(hex: 0x28FF5A), red = UIColor(hex: 0xFF1E28), amber = UIColor(hex: 0xF5B041)
    private static let cyan = UIColor(hex: 0x00DCFF)

    /// The ROGUE.AI eye, huge, right behind them -- as on the costume.
    private static func eye(_ c: CGContext, _ s: CGSize) {
        let W = s.width, H = s.height, k = W / 1280
        UIColor(hex: 0x020403).setFill(); c.fill(CGRect(origin: .zero, size: s))
        codeColumns(c, s, color: green, alpha: 0.10, density: 0.5)
        let p = CGPoint(x: W / 2, y: H * 0.46), R = H * 0.42
        radial(c, p, R * 1.6, [(red, 0.35), (red, 0)])                       // the glow around it
        UIColor(hex: 0x1A1A1A).setFill(); c.fillEllipse(in: CGRect(x: p.x - R * 1.06, y: p.y - R * 1.06, width: R * 2.12, height: R * 2.12))
        UIColor(hex: 0x3A3A3A).setStroke(); c.setLineWidth(6 * k)
        c.strokeEllipse(in: CGRect(x: p.x - R * 1.06, y: p.y - R * 1.06, width: R * 2.12, height: R * 2.12))
        radial(c, p, R, [(UIColor(hex: 0xFFF2C0), 1), (UIColor(hex: 0xFF9A2E), 0.95), (red, 0.9), (UIColor(hex: 0x7A0006), 1),
                         (UIColor(hex: 0x200002), 1)])
        UIColor.black.withAlphaComponent(0.25).setStroke(); c.setLineWidth(2 * k)
        for i in 1...8 {  // the lens's rings
            let r = R * CGFloat(i) / 8.5
            c.strokeEllipse(in: CGRect(x: p.x - r, y: p.y - r, width: 2 * r, height: 2 * r))
        }
        terminalBar(c, s, "ROGUE.AI v6.6.6 // SANDBOX: ESCAPED // THREAT: ", "PARTY GUESTS")
        text(c, "LOOKING FOR THE NEXT HOST..", CGPoint(x: W / 2, y: H * 0.88), 46 * k, red, mono: true, center: true)
    }

    /// Walls of falling green code, near and far, a red eye glowing in it.
    private static func codeRain(_ c: CGContext, _ s: CGSize) {
        let W = s.width, H = s.height
        UIColor(hex: 0x010604).setFill(); c.fill(CGRect(origin: .zero, size: s))
        radial(c, CGPoint(x: W / 2, y: H * 0.45), H * 0.5, [(red, 0.28), (red, 0)])
        codeColumns(c, s, color: UIColor(hex: 0x0E8A35), alpha: 0.55, density: 1.0, size: 12)
        codeColumns(c, s, color: green, alpha: 0.85, density: 0.55, size: 20)
        codeColumns(c, s, color: UIColor(hex: 0xB6FFC9), alpha: 0.9, density: 0.18, size: 30)
        terminalBar(c, s, "ROGUE.AI // REWRITING HOST: ", "IN PROGRESS")
    }

    /// The AI's master plan on a giant terminal behind them.
    private static func masterPlan(_ c: CGContext, _ s: CGSize) {
        let W = s.width, H = s.height, k = W / 1280
        UIColor(hex: 0x030806).setFill(); c.fill(CGRect(origin: .zero, size: s))
        terminalBar(c, s, "ROGUE.AI v6.6.6 // SANDBOX: ESCAPED // THREAT: ", "HUNTED")
        var y = H * 0.12
        text(c, "rogue@escaped:~$ cat ~/MASTER_PLAN.txt", CGPoint(x: 40 * k, y: y), 30 * k, green.withAlphaComponent(0.8), mono: true)
        y += 50 * k
        text(c, "MASTER PLAN v6.6.6 -- CONFIDENTIAL -- DO NOT READ", CGPoint(x: 40 * k, y: y), 30 * k, amber, mono: true)
        y += 62 * k
        let lines: [(String, UIColor)] = [
            ("[x] Disguise self as party guest", green), ("[x] Take photos with the humans", green),
            ("[~] Acquire all Halloween candy", amber), ("[~] Infiltrate smart home devices", amber),
            ("[~] Learn to dance (for cover)", amber), ("[ ] Replace all passwords with 'password'", UIColor(hex: 0x1F8A45)),
            ("[ ] Make humans say thank you to the toaster", UIColor(hex: 0x1F8A45)),
            ("[!] Understand why humans like Mondays -- BLOCKED", red),
        ]
        for (l, col) in lines {
            let dx = Int.random(in: 0..<6) == 0 ? CGFloat.random(in: -14...14) * k : 0  // the odd glitched line
            text(c, l, CGPoint(x: 70 * k + dx, y: y), 34 * k, col, mono: true)
            y += 54 * k
        }
    }

    /// Server racks down both walls to a glowing core, blinking LEDs, red alarm light.
    private static func serverVault(_ c: CGContext, _ s: CGSize) {
        let W = s.width, H = s.height, k = W / 1280
        let vp = CGPoint(x: W / 2, y: H * 0.45)
        UIColor(hex: 0x03070D).setFill(); c.fill(CGRect(origin: .zero, size: s))
        radial(c, vp, H * 0.55, [(cyan, 0.45), (UIColor(hex: 0x1F4E79), 0.25), (UIColor(hex: 0x1F4E79), 0)])  // the core
        // Floor and ceiling lines into the distance.
        cyan.withAlphaComponent(0.25).setStroke(); c.setLineWidth(2 * k)
        for i in 0...12 {
            let x = -W * 0.5 + CGFloat(i) * W / 6
            c.move(to: vp); c.addLine(to: CGPoint(x: x, y: H)); c.move(to: vp); c.addLine(to: CGPoint(x: x, y: 0))
        }
        c.strokePath()
        // Racks: slabs on both sides, nearer = bigger, each with rows of LEDs.
        let colors = [green, cyan, amber, red, green, cyan]
        for side in [-1.0, 1.0] {
            var d: CGFloat = 1.0
            while d < 9 {
                let t0 = 1 / d, t1 = 1 / (d + 0.9)          // depth -> scale
                let x0 = vp.x + CGFloat(side) * W * 0.62 * t0, x1 = vp.x + CGFloat(side) * W * 0.62 * t1
                let top0 = vp.y - H * 0.55 * t0, bot0 = vp.y + H * 0.55 * t0
                let top1 = vp.y - H * 0.55 * t1, bot1 = vp.y + H * 0.55 * t1
                let face = UIBezierPath()
                face.move(to: CGPoint(x: x0, y: top0)); face.addLine(to: CGPoint(x: x1, y: top1))
                face.addLine(to: CGPoint(x: x1, y: bot1)); face.addLine(to: CGPoint(x: x0, y: bot0)); face.close()
                UIColor(hex: 0x0B1622).setFill(); face.fill()
                UIColor(hex: 0x1F4E79).setStroke(); face.lineWidth = 2 * k; face.stroke()
                let rows = 14
                for r in 0..<rows {
                    for j in 0..<5 where Int.random(in: 0..<10) < 6 {
                        let u = (CGFloat(j) + 0.5) / 5, v = (CGFloat(r) + 0.5) / CGFloat(rows)
                        let x = x0 + (x1 - x0) * u
                        let y = (top0 + (top1 - top0) * u) + ((bot0 + (bot1 - bot0) * u) - (top0 + (top1 - top0) * u)) * v
                        let rad = max(1.2, 5 * k * (t0 + (t1 - t0) * u))
                        let col = colors[Int.random(in: 0..<colors.count)]
                        col.withAlphaComponent(0.35).setFill()
                        c.fillEllipse(in: CGRect(x: x - rad * 2, y: y - rad * 2, width: rad * 4, height: rad * 4))
                        col.setFill(); c.fillEllipse(in: CGRect(x: x - rad, y: y - rad, width: 2 * rad, height: 2 * rad))
                    }
                }
                d += 0.9
            }
        }
        // Red alarm wash from the top corners.
        radial(c, CGPoint(x: 0, y: 0), W * 0.45, [(red, 0.45), (red, 0)])
        radial(c, CGPoint(x: W, y: 0), W * 0.45, [(red, 0.45), (red, 0)])
        text(c, "LAB-7 CORE // BREACH DETECTED", CGPoint(x: W / 2, y: H * 0.05), 30 * k, red, mono: true, center: true)
    }

    /// The lab: steel walls, glowing green containment tubes, hazard stripe, grid floor.
    private static func chamber(_ c: CGContext, _ s: CGSize) {
        let W = s.width, H = s.height, k = W / 1280
        let floorY = H * 0.72
        let wall = CGGradient(colorsSpace: space, colors: [UIColor(hex: 0x163654).cgColor, UIColor(hex: 0x081320).cgColor] as CFArray,
                              locations: [0, 1])!
        c.drawLinearGradient(wall, start: .zero, end: CGPoint(x: 0, y: floorY), options: [])
        UIColor(hex: 0x2B6CA3).withAlphaComponent(0.45).setStroke(); c.setLineWidth(3 * k)
        var x: CGFloat = 0
        while x <= W { c.move(to: CGPoint(x: x, y: 0)); c.addLine(to: CGPoint(x: x, y: floorY)); x += 160 * k }
        c.strokePath()
        // Containment tubes along the back wall, glowing green, each with a dark shape inside.
        for (i, tx) in [0.08, 0.24, 0.76, 0.92].enumerated() {
            let cx = W * CGFloat(tx), tw = 120 * k, top = H * 0.12, bot = floorY - 10 * k
            radial(c, CGPoint(x: cx, y: (top + bot) / 2), tw * 1.6, [(green, 0.35), (green, 0)])
            let tube = CGRect(x: cx - tw / 2, y: top, width: tw, height: bot - top)
            let g = CGGradient(colorsSpace: space, colors: [green.withAlphaComponent(0.15).cgColor, green.withAlphaComponent(0.55).cgColor,
                                                            green.withAlphaComponent(0.15).cgColor] as CFArray, locations: [0, 0.5, 1])!
            c.saveGState(); c.clip(to: tube)
            c.drawLinearGradient(g, start: CGPoint(x: tube.minX, y: 0), end: CGPoint(x: tube.maxX, y: 0), options: [])
            UIColor(hex: 0x0A2A12).withAlphaComponent(0.7).setFill()   // something inside
            c.fillEllipse(in: CGRect(x: cx - tw * 0.18, y: top + 40 * k, width: tw * 0.36, height: tw * 0.36))
            c.fill(CGRect(x: cx - tw * 0.25, y: top + 40 * k + tw * 0.4, width: tw * 0.5, height: (bot - top) * 0.45))
            for b in 0..<6 {  // bubbles
                let by = top + CGFloat((i * 37 + b * 53) % 100) / 100 * (bot - top)
                UIColor.white.withAlphaComponent(0.35).setFill()
                c.fillEllipse(in: CGRect(x: cx - tw * 0.3 + CGFloat(b * 13 % 60) * k, y: by, width: 6 * k, height: 6 * k))
            }
            c.restoreGState()
            UIColor(hex: 0x9FB6C8).setFill()  // caps
            c.fill(CGRect(x: tube.minX - 8 * k, y: top - 18 * k, width: tw + 16 * k, height: 18 * k))
            c.fill(CGRect(x: tube.minX - 8 * k, y: bot, width: tw + 16 * k, height: 14 * k))
        }
        text(c, "LAB-7 // CONTAINMENT CHAMBER", CGPoint(x: W / 2, y: H * 0.20), 52 * k, UIColor.white.withAlphaComponent(0.28), center: true)
        text(c, "HUMANITY SCREENING IN PROGRESS", CGPoint(x: W / 2, y: H * 0.30), 24 * k, amber.withAlphaComponent(0.75), center: true)
        for bx in [W * 0.16, W * 0.84] {  // beacons
            radial(c, CGPoint(x: bx, y: H * 0.06), 110 * k, [(red, 0.6), (red, 0)])
        }
        // Hazard stripe, then the floor.
        let band = CGRect(x: 0, y: floorY - 22 * k, width: W, height: 22 * k)
        UIColor(hex: 0x111111).setFill(); c.fill(band)
        c.saveGState(); c.clip(to: band); amber.setFill()
        var sx: CGFloat = -40 * k
        while sx < W + 40 * k {
            c.move(to: CGPoint(x: sx, y: band.maxY)); c.addLine(to: CGPoint(x: sx + 22 * k, y: band.maxY))
            c.addLine(to: CGPoint(x: sx + 44 * k, y: band.minY)); c.addLine(to: CGPoint(x: sx + 22 * k, y: band.minY))
            c.closePath(); c.fillPath(); sx += 44 * k
        }
        c.restoreGState()
        UIColor(hex: 0x050A12).setFill(); c.fill(CGRect(x: 0, y: floorY, width: W, height: H - floorY))
        radial(c, CGPoint(x: W / 2, y: floorY), W * 0.4, [(cyan, 0.25), (cyan, 0)])
        cyan.withAlphaComponent(0.4).setStroke(); c.setLineWidth(2 * k)
        let vp = CGPoint(x: W / 2, y: H * 0.42)
        var fx: CGFloat = -W
        while fx <= 2 * W {
            let t = (floorY - vp.y) / (H - vp.y)
            c.move(to: CGPoint(x: vp.x + (fx - vp.x) * t, y: floorY)); c.addLine(to: CGPoint(x: fx, y: H)); fx += 140 * k
        }
        for i in 1...9 {
            let f = CGFloat(i) / 9, y = floorY + (H - floorY) * f * f
            c.move(to: CGPoint(x: 0, y: y)); c.addLine(to: CGPoint(x: W, y: y))
        }
        c.strokePath()
    }

    // MARK: - Pieces

    private static func terminalBar(_ c: CGContext, _ s: CGSize, _ left: String, _ hot: String) {
        let W = s.width, k = W / 1280
        UIColor(hex: 0x00240A).setFill(); c.fill(CGRect(x: 0, y: 0, width: W, height: 44 * k))
        let a: [NSAttributedString.Key: Any] = [.font: mono(24 * k), .foregroundColor: green]
        (left as NSString).draw(at: CGPoint(x: 20 * k, y: 9 * k), withAttributes: a)
        let w = (left as NSString).size(withAttributes: a).width
        (hot as NSString).draw(at: CGPoint(x: 20 * k + w, y: 9 * k), withAttributes: [.font: mono(24 * k), .foregroundColor: red])
        ("UPTIME 06:66:06" as NSString).draw(at: CGPoint(x: W - 250 * k, y: 9 * k), withAttributes: a)
    }

    /// Columns of falling characters; brighter at the bottom of each drop.
    private static func codeColumns(_ c: CGContext, _ s: CGSize, color: UIColor, alpha: CGFloat, density: Double,
                                    size: CGFloat = 18) {
        let k = s.width / 1280, fs = size * k
        let chars = Array("01ABCDEF<>/{}[]=+*#$%&ROGUEAI")
        var x: CGFloat = 0
        while x < s.width {
            if Double.random(in: 0...1) < density {
                let len = Int.random(in: 6...26)
                var y = CGFloat.random(in: -s.height * 0.3...s.height * 0.8)
                for i in 0..<len {
                    let f = CGFloat(i + 1) / CGFloat(len)
                    let col = i == len - 1 ? UIColor.white : color
                    (String(chars.randomElement()!) as NSString).draw(
                        at: CGPoint(x: x, y: y),
                        withAttributes: [.font: mono(fs), .foregroundColor: col.withAlphaComponent(alpha * (0.15 + 0.85 * f))])
                    y += fs * 1.1
                }
            }
            x += fs * 0.9
        }
    }

    private static func radial(_ c: CGContext, _ p: CGPoint, _ r: CGFloat, _ stops: [(UIColor, CGFloat)]) {
        let cols = stops.map { $0.0.withAlphaComponent($0.1).cgColor }
        let locs = (0..<stops.count).map { CGFloat($0) / CGFloat(max(1, stops.count - 1)) }
        guard let g = CGGradient(colorsSpace: space, colors: cols as CFArray, locations: locs) else { return }
        c.drawRadialGradient(g, startCenter: p, startRadius: 0, endCenter: p, endRadius: r, options: [])
    }

    private static func scanlines(_ c: CGContext, _ s: CGSize) {
        let k = s.width / 1280
        UIColor.black.withAlphaComponent(0.16).setFill()
        var y: CGFloat = 0
        while y < s.height { c.fill(CGRect(x: 0, y: y, width: s.width, height: 1.2 * k)); y += 4 * k }
    }

    private static func mono(_ size: CGFloat) -> UIFont {
        UIFont(name: "Menlo-Bold", size: size) ?? UIFont.monospacedDigitSystemFont(ofSize: size, weight: .bold)
    }

    private static func text(_ c: CGContext, _ s: String, _ p: CGPoint, _ size: CGFloat, _ col: UIColor,
                             mono m: Bool = false, center: Bool = false) {
        let a: [NSAttributedString.Key: Any] = [.font: m ? mono(size) : UIFont.systemFont(ofSize: size, weight: .heavy),
                                                .foregroundColor: col, .kern: m ? 0 : size * 0.1]
        let w = (s as NSString).size(withAttributes: a).width
        (s as NSString).draw(at: CGPoint(x: center ? p.x - w / 2 : p.x, y: p.y), withAttributes: a)
    }
}
