// The scanner's game logic, as in ../../scanner/src/main.cpp: distance zones
// (with hysteresis), the CONTAIN sequence as seen from the scanner (it always
// fails: the AI escapes), the beam's recharge, the lock beep.
import Combine
import Foundation
import UIKit

enum Zone: Int, Comparable {
    case lost, detected, near, close
    static func < (a: Zone, b: Zone) -> Bool { a.rawValue < b.rawValue }
}

enum ContainState { case ready, fired, containing, failed, squad }

/// The infection game's rules: rare and gentle -- a party is all close contact.
enum Outbreak {
    static let contactDbm = -55.0          // "very close" to another phone: under ~50 cm
    static let contactSeconds = 30.0       // that long near a contagious phone: a chance to catch it
    static let aiSeconds = 45.0            // that long at TARGET LOCKED next to the AI: a chance
    static let catchChance = 3             // 1 in 3 each time
    static let maxShare = 0.25             // never more than a quarter of the guests infected
    static let cureDbm = -72.0             // standing at the checkpoint iPad
    static let immuneSeconds = 1800.0      // after a cure (or healing)
    static let healSeconds = 900.0         // it wears off on its own
}

final class Scanner: ObservableObject {
    let radio = Radio()
    @Published private(set) var zone = Zone.lost
    @Published private(set) var contain = ContainState.ready
    @Published private(set) var cooldownUntil = Date.distantPast
    @Published private(set) var containAt = Date.distantPast
    @Published private(set) var now = Date()
    @Published private(set) var jammedAt = Date.distantPast   // CONTAIN pressed while infected
    @Published private(set) var curedAt = Date.distantPast
    @Published private(set) var midnightStart: Date?           // the takeover is playing (MidnightView)
    private var takeoverClear = true                            // the beacon stopped saying TAKEOVER since the last one
    private var midnightClockDone = 0.0                         // the event midnight already played for
    private var firedAt = Date.distantPast
    private var exposure = 0.0, aiExposure = 0.0

    private let settings = Settings.shared
    private var lockBeepAt = Date.distantPast
    private var nextBuzz = Date.distantPast
    private let tap = UIImpactFeedbackGenerator(style: .light)
    private let thump = UIImpactFeedbackGenerator(style: .heavy)
    private var bag = Set<AnyCancellable>()

    init() {
        radio.objectWillChange.sink { [weak self] in self?.objectWillChange.send() }.store(in: &bag)
        Timer.publish(every: 0.1, on: .main, in: .common).autoconnect()
            .sink { [weak self] t in self?.update(t) }.store(in: &bag)
    }

    var ai: AiState { radio.ai }
    var cooling: Bool { now < cooldownUntil }
    var cooldownLeft: Int { max(0, Int(cooldownUntil.timeIntervalSince(now).rounded(.up))) }
    var cooldownProgress: Double {
        1 - max(0, cooldownUntil.timeIntervalSince(now)) / Settings.containCooldown
    }
    var containingPercent: Int { min(97, Int(now.timeIntervalSince(containAt) * 97 / 4.3)) }

    @Published private(set) var sentinelAt = Date.distantPast
    /// The SentinelOne button: contains the AI for real.
    func fireSentinel() {
        radio.fireSentinel()
        sentinelAt = now
        UINotificationFeedbackGenerator().notificationOccurred(.success)
        if settings.soundOn { Sound.shared.play(.lock) }
    }

    func fire() {
        if settings.infected {        // an infected scanner's beam is jammed: get cured first
            jammedAt = now
            play(.fail, always: true)
            return
        }
        if cooling {
            play(.beep, always: true)  // still recharging
            return
        }
        radio.fireContain()
        settings.countContain()
        play(.zap, always: true)
        contain = .fired
        containAt = now
        firedAt = now
        cooldownUntil = now.addingTimeInterval(Settings.containCooldown)
    }

    /// The takeover ended (its last frame, or RESIST).
    func endMidnight() { midnightStart = nil }

    /// Midnight: when the costume's beacon says TAKEOVER (it plays its own for ~70 s),
    /// or at the event's midnight time with no costume in range.
    private func midnight(_ t: Date, _ a: AiState) {
        let takeover = a.seen && a.activity == 3
        if !takeover { takeoverClear = true }
        if let s = midnightStart {
            if t.timeIntervalSince(s) > MidnightShow.total + 2 { midnightStart = nil }
            return
        }
        var clock = false
        if settings.joined, let m = settings.midnightDate, t >= m, t.timeIntervalSince(m) < 120,
           midnightClockDone != m.timeIntervalSince1970 {
            clock = true
        }
        guard (takeover && takeoverClear) || clock else { return }
        midnightStart = t
        takeoverClear = !takeover
        if let m = settings.midnightDate, abs(t.timeIntervalSince(m)) < 120 { midnightClockDone = m.timeIntervalSince1970 }
    }

#if DEBUG
    func demoMidnight() { midnightStart = Date() }   // simulator: -demoMidnight
#endif

    private func play(_ t: Tone, always: Bool = false) {
        if always || settings.soundOn { Sound.shared.play(t) }
    }

    private func zoneFor(_ a: AiState) -> Zone {
        guard a.seen else { return .lost }
        func above(_ th: Int, _ at: Zone) -> Bool { a.rssi >= th - (zone >= at ? Settings.hysteresisDb : 0) }
        return above(settings.closeDbm, .close) ? .close : above(settings.nearDbm, .near) ? .near : .detected
    }

    /// The infection: catching it (near a contagious phone, or the AI), spreading it
    /// (this phone's beacon), and the cure at the checkpoint.
    private func infection(_ t: Date, _ z: Zone) {
        let (agent, crowd, sick) = MainActor.assumeIsolated { (Game.shared.isAgent, Game.shared.crowd, Game.shared.infectedNow) }
        radio.contagious = agent || settings.infected     // the AI's agent: a carrier that looks clean
        radio.showsInfected = settings.infected           // what the checkpoint iPad sees (never the agent)
        if settings.infected {
            let healed = t.timeIntervalSince1970 - settings.infectedSince > Outbreak.healSeconds
            if healed || (t.timeIntervalSince(radio.cureAt) < 2 && radio.cureRssi >= Outbreak.cureDbm) {
                settings.infected = false
                settings.immuneUntil = t.addingTimeInterval(Outbreak.immuneSeconds).timeIntervalSince1970
                curedAt = t
                exposure = 0; aiExposure = 0
                play(.lock, always: true)
                thump.impactOccurred()
            }
            return
        }
        guard !agent, settings.joined || settings.demoMode,
              t.timeIntervalSince1970 >= settings.immuneUntil else { exposure = 0; aiExposure = 0; return }
        let near = radio.peers.values.contains {
            $0.contagious && t.timeIntervalSince($0.lastSeen) < 3 && $0.rssi >= Outbreak.contactDbm
        }
        exposure = near ? exposure + 0.1 : max(0, exposure - 0.05)
        aiExposure = z == .close ? aiExposure + 0.1 : max(0, aiExposure - 0.05)
        if exposure >= Outbreak.contactSeconds || aiExposure >= Outbreak.aiSeconds {
            let fromAI = aiExposure >= Outbreak.aiSeconds
            exposure = 0; aiExposure = 0
            // A chance, not a certainty -- and never more than a quarter of the guests at once.
            let capped = Double(sick) >= max(1, Double(crowd) * Outbreak.maxShare)
            guard !capped, Int.random(in: 0..<Outbreak.catchChance) == 0 else { return }
            settings.infectedBy = fromAI ? "THE ENTITY" : "ANOTHER SCANNER"
            settings.infectedSince = t.timeIntervalSince1970
            settings.infected = true
            play(.fail, always: true)
            thump.impactOccurred()
        }
    }

    private func update(_ t: Date) {
        now = t
        let a = radio.ai
        let z = zoneFor(a)
        if z != zone {
            // Lock beep-beep at most every 20 s, so a wobbling signal doesn't nag.
            if z == .close && t.timeIntervalSince(lockBeepAt) > 20 {
                play(.lock)
                lockBeepAt = t
            } else if z == .near && zone < .near {
                play(.beep)   // the signal just turned strong
            }
            zone = z
        }
        // Geiger counter: a click (sound on) and a buzz (vibrate on) that speed up as
        // the AI gets closer (1.6 s far, 0.15 s close). Quiet while the beam's sounds play.
        if a.seen && t >= nextBuzz && (settings.hapticsOn || settings.soundOn) {
            let k = Double(min(max(a.rssi, -95), -45) + 95) / 50
            nextBuzz = t.addingTimeInterval(1.6 - 1.45 * k)
            if settings.hapticsOn { (z == .close ? thump : tap).impactOccurred() }
            if settings.soundOn && contain == .ready { Sound.shared.play(.tick) }
        }
        infection(t, z)
        midnight(t, a)
        // A squad of scanners (3+ at once) contained it: the costume says so for a while.
        if a.seen && a.activity == 6 && contain != .squad {
            if t.timeIntervalSince(firedAt) < 15 { settings.countSquad() }   // this phone was in the squad
            contain = .squad
            containAt = t
            play(.lock, always: true)
        }
        // Follow the costume's side of the fight (it always wins).
        switch contain {
        case .squad where !a.seen || a.activity != 6:
            contain = .failed   // ...it had a backup
            containAt = t
            play(.fail)
        case .fired where a.activity == 4:
            contain = .containing
            containAt = t
        case .containing where a.activity != 4:
            contain = .failed
            containAt = t
            play(.fail)
        case .fired where t.timeIntervalSince(containAt) > 3:
            contain = .ready  // the beam didn't reach it (out of range)
        case .failed where t.timeIntervalSince(containAt) > 4.5:
            contain = .ready
        default:
            break
        }
    }
}
