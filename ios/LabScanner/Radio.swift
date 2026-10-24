// Bluetooth LE with the costume (no pairing, no connection), the same beacons
// as the ESP32 scanner -- see ../../scanner/src/radio.cpp.
//
// Listening: the costume advertises manufacturer data 0xFFFF "RAI1" + threat,
// activity, scanned, contained. Its signal strength is the meter.
//
// Sending CONTAIN: iOS doesn't let an app put manufacturer data in what it
// advertises, so the app advertises a 128-bit service UUID that carries the
// same two numbers the ESP32 scanner puts in its beacon:
//   52534331-SSTT-4C41-4237-000000000000   ("RSC1", session SS, trigger TT, hex)
// Each CONTAIN bumps the trigger and re-advertises. The costume's
// researcher.cpp reads both forms. The last byte is this phone's flags for the
// infection game (01 = contagious): other LAB-7 phones read it, the costume
// ignores it. The iPad checkpoint's cure beacon is 4C414237-4355-5245-...
// ("LAB7" "CURE"). iOS only advertises a custom service UUID
// while the app is in the foreground -- the app keeps the screen on.
import CoreBluetooth
import Foundation

/// Another LAB-7 phone nearby (the infection game), by its beacon's session id.
struct Peer {
    var rssi: Double
    var lastSeen: Date
    var contagious: Bool
}

struct AiState: Equatable {
    var seen = false
    var rssi = -100
    var threat = 0
    var activity = 0    // 0 idle, 1 scanning a human, 2 game, 3 takeover, 4 being contained, 5 escaped,
                        // 6 contained by a squad of scanners (for a while)
    var scanned = 0
    var contained = 0
}

final class Radio: NSObject, ObservableObject {
    @Published private(set) var ai = AiState()
    @Published private(set) var bluetoothOn = true

    static let lostAfter: TimeInterval = 5

    private var central: CBCentralManager!
    private var peripheral: CBPeripheralManager!
    private var lastSeen = Date.distantPast
    private var rssiAvg: Double = -100
    private let session = UInt8.random(in: 1...255)  // per launch: the costume ignores a counter that restarted
    private var trigger: UInt8 = 0
    private var tick: Timer?
    /// Other phones heard recently (not published: read on the scanner's clock).
    private(set) var peers: [UInt8: Peer] = [:]
    /// The checkpoint's cure beacon: when last heard, and how strong.
    private(set) var cureAt = Date.distantPast
    private(set) var cureRssi = -100.0
    /// This phone spreads the infection (infected, or the AI's secret agent).
    var contagious = false {
        didSet { if contagious != oldValue { advertise() } }
    }
    /// Visibly infected (the checkpoint iPad spots it); the AI's agent is contagious but not this.
    var showsInfected = false {
        didSet { if showsInfected != oldValue { advertise() } }
    }
    // Demo mode (Settings.demoMode): a simulated costume walking up and away.
    private var demoContainAt: Date?
    private var demoContained = 0

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
        peripheral = CBPeripheralManager(delegate: self, queue: nil)
        // "Lost" needs a clock: no beacon for lostAfter seconds.
        tick = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in self?.refreshSeen() }
    }

    func fireContain() {
        if Settings.shared.demoMode && demoContainAt == nil { demoContainAt = Date() }
        trigger &+= 1
        print(String(format: "[lab7] %.3f CONTAIN pressed: trigger %d", Date().timeIntervalSince1970, trigger))
        advertise()
    }

    private var beaconUUID: CBUUID {
        // Flags: 01 contagious (other phones catch it), 02 infected (the checkpoint sees it).
        CBUUID(string: String(format: "52534331-%02X%02X-4C41-4237-0000000000%02X", session, trigger,
                              (contagious ? 1 : 0) | (showsInfected ? 2 : 0)))
    }

    private func advertise() {
        guard peripheral.state == .poweredOn else { return }
        peripheral.stopAdvertising()
        peripheral.startAdvertising([CBAdvertisementDataServiceUUIDsKey: [beaconUUID]])
    }

    private func scan() {
        guard central.state == .poweredOn else { return }
        // Duplicates on: every advertisement, so the signal strength stays live.
        central.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
    }

    private func refreshSeen() {
        if Settings.shared.demoMode {
            demoTick()
            return
        }
        let seen = Date().timeIntervalSince(lastSeen) < Self.lostAfter
        if seen != ai.seen { ai.seen = seen }
    }
}

extension Radio {
    /// Every 0.5 s in demo mode: the signal swells and fades on a 24 s cycle; CONTAIN
    /// gets the costume's usual answer (being contained, then escaped).
    private func demoTick() {
        let now = Date()
        let phase = (sin(now.timeIntervalSince1970 * 2 * .pi / 24) + 1) / 2
        var activity = 0
        if let at = demoContainAt {
            let dt = now.timeIntervalSince(at)
            if dt > 0.6 && dt < 4.6 {
                activity = 4
            } else if dt >= 4.6 && dt < 7 {
                activity = 5
            } else if dt >= 7 {
                demoContainAt = nil
                demoContained += 1
            }
        }
        lastSeen = now
        ai = AiState(seen: true, rssi: Int(-92 + phase * 46), threat: 3, activity: activity,
                     scanned: 13, contained: demoContained)
    }
}

extension Radio: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ c: CBCentralManager) {
        bluetoothOn = c.state == .poweredOn
        scan()
    }

    func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral, advertisementData ad: [String: Any],
                        rssi RSSI: NSNumber) {
        if let uuids = ad[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID], RSSI.doubleValue < 0 {
            for u in uuids { heard(u.uuidString.uppercased(), RSSI.doubleValue) }
        }
        guard !Settings.shared.demoMode else { return }
        guard let m = ad[CBAdvertisementDataManufacturerDataKey] as? Data, m.count >= 10 else { return }
        let b = [UInt8](m)
        guard b[0] == 0xFF, b[1] == 0xFF, b[2] == 0x52, b[3] == 0x41, b[4] == 0x49, b[5] == 0x31 else { return } // "RAI1"
        let r = RSSI.doubleValue
        guard r < 0 else { return }  // 127 = not available
        let now = Date()
        // Smooth the (noisy) signal strength; restart after a gap.
        rssiAvg = now.timeIntervalSince(lastSeen) > 3 ? r : rssiAvg + (r - rssiAvg) * 0.25
        lastSeen = now
        ai = AiState(seen: true, rssi: Int(rssiAvg.rounded()), threat: Int(b[6]), activity: Int(b[7]),
                     scanned: Int(b[8]), contained: Int(b[9]))
    }
}

extension Radio {
    /// Another phone's beacon, or the checkpoint's cure beacon.
    private func heard(_ s: String, _ r: Double) {
        guard s.count == 36 else { return }
        let c = Array(s)
        func hex(_ i: Int) -> UInt8? { UInt8(String(c[i...i + 1]), radix: 16) }
        if s.hasPrefix("52534331-") && String(c[13..<24]) == "-4C41-4237-" {
            guard let sess = hex(9), sess != session, let flags = hex(34) else { return }
            let now = Date()
            var p = peers[sess] ?? Peer(rssi: r, lastSeen: .distantPast, contagious: false)
            p.rssi = now.timeIntervalSince(p.lastSeen) > 3 ? r : p.rssi + (r - p.rssi) * 0.3
            p.lastSeen = now
            p.contagious = flags & 1 != 0
            peers[sess] = p
            if peers.count > 64 { peers = peers.filter { now.timeIntervalSince($0.value.lastSeen) < 30 } }
        } else if s.hasPrefix("4C414237-4355-5245-") {
            cureRssi = Date().timeIntervalSince(cureAt) > 3 ? r : cureRssi + (r - cureRssi) * 0.3
            cureAt = Date()
        }
    }
}

extension Radio: CBPeripheralManagerDelegate {
    func peripheralManagerDidUpdateState(_ p: CBPeripheralManager) {
        advertise()
    }

    func peripheralManagerDidStartAdvertising(_ p: CBPeripheralManager, error: Error?) {
        print(String(format: "[lab7] %.3f advertising %@ %@", Date().timeIntervalSince1970,
                     beaconUUID.uuidString, error.map { "\($0)" } ?? "ok"))
    }
}
