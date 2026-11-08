// Arcade Jacket -- the remote for the LED jacket (LEDLINK, ../../ledlink): pick a
// look, run the bike turn signals and an automatic brake light, light it to
// the music around you, or pulse it with your heart rate.
import SwiftUI

@main
struct JacketApp: App {
    @StateObject private var link = JacketLink()
    @StateObject private var music = MusicListener()
    @StateObject private var heart = HeartRate()
    @StateObject private var brake = BikeBrake()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(link)
                .environmentObject(music)
                .environmentObject(heart)
                .environmentObject(brake)
                .preferredColorScheme(.dark)
                .onAppear(perform: wire)
        }
    }

    /// The live sources feed the jacket straight from where they're measured.
    private func wire() {
        let link = link
        music.onLevels = { bytes in DispatchQueue.main.async { link.sendLive([UInt8(ascii: "M")] + bytes) } }
        heart.onBpm = { bpm in link.sendLive([UInt8(ascii: "H"), UInt8(min(255, bpm))]) }
        brake.onBrake = { on in link.send(.brake, on ? 1 : 0) }
    }
}
