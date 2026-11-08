// Demo mode: with no controller connected, a drawn jacket shows the look --
// run by the jacket's own looks code (JacketPreview.cpp), so every button in
// the app does what it would on the real thing -- and "What you need" says
// what it takes to light a real one.
import SwiftUI

/// The banner above the tabs while no controller is connected.
struct DemoBanner: View {
    @State private var help = false

    var body: some View {
        VStack(spacing: 8) {
            HStack(spacing: 8) {
                Text("DEMO").font(.caption2.weight(.black).monospaced())
                    .padding(.horizontal, 6).padding(.vertical, 2)
                    .background(Capsule().fill(Color.orange))
                    .foregroundStyle(.black)
                Text("No jacket connected: a preview").font(.caption).foregroundStyle(.secondary)
                Spacer()
                Button("What you need") { help = true }.font(.caption.weight(.semibold))
            }
            JacketPreviewView().frame(height: 200)
        }
        .padding(.horizontal, 16)
        .padding(.bottom, 8)
        .sheet(isPresented: $help) { WhatYouNeed() }
    }
}

/// The model jacket, front view, its LEDs lit by the looks code every frame.
struct JacketPreviewView: View {
    @EnvironmentObject var link: JacketLink
    private let model = PreviewModel.shared

    var body: some View {
        TimelineView(.animation) { tl in
            Canvas { ctx, size in
                let colors = model.frame(at: tl.date)
                let side = min(size.width, size.height)
                let origin = CGPoint(x: (size.width - side) / 2, y: (size.height - side) / 2)
                func at(_ p: CGPoint) -> CGPoint { CGPoint(x: origin.x + p.x * side, y: origin.y + p.y * side) }

                // The jacket.
                var body = Path()
                body.addLines(PreviewModel.silhouette.map(at))
                body.closeSubpath()
                ctx.fill(body, with: .color(Color.white.opacity(0.07)))
                ctx.stroke(body, with: .color(Color.white.opacity(0.18)), lineWidth: 1)

                // The LEDs: a soft glow, then the dots.
                let k = 0.6 + 0.4 * Double(link.status.bright) / 100
                let r = max(1.5, side / 190)
                var glow = ctx
                glow.addFilter(.blur(radius: r * 2.5))
                for (i, p) in model.points.enumerated() {
                    let c = colors[i]
                    let q = at(p)
                    guard c != 0 else {  // an unlit LED: just the strip
                        ctx.fill(Path(ellipseIn: CGRect(x: q.x - r / 2, y: q.y - r / 2, width: r, height: r)), with: .color(Color.white.opacity(0.08)))
                        continue
                    }
                    let color = Color(rgb: c).opacity(k)
                    glow.fill(Path(ellipseIn: CGRect(x: q.x - r * 2.5, y: q.y - r * 2.5, width: r * 5, height: r * 5)), with: .color(color))
                    ctx.fill(Path(ellipseIn: CGRect(x: q.x - r, y: q.y - r, width: r * 2, height: r * 2)), with: .color(color))
                }
            }
        }
        .accessibilityLabel("Preview of the jacket showing \(Look.of(link.status.look).name)")
    }
}

/// Where the model's LEDs sit on the drawing (0...1, front view: the wearer's
/// right is on the left), and the buffer the looks code draws into.
final class PreviewModel {
    static let shared = PreviewModel()

    let points: [CGPoint]
    private var buffer: [UInt32]

    // Hem up to the collar (the wearer's right edge of the zipper), and
    // shoulder down to the cuff (their right sleeve); the left side mirrors them.
    private static let front: [CGPoint] = [CGPoint(x: 0.47, y: 0.96), CGPoint(x: 0.47, y: 0.34), CGPoint(x: 0.44, y: 0.2), CGPoint(x: 0.39, y: 0.09)]
    private static let sleeve: [CGPoint] = [CGPoint(x: 0.29, y: 0.13), CGPoint(x: 0.2, y: 0.42), CGPoint(x: 0.11, y: 0.86)]
    static let silhouette: [CGPoint] = [
        CGPoint(x: 0.38, y: 0.07), CGPoint(x: 0.27, y: 0.11), CGPoint(x: 0.22, y: 0.2), CGPoint(x: 0.12, y: 0.5),
        CGPoint(x: 0.04, y: 0.86), CGPoint(x: 0.16, y: 0.9), CGPoint(x: 0.24, y: 0.56), CGPoint(x: 0.27, y: 0.4),
        CGPoint(x: 0.28, y: 0.98), CGPoint(x: 0.72, y: 0.98), CGPoint(x: 0.73, y: 0.4), CGPoint(x: 0.76, y: 0.56),
        CGPoint(x: 0.84, y: 0.9), CGPoint(x: 0.96, y: 0.86), CGPoint(x: 0.88, y: 0.5), CGPoint(x: 0.78, y: 0.2),
        CGPoint(x: 0.73, y: 0.11), CGPoint(x: 0.62, y: 0.07), CGPoint(x: 0.5, y: 0.15),
    ]

    private init() {
        let n = Int(jp_count())
        buffer = [UInt32](repeating: 0, count: n)
        points = (0..<n).map { i in
            var part: Int32 = 0, side: Int32 = 0, t: Float = 0
            jp_pixel(Int32(i), &part, &side, &t)
            let p = PreviewModel.along(part == 1 ? PreviewModel.sleeve : PreviewModel.front, CGFloat(t))
            return side < 0 ? CGPoint(x: 1 - p.x, y: p.y) : p  // the wearer's left is on the viewer's right
        }
    }

    /// The LEDs now (the date only makes each animation frame draw anew).
    func frame(at _: Date) -> [UInt32] {
        buffer.withUnsafeMutableBufferPointer { jp_render($0.baseAddress) }
        return buffer
    }

    /// The point a fraction t (0...1) of the way along a polyline.
    private static func along(_ pts: [CGPoint], _ t: CGFloat) -> CGPoint {
        let lens = zip(pts, pts.dropFirst()).map { hypot($1.x - $0.x, $1.y - $0.y) }
        var d = min(max(t, 0), 1) * lens.reduce(0, +)
        for (k, len) in lens.enumerated() {
            if d <= len || k == lens.count - 1 {
                let f = len > 0 ? min(d / len, 1) : 0
                return CGPoint(x: pts[k].x + (pts[k + 1].x - pts[k].x) * f, y: pts[k].y + (pts[k + 1].y - pts[k].y) * f)
            }
            d -= len
        }
        return pts.last ?? .zero
    }
}

/// What it takes to light a real jacket.
struct WhatYouNeed: View {
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Text("Arcade Jacket is the remote for a DIY LED jacket: you wire addressable LED strips into a jacket and drive them with a small Wi-Fi/Bluetooth LED controller. Until one is connected, the app runs this preview.")
                        .font(.subheadline)
                }
                Section("You need") {
                    item("tshirt", "A jacket with LED strips", "Addressable strips (WS2811 or WS2812B) along the front and down the sleeves, wired one after another into a single chain.")
                    item("cpu", "A Gledopto ESP32 LED controller", "The small ESP32 controllers sold for WLED, powered from a USB power bank.")
                    item("arrow.down.circle", "The free LEDLINK firmware", "Open source; install it on the controller from the web flasher (it replaces WLED). A controller still running WLED won't show up in the app.")
                    item("antenna.radiowaves.left.and.right", "Then just open the app", "It finds the controller over Bluetooth by itself, no pairing. Tell it how your strips are laid out in Settings.")
                }
                Section {
                    Link(destination: URL(string: "https://alinke.github.io/rogue-ai-costume/")!) {
                        Label("Web flasher and build guide", systemImage: "safari")
                    }
                    Link(destination: URL(string: "https://github.com/alinke/rogue-ai-costume")!) {
                        Label("Source code (GitHub)", systemImage: "chevron.left.forwardslash.chevron.right")
                    }
                }
            }
            .navigationTitle("What you need")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button("Done") { dismiss() } } }
        }
    }

    private func item(_ symbol: String, _ title: String, _ text: String) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Label(title, systemImage: symbol).font(.headline)
            Text(text).font(.subheadline).foregroundStyle(.secondary)
        }
        .padding(.vertical, 2)
    }
}
