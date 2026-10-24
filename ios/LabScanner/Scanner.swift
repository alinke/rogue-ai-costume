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

/// The infection game's rules.
enum Outbreak {
    static let contactDbm = -64.0          // "close" to another phone: about 1-2 m
    static let contactSeconds = 4.0        // that long near a contagious phone: infected
    static let aiSeconds = 10.0            // that long right next to the AI: infected
    static let cureDbm = -72.0             // standing at the checkpoint iPad
    static let immuneSeconds = 300.0       // after a cure
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
        let agent = MainActor.assumeIsolated { Game.shared.isAgent }
        radio.contagious = agent || settings.infected     // the AI's agent: a carrier that looks clean
        if settings.infected {
            if t.timeIntervalSince(radio.cureAt) < 2 && radio.cureRssi >= Outbreak.cureDbm {
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
            settings.infectedBy = aiExposure >= Outbreak.aiSeconds ? "THE ENTITY" : "ANOTHER SCANNER"
            settings.infected = true
            exposure = 0; aiExposure = 0
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
