// The music modes: the iPhone's microphone hears the music around you, an FFT
// splits it into bass / mids / treble, and a beat is a jump in the bass over its
// recent average. ~45 times a second the levels go to the jacket (JacketLink
// .sendLive, "M" bass mid treble level beat) -- nothing is recorded or kept.
//
// Each band is scaled against its own recent peak (automatic gain), so a quiet
// room and a loud concert both use the jacket's whole range.
import Accelerate
import AVFoundation

final class MusicListener: ObservableObject {
    @Published private(set) var running = false
    @Published private(set) var denied = false
    @Published private(set) var bass: Float = 0
    @Published private(set) var mid: Float = 0
    @Published private(set) var treble: Float = 0
    @Published private(set) var beats = 0

    /// Called on the audio thread with each frame's bytes (bass, mid, treble, level, beat).
    var onLevels: (([UInt8]) -> Void)?

    private let engine = AVAudioEngine()
    private let n = 1024
    private lazy var fft = vDSP.FFT(log2n: vDSP_Length(log2(Double(n))), radix: .radix2, ofType: DSPSplitComplex.self)!
    private lazy var window = vDSP.window(ofType: Float.self, usingSequence: .hanningDenormalized, count: n, isHalfWindow: false)
    private var peaks: [Float] = [1e-4, 1e-4, 1e-4, 1e-4]   // per band: a slowly falling peak (auto gain)
    private var bassHistory: [Float] = []
    private var lastBeat = Date.distantPast
    private var uiTick = 0

    func start() {
        guard !running else { return }
        AVAudioApplication.requestRecordPermission { ok in
            DispatchQueue.main.async {
                self.denied = !ok
                if ok { self.begin() }
            }
        }
    }

    func stop() {
        guard running else { return }
        engine.inputNode.removeTap(onBus: 0)
        engine.stop()
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
        running = false
    }

    private func begin() {
        let session = AVAudioSession.sharedInstance()
        do {
            // .measurement: no automatic gain or voice processing on the mic.
            // Mixing with others: music playing from this phone keeps playing.
            try session.setCategory(.playAndRecord, mode: .measurement,
                                    options: [.mixWithOthers, .defaultToSpeaker, .allowBluetoothA2DP])
            try session.setActive(true)
        } catch {
            return
        }
        let input = engine.inputNode
        let format = input.outputFormat(forBus: 0)
        guard format.sampleRate > 0 else { return }
        input.installTap(onBus: 0, bufferSize: AVAudioFrameCount(n), format: format) { [weak self] buf, _ in
            self?.process(buf, rate: Float(format.sampleRate))
        }
        do {
            try engine.start()
            running = true
        } catch {
            input.removeTap(onBus: 0)
        }
    }

    private func process(_ buf: AVAudioPCMBuffer, rate: Float) {
        guard let ch = buf.floatChannelData?[0], Int(buf.frameLength) >= n else { return }
        let samples = vDSP.multiply(UnsafeBufferPointer(start: ch, count: n), window)
        var real = [Float](repeating: 0, count: n / 2), imag = [Float](repeating: 0, count: n / 2)
        var mags = [Float](repeating: 0, count: n / 2)
        real.withUnsafeMutableBufferPointer { rp in
            imag.withUnsafeMutableBufferPointer { ip in
                var split = DSPSplitComplex(realp: rp.baseAddress!, imagp: ip.baseAddress!)
                samples.withUnsafeBufferPointer { sp in
                    sp.baseAddress!.withMemoryRebound(to: DSPComplex.self, capacity: n / 2) {
                        vDSP_ctoz($0, 2, &split, 1, vDSP_Length(n / 2))
                    }
                }
                fft.forward(input: split, output: &split)
                vDSP_zvmags(&split, 1, &mags, 1, vDSP_Length(n / 2))
            }
        }
        let hz = rate / Float(n)
        func band(_ lo: Float, _ hi: Float) -> Float {
            let a = max(1, Int(lo / hz)), b = min(n / 2 - 1, Int(hi / hz))
            guard b > a else { return 0 }
            return sqrt(vDSP.mean(mags[a...b]))
        }
        let raw = [band(30, 160), band(160, 2000), band(2000, 9000), band(30, 9000)]
        var out = [Float](repeating: 0, count: 4)
        for i in 0..<4 {
            peaks[i] = max(raw[i], peaks[i] * 0.997, 1e-4)        // falls ~13% a second
            out[i] = min(1, raw[i] / peaks[i])
            out[i] = out[i] * out[i]                               // more contrast
        }
        // A beat: the bass well above its last ~0.7 s, not too soon after the last.
        bassHistory.append(raw[0])
        if bassHistory.count > 32 { bassHistory.removeFirst() }
        let avg = bassHistory.reduce(0, +) / Float(bassHistory.count)
        let now = Date()
        let beat = raw[0] > avg * 1.45 && raw[0] > peaks[0] * 0.3 && now.timeIntervalSince(lastBeat) > 0.25
        if beat { lastBeat = now }
        onLevels?(out.map { UInt8($0 * 255) } + [beat ? 1 : 0])
        uiTick += 1
        if beat || uiTick % 3 == 0 {
            DispatchQueue.main.async {
                self.bass = out[0]
                self.mid = out[1]
                self.treble = out[2]
                if beat { self.beats += 1 }
            }
        }
    }
}
