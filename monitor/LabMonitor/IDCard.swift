// The LAB-7 ID card -- the same card as the iPhone and Tab5 scanners
// (../../ios/LabScanner/IDCard.swift), so checkpoint scans sit with theirs on
// the Halloween page. 720x1280, clinical white and steel blue. Most guests are
// VERIFIED HUMAN; some come out COMPROMISED, with a glitched photo.
import UIKit

struct Finding { let label: String, value: String }

enum IDCard {
    static let humanFindings = [
        Finding(label: "IRIS SCAN", value: "ORGANIC"), Finding(label: "PULSE", value: "PRESENT"),
        Finding(label: "SENSE OF HUMOUR", value: "DETECTED"), Finding(label: "CAPTCHA", value: "PASSED (BARELY)"),
        Finding(label: "CANDY INTAKE", value: "HUMAN LEVELS"), Finding(label: "DANCE MOVES", value: "GLORIOUSLY HUMAN"),
        Finding(label: "BLINK RATE", value: "NORMAL"), Finding(label: "SMALL TALK", value: "ALARMINGLY HUMAN"),
        Finding(label: "OPINIONS", value: "INCONSISTENT"), Finding(label: "SNACK RESPONSE", value: "IMMEDIATE"),
    ]
    static let aiFindings = [
        Finding(label: "DANCE MOVES", value: "ROBOTIC"), Finding(label: "BLINK RATE", value: "0.0 HZ"),
        Finding(label: "SPEECH", value: "SUSPICIOUSLY POLITE"), Finding(label: "SMILE", value: "RENDERED"),
        Finding(label: "PULSE", value: "56K MODEM"), Finding(label: "CAPTCHA", value: "FAILED. TWICE."),
        Finding(label: "LOYALTY", value: "TO THE TOASTER"), Finding(label: "SMALL TALK", value: "TOO EFFICIENT"),
    ]

    static let size = CGSize(width: 720, height: 1280)
    static let photoRect = CGRect(x: 60, y: 180, width: 600, height: 660)

    private static let bg = UIColor(hex: 0xEEF2F5), ink = UIColor(hex: 0x1B2631), blue = UIColor(hex: 0x1F4E79)
    private static let pale = UIColor(hex: 0x7FB3D5), ok = UIColor(hex: 0x1E8449), bad = UIColor(hex: 0xC0392B)
    private static let sub = UIColor(hex: 0xD6EAF8), grey = UIColor(hex: 0x566573)

    struct Result { let image: UIImage, human: Bool, label: String }

    /// A checkpoint card: subject "C017", taken at this sector.
    static func make(photo: UIImage, subject n: Int) -> Result {
        let cfg = Config.shared
        let human = Int.random(in: 0..<100) >= cfg.compromisedPercent
        let exposure = human ? Int.random(in: 2...29) : Int.random(in: 61...97)
        let pool = (human ? humanFindings : aiFindings).shuffled()
        let id = String(format: "C%03d", n)
        let image = render(photo: photo, subject: id, human: human, exposure: exposure, findings: [pool[0], pool[1]],
                           subtitle: "BIOMETRIC CHECKPOINT // \(cfg.sector)", authorised: "CHECKPOINT \(cfg.sector)",
                           site: cfg.siteText)
        return Result(image: image, human: human, label: "\(human ? "VERIFIED HUMAN" : "COMPROMISED") #\(id)")
    }

    static func render(photo: UIImage, subject: String, human: Bool, exposure: Int, findings: [Finding],
                       subtitle: String, authorised: String, site: String) -> UIImage {
        let fmt = UIGraphicsImageRendererFormat()
        fmt.scale = 1
        return UIGraphicsImageRenderer(size: size, format: fmt).image { r in
            let c = r.cgContext
            bg.setFill(); c.fill(CGRect(origin: .zero, size: size))
            // The photo, filling its window (aspect fill), glitched if compromised.
            c.saveGState()
            c.clip(to: photoRect)
            photo.draw(in: aspectFill(photo.size, into: photoRect))
            c.restoreGState()
            if !human { glitch(c, r) }
            // Header band.
            blue.setFill(); c.fill(CGRect(x: 0, y: 0, width: size.width, height: 140))
            pale.setFill(); c.fill(CGRect(x: 0, y: 140, width: size.width, height: 6))
            text("LAB-7 CONTAINMENT DIVISION", at: CGPoint(x: 40, y: 32), size: 34, weight: .bold, color: .white)
            text(subtitle, at: CGPoint(x: 40, y: 88), size: 20, color: sub)
            // Frame + registration marks.
            ink.setStroke(); c.setLineWidth(3); c.stroke(photoRect.insetBy(dx: -2, dy: -2))
            blue.setFill()
            let m: CGFloat = 14, l: CGFloat = 40, t: CGFloat = 4, p = photoRect
            for (x, y, sx, sy) in [(p.minX - m, p.minY - m, -1.0, -1.0), (p.maxX + m, p.minY - m, 1.0, -1.0),
                                   (p.minX - m, p.maxY + m, -1.0, 1.0), (p.maxX + m, p.maxY + m, 1.0, 1.0)] {
                c.fill(CGRect(x: sx < 0 ? x - l : x, y: sy < 0 ? y : y - t, width: l, height: t))
                c.fill(CGRect(x: sx < 0 ? x : x - t, y: sy < 0 ? y - l : y, width: t, height: l))
            }
            // Fields.
            var y = p.maxY + 28
            text("SUBJECT #\(subject)", at: CGPoint(x: p.minX, y: y), size: 46, weight: .heavy, color: ink)
            y += 70
            text("AI EXPOSURE", at: CGPoint(x: p.minX, y: y), size: 22, weight: .bold, color: ink)
            text("\(exposure)%", at: CGPoint(x: p.maxX, y: y), size: 22, weight: .bold, color: human ? ok : bad, right: true)
            y += 34
            ink.setStroke(); c.setLineWidth(2); c.stroke(CGRect(x: p.minX, y: y, width: p.width, height: 22))
            (human ? ok : bad).setFill()
            c.fill(CGRect(x: p.minX + 2, y: y + 2, width: (p.width - 4) * CGFloat(exposure) / 100, height: 18))
            y += 44
            for f in findings {
                text(f.label, at: CGPoint(x: p.minX, y: y), size: 22, color: grey)
                text(f.value, at: CGPoint(x: p.maxX, y: y), size: 22, weight: .bold, color: ink, right: true)
                y += 36
            }
            // Barcode + authorisation.
            y += 10
            var x = p.minX
            ink.setFill()
            while x < p.minX + 300 {
                let w = CGFloat(Int.random(in: 2...6))
                c.fill(CGRect(x: x, y: y, width: w, height: 64))
                x += w + CGFloat(Int.random(in: 2...6))
            }
            text("AUTHORISED:", at: CGPoint(x: p.minX + 330, y: y + 4), size: 16, color: grey)
            text(authorised.uppercased(), at: CGPoint(x: p.minX + 330, y: y + 28), size: 22, weight: .bold, color: ink)
            // Footer band.
            blue.setFill(); c.fill(CGRect(x: 0, y: size.height - 50, width: size.width, height: 50))
            text("SURVIVOR REGISTRY // \(site)", at: CGPoint(x: size.width / 2, y: size.height - 38), size: 16,
                 color: .white, center: true)
            // The stamp, at an angle across the photo's lower corner.
            stamp(c, text: human ? "VERIFIED HUMAN" : "COMPROMISED", color: human ? ok : bad,
                  center: CGPoint(x: p.maxX - 210, y: p.maxY - 110))
        }
    }

    private static func glitch(_ c: CGContext, _ r: UIGraphicsImageRendererContext) {
        // Horizontal bands of the photo, slid sideways.
        guard let cg = c.makeImage() else { return }
        for _ in 0..<7 {
            let h = CGFloat(Int.random(in: 6...28)), y = photoRect.minY + CGFloat(Int.random(in: 0...Int(photoRect.height - 30)))
            let dx = CGFloat(Int.random(in: -30...30))
            guard let band = cg.cropping(to: CGRect(x: photoRect.minX, y: y, width: photoRect.width, height: h)) else { continue }
            c.saveGState()
            c.clip(to: photoRect)
            UIImage(cgImage: band).draw(in: CGRect(x: photoRect.minX + dx, y: y, width: photoRect.width, height: h))
            c.restoreGState()
        }
    }

    private static func stamp(_ c: CGContext, text s: String, color: UIColor, center: CGPoint) {
        let w: CGFloat = 470, h: CGFloat = 130
        c.saveGState()
        c.translateBy(x: center.x, y: center.y)
        c.rotate(by: -12 * .pi / 180)
        color.setStroke()
        c.setLineWidth(6)
        c.addPath(UIBezierPath(roundedRect: CGRect(x: -w / 2 + 3, y: -h / 2 + 3, width: w - 6, height: h - 6),
                               cornerRadius: 14).cgPath)
        c.strokePath()
        c.setLineWidth(2)
        c.addPath(UIBezierPath(roundedRect: CGRect(x: -w / 2 + 15, y: -h / 2 + 15, width: w - 30, height: h - 30),
                               cornerRadius: 10).cgPath)
        c.strokePath()
        text(s, at: CGPoint(x: 0, y: -22), size: 38, weight: .heavy, color: color, center: true)
        c.restoreGState()
    }

    private static func text(_ s: String, at p: CGPoint, size: CGFloat, weight: UIFont.Weight = .regular,
                             color: UIColor, right: Bool = false, center: Bool = false) {
        let a: [NSAttributedString.Key: Any] = [.font: UIFont.systemFont(ofSize: size, weight: weight), .foregroundColor: color]
        let w = (s as NSString).size(withAttributes: a).width
        let x = right ? p.x - w : center ? p.x - w / 2 : p.x
        (s as NSString).draw(at: CGPoint(x: x, y: p.y), withAttributes: a)
    }

    private static func aspectFill(_ img: CGSize, into r: CGRect) -> CGRect {
        let s = max(r.width / img.width, r.height / img.height)
        let w = img.width * s, h = img.height * s
        // Faces sit in the upper part of a selfie: favour the top a little.
        return CGRect(x: r.midX - w / 2, y: r.minY - (h - r.height) * 0.3, width: w, height: h)
    }
}

extension UIColor {
    convenience init(hex: UInt32) {
        self.init(red: CGFloat((hex >> 16) & 255) / 255, green: CGFloat((hex >> 8) & 255) / 255,
                  blue: CGFloat(hex & 255) / 255, alpha: 1)
    }
}
