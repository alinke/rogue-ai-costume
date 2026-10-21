// The scanner's tones, synthesised like ../../scanner/src/sound_impl.h:
// LOCK (beep-beep, target locked), ZAP (the CONTAIN beam), FAIL (containment
// failed), BEEP (still recharging / sound on). Plus a matching haptic.
import AVFoundation
import UIKit

enum Tone { case lock, zap, fail, beep }

final class Sound {
    static let shared = Sound()

    private let engine = AVAudioEngine()
    private let player = AVAudioPlayerNode()
    private let format = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 1)!
    private var buffers: [Tone: AVAudioPCMBuffer] = [:]

    private init() {
        try? AVAudioSession.sharedInstance().setCategory(.playback, options: [.mixWithOthers])
        try? AVAudioSession.sharedInstance().setActive(true)
        engine.attach(player)
        engine.connect(player, to: engine.mainMixerNode, format: format)
        try? engine.start()
        for t in [Tone.lock, .zap, .fail, .beep] { buffers[t] = synth(t) }
    }

    func play(_ t: Tone) {
        haptic(t)
        guard let b = buffers[t] else { return }
        if !engine.isRunning { try? engine.start() }
        player.stop()
        player.scheduleBuffer(b, at: nil)
        player.play()
    }

    private func haptic(_ t: Tone) {
        switch t {
        case .lock: UINotificationFeedbackGenerator().notificationOccurred(.warning)
        case .zap: UIImpactFeedbackGenerator(style: .heavy).impactOccurred()
        case .fail: UINotificationFeedbackGenerator().notificationOccurred(.error)
        case .beep: UIImpactFeedbackGenerator(style: .light).impactOccurred()
        }
    }

    private func synth(_ t: Tone) -> AVAudioPCMBuffer {
        let rate = 44100.0
        let ms: Double = t == .zap ? 700 : t == .fail ? 650 : t == .lock ? 260 : 60
        let n = Int(rate * ms / 1000)
        let buf = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(n))!
        buf.frameLength = AVAudioFrameCount(n)
        let out = buf.floatChannelData![0]
        var phase = 0.0
        let drive = t == .fail ? 5.0 : 1.5
        for i in 0..<n {
            let x = Double(i) / Double(n)
            var f = 1000.0
            switch t {
            case .zap: f = 2400 * pow(0.08, x)                          // falling "pew"
            case .fail: f = 140                                          // buzzer
            case .lock: f = (i / Int(rate / 16)) % 2 == 1 ? 0 : 1600     // beep-beep
            case .beep: f = 1200
            }
            var s = 0.0
            if f > 0 {
                phase += 2 * .pi * f / rate
                if phase > 2 * .pi { phase -= 2 * .pi }
                s = tanh(drive * sin(phase)) / tanh(drive) * 0.6 * (1 - x * 0.5)
            }
            out[i] = Float(s * 0.8)
        }
        return buf
    }
}
