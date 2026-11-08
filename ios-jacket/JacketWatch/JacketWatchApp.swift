// Arcade Jacket on the Apple Watch: a live heart rate for the jacket's HEARTBEAT look.
// An Apple Watch doesn't share its heart rate over Bluetooth like a chest strap
// does, so this runs a workout session (the watch then measures continuously)
// and sends each reading to the Arcade Jacket app on the iPhone, which passes it to
// the jacket. The workout isn't saved to Health.
import SwiftUI

@main
struct JacketWatchApp: App {
    @StateObject private var heart = WatchHeart()

    var body: some Scene {
        WindowGroup {
            WatchView().environmentObject(heart)
        }
    }
}

struct WatchView: View {
    @EnvironmentObject var heart: WatchHeart

    var body: some View {
        VStack(spacing: 8) {
            Text("ARCADE JACKET").font(.system(.headline, design: .monospaced).weight(.black)).foregroundStyle(.cyan)
            HStack(alignment: .firstTextBaseline, spacing: 4) {
                Image(systemName: "heart.fill").foregroundStyle(.red)
                Text(heart.bpm > 0 ? "\(heart.bpm)" : "--").font(.system(size: 44, weight: .bold).monospacedDigit())
                Text("BPM").font(.caption2).foregroundStyle(.secondary)
            }
            Text(heart.running ? (heart.phoneReachable ? "Sending to iPhone" : "iPhone not reachable") : "Pulse the jacket with your heart rate")
                .font(.caption2).foregroundStyle(.secondary).multilineTextAlignment(.center)
                .lineLimit(2).fixedSize(horizontal: false, vertical: true)
            Button(heart.running ? "Stop" : "Start") {
                heart.running ? heart.stop() : heart.start()
            }
            .tint(heart.running ? .gray : .red)
            if let e = heart.error { Text(e).font(.caption2).foregroundStyle(.orange) }
        }
    }
}
