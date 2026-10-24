// The scanner's tones, synthesised like ../../scanner/src/sound_impl.h:
// LOCK (beep-beep, target locked), ZAP (the CONTAIN beam), FAIL (containment
// failed), BEEP (still recharging / sound on). Plus a matching haptic.
import AVFoundation
import UIKit

enum Tone { case lock, zap, fail, beep, tick }   // tick: the Geiger counter (no haptic of its own)

final class Sound {
    static let shared = Sound()

    private let engine = AVAudioEngine()
    private let player = AVAudioPlayerNode()
    private let format = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 1)!
    private var buffers: [Tone: AVAudioPCMBuffer] = [:]

    private init() {
        engine.attach(player)
        engine.connect(player, to: engine.mainMixerNode, format: format)
        for t in [Tone.lock, .zap, .fail, .beep, .tick] { buffers[t] = synth(t) }
        start()
        // A call, Siri, an alarm or another app can stop the engine: restart it afterwards.
        let nc = NotificationCenter.default
        nc.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: .main) { [weak self] _ in
            self?.start()
        }
        nc.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine, queue: .main) { [weak self] _ in
            self?.start()
        }
        nc.addObserver(forName: UIApplication.didBecomeActiveNotification, object: nil, queue: .main) { [weak self] _ in
            self?.start()
        }
    }

    /// Playback category: plays with the ring/silent switch on silent (media volume).
    private func start() {
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.playback, options: [.mixWithOthers])
            try session.setActive(true)
            if !engine.isRunning { try engine.start() }
        } catch {
            print("[lab7] sound: \(error)")
        }
    }

    func play(_ t: Tone) {
        haptic(t)
        guard let b = buffers[t] else { return }
        if !engine.isRunning { start() }
        guard engine.isRunning else { return }
        player.stop()
        player.scheduleBuffer(b, at: nil)
        player.play()
        if t != .tick { print("[lab7] sound \(t) volume \(AVAudioSession.sharedInstance().outputVolume)") }
    }

    private func haptic(_ t: Tone) {
        switch t {
        case .lock: UINotificationFeedbackGenerator().notificationOccurred(.warning)
        case .zap: UIImpactFeedbackGenerator(style: .heavy).impactOccurred()
        case .fail: UINotificationFeedbackGenerator().notificationOccurred(.error)
        case .beep: UIImpactFeedbackGenerator(style: .light).impactOccurred()
        case .tick: break   // the scanner buzzes the Geiger haptic itself
        }
    }

    private func synth(_ t: Tone) -> AVAudioPCMBuffer {
        let rate = 44100.0
        if t == .tick {   // a Geiger click: a short burst of noise
            let n = Int(rate * 0.012)
            let buf = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(n))!
            buf.frameLength = AVAudioFrameCount(n)
            for i in 0..<n {
                buf.floatChannelData![0][i] = Float.random(in: -1...1) * 0.7 * Float(1 - Double(i) / Double(n))
            }
            return buf
        }
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
            case .tick: f = 0
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
