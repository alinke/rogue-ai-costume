// The lab behind the guests: at the moment of the photo, the people are cut
// out (iPadOS person segmentation, iOS 15+) and put in a LAB-7 containment
// chamber. Without iOS 15, or when no one is found, the photo stays as it is.
import CoreImage
import UIKit
import Vision

enum Backdrop {
    private static let ci = CIContext()
    private static var cached: (CGSize, CGImage)?

    /// The photo with its background replaced; the original if that can't be done.
    static func apply(_ frame: UIImage) -> UIImage {
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
        mask = mask.applyingFilter("CIColorControls", parameters: [kCIInputContrastKey: 1.6])
        // Nobody found (or barely): keep the real photo rather than erase the guest.
        if coverage(mask) < 0.03 { return frame }
        guard let bgImage = backdrop(CGSize(width: ext.width, height: ext.height)) else { return frame }
        let out = fg.applyingFilter("CIBlendWithMask", parameters: [
            kCIInputBackgroundImageKey: CIImage(cgImage: bgImage),
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

    /// The containment chamber, drawn once per size.
    static func backdrop(_ size: CGSize) -> CGImage? {
        if let c = cached, c.0 == size { return c.1 }
        let fmt = UIGraphicsImageRendererFormat()
        fmt.scale = 1
        let img = UIGraphicsImageRenderer(size: size, format: fmt).image { r in
            draw(r.cgContext, size)
        }
        guard let cg = img.cgImage else { return nil }
        cached = (size, cg)
        return cg
    }

    private static func draw(_ c: CGContext, _ s: CGSize) {
        let W = s.width, H = s.height, k = W / 1280
        let floorY = H * 0.70
        // Wall: deep navy, darker toward the floor.
        let space = CGColorSpaceCreateDeviceRGB()
        let wall = CGGradient(colorsSpace: space, colors: [UIColor(hex: 0x10263C).cgColor, UIColor(hex: 0x060E18).cgColor] as CFArray,
                              locations: [0, 1])!
        c.drawLinearGradient(wall, start: .zero, end: CGPoint(x: 0, y: floorY), options: [])
        // Wall panels and rails.
        UIColor(hex: 0x1F4E79).withAlphaComponent(0.55).setStroke()
        c.setLineWidth(3 * k)
        var x: CGFloat = 0
        while x <= W { c.move(to: CGPoint(x: x, y: 0)); c.addLine(to: CGPoint(x: x, y: floorY)); x += 160 * k }
        for y in [H * 0.17, H * 0.60] { c.move(to: CGPoint(x: 0, y: y)); c.addLine(to: CGPoint(x: W, y: y)) }
        c.strokePath()
        // Red warning beacons, glowing.
        for bx in [W * 0.09, W * 0.91] {
            let glow = CGGradient(colorsSpace: space, colors: [UIColor(hex: 0xFF2030).withAlphaComponent(0.55).cgColor,
                                                              UIColor(hex: 0xFF2030).withAlphaComponent(0).cgColor] as CFArray,
                                  locations: [0, 1])!
            let p = CGPoint(x: bx, y: H * 0.11)
            c.drawRadialGradient(glow, startCenter: p, startRadius: 0, endCenter: p, endRadius: 130 * k, options: [])
            UIColor(hex: 0xFF4050).setFill()
            c.fillEllipse(in: CGRect(x: p.x - 14 * k, y: p.y - 14 * k, width: 28 * k, height: 28 * k))
        }
        // Lettering on the wall.
        func text(_ t: String, _ y: CGFloat, _ size: CGFloat, _ col: UIColor) {
            let a: [NSAttributedString.Key: Any] = [.font: UIFont.systemFont(ofSize: size * k, weight: .heavy),
                                                    .foregroundColor: col, .kern: 6 * k]
            let w = (t as NSString).size(withAttributes: a).width
            (t as NSString).draw(at: CGPoint(x: (W - w) / 2, y: y), withAttributes: a)
        }
        text("LAB-7 // CONTAINMENT CHAMBER", H * 0.21, 58, UIColor.white.withAlphaComponent(0.16))
        text("HUMANITY SCREENING IN PROGRESS", H * 0.33, 26, UIColor(hex: 0xF5B041).withAlphaComponent(0.45))
        // Hazard stripe where wall meets floor.
        let band = CGRect(x: 0, y: floorY - 22 * k, width: W, height: 22 * k)
        UIColor(hex: 0x111111).setFill(); c.fill(band)
        c.saveGState()
        c.clip(to: band)
        UIColor(hex: 0xF5B041).setFill()
        var sx: CGFloat = -40 * k
        while sx < W + 40 * k {
            c.move(to: CGPoint(x: sx, y: band.maxY)); c.addLine(to: CGPoint(x: sx + 22 * k, y: band.maxY))
            c.addLine(to: CGPoint(x: sx + 44 * k, y: band.minY)); c.addLine(to: CGPoint(x: sx + 22 * k, y: band.minY))
            c.closePath(); c.fillPath()
            sx += 44 * k
        }
        c.restoreGState()
        // Floor: dark, with a glowing grid running to a vanishing point.
        UIColor(hex: 0x04080E).setFill(); c.fill(CGRect(x: 0, y: floorY, width: W, height: H - floorY))
        UIColor(hex: 0x00DCFF).withAlphaComponent(0.30).setStroke()
        c.setLineWidth(2 * k)
        let vp = CGPoint(x: W / 2, y: H * 0.42)
        var fx: CGFloat = -W
        while fx <= 2 * W {
            let t = (floorY - vp.y) / (H - vp.y)  // where the line enters the floor
            c.move(to: CGPoint(x: vp.x + (fx - vp.x) * t, y: floorY)); c.addLine(to: CGPoint(x: fx, y: H))
            fx += 140 * k
        }
        for i in 1...9 {  // horizontal floor lines, closer together toward the back
            let f = CGFloat(i) / 9
            let y = floorY + (H - floorY) * f * f
            c.move(to: CGPoint(x: 0, y: y)); c.addLine(to: CGPoint(x: W, y: y))
        }
        c.strokePath()
        // Fine scanlines over everything.
        UIColor.black.withAlphaComponent(0.18).setFill()
        var y: CGFloat = 0
        while y < H { c.fill(CGRect(x: 0, y: y, width: W, height: 1.2 * k)); y += 4 * k }
    }
}
