// The scanner's game logic, as in ../../scanner/src/main.cpp: distance zones
// (with hysteresis), the CONTAIN sequence as seen from the scanner (it always
// fails: the AI escapes), the beam's recharge, the lock beep.
import Combine
import Foundation

enum Zone: Int, Comparable {
    case lost, detected, near, close
    static func < (a: Zone, b: Zone) -> Bool { a.rawValue < b.rawValue }
}

enum ContainState { case ready, fired, containing, failed }

final class Scanner: ObservableObject {
    let radio = Radio()
    @Published private(set) var zone = Zone.lost
    @Published private(set) var contain = ContainState.ready
    @Published private(set) var cooldownUntil = Date.distantPast
    @Published private(set) var containAt = Date.distantPast
    @Published private(set) var now = Date()

    private let settings = Settings.shared
    private var lockBeepAt = Date.distantPast
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
        if cooling {
            play(.beep, always: true)  // still recharging
            return
        }
        radio.fireContain()
        play(.zap, always: true)
        contain = .fired
        containAt = now
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

    private func update(_ t: Date) {
        now = t
        let a = radio.ai
        let z = zoneFor(a)
        if z != zone {
            // Lock beep-beep at most every 20 s, so a wobbling signal doesn't nag.
            if z == .close && t.timeIntervalSince(lockBeepAt) > 20 {
                play(.lock)
                lockBeepAt = t
            }
            zone = z
        }
        // Follow the costume's side of the fight (it always wins).
        switch contain {
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
