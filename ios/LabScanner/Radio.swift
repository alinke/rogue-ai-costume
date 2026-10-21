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
// researcher.cpp reads both forms. iOS only advertises a custom service UUID
// while the app is in the foreground -- the app keeps the screen on.
import CoreBluetooth
import Foundation

struct AiState: Equatable {
    var seen = false
    var rssi = -100
    var threat = 0
    var activity = 0    // 0 idle, 1 scanning a human, 2 game, 3 takeover, 4 being contained, 5 escaped
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

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
        peripheral = CBPeripheralManager(delegate: self, queue: nil)
        // "Lost" needs a clock: no beacon for lostAfter seconds.
        tick = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in self?.refreshSeen() }
    }

    func fireContain() {
        trigger &+= 1
        advertise()
    }

    private var beaconUUID: CBUUID {
        CBUUID(string: String(format: "52534331-%02X%02X-4C41-4237-000000000000", session, trigger))
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
        let seen = Date().timeIntervalSince(lastSeen) < Self.lostAfter
        if seen != ai.seen { ai.seen = seen }
    }
}

extension Radio: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ c: CBCentralManager) {
        bluetoothOn = c.state == .poweredOn
        scan()
    }

    func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral, advertisementData ad: [String: Any],
                        rssi RSSI: NSNumber) {
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

extension Radio: CBPeripheralManagerDelegate {
    func peripheralManagerDidUpdateState(_ p: CBPeripheralManager) {
        advertise()
    }
}
