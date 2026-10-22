// Listens for the costume's Bluetooth beacon (manufacturer data 0xFFFF "RAI1"
// + threat, activity, scanned, contained -- see ../../costume/src/researcher.cpp).
// Its signal strength says how close the AI is to this iPad.
import CoreBluetooth
import Foundation

struct AiState {
    var seen = false
    var rssi = -100
    var threat = 0
    var activity = 0    // 0 idle, 1 scanning a human, 2 game, 3 takeover, 4 being contained, 5 escaped
    var scanned = 0
    var contained = 0
}

final class Radio: NSObject, CBCentralManagerDelegate {
    private(set) var ai = AiState()
    private(set) var bluetoothOn = false
    private var central: CBCentralManager!
    private var lastSeen = Date.distantPast
    private var rssiAvg = -100.0
    static let lostAfter: TimeInterval = 6

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
    }

    /// From the display loop: "lost" needs a clock.
    func tick() {
        ai.seen = Date().timeIntervalSince(lastSeen) < Radio.lostAfter
    }

    func centralManagerDidUpdateState(_ c: CBCentralManager) {
        bluetoothOn = c.state == .poweredOn
        if bluetoothOn {
            // Duplicates on: every advertisement, so the signal strength stays live.
            c.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
        }
    }

    func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral, advertisementData ad: [String: Any],
                        rssi RSSI: NSNumber) {
        guard let m = ad[CBAdvertisementDataManufacturerDataKey] as? Data, m.count >= 10 else { return }
        let b = [UInt8](m)
        guard b[0] == 0xFF, b[1] == 0xFF, b[2] == 0x52, b[3] == 0x41, b[4] == 0x49, b[5] == 0x31 else { return } // "RAI1"
        let r = RSSI.doubleValue
        guard r < 0 else { return }  // 127 = not available
        let now = Date()
        rssiAvg = now.timeIntervalSince(lastSeen) > 3 ? r : rssiAvg + (r - rssiAvg) * 0.25
        lastSeen = now
        ai = AiState(seen: true, rssi: Int(rssiAvg.rounded()), threat: Int(b[6]), activity: Int(b[7]),
                     scanned: Int(b[8]), contained: Int(b[9]))
    }
}
