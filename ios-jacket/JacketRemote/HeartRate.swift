// Your heart rate, for the HEARTBEAT look, from either:
//  - a Bluetooth heart-rate sensor: anything with the standard Heart Rate
//    service (0x180D) -- a Fitbit Charge 6 or Pixel Watch 2-4 with heart-rate
//    sharing on (older Fitbits can't), Polar / Wahoo / Garmin chest straps and arm bands, many
//    watches' "broadcast heart rate";
//  - an Apple Watch running this app's watch app (JacketWatch), which runs a
//    workout session for a live heart rate and sends it here.
// Whichever is sending; the newest reading wins.
import CoreBluetooth
import Foundation
import WatchConnectivity

struct HeartSensor: Identifiable, Equatable {
    let id: UUID
    var name: String
}

final class HeartRate: NSObject, ObservableObject {
    @Published private(set) var bpm: Int?
    @Published private(set) var source = ""
    @Published private(set) var sensors: [HeartSensor] = []
    @Published private(set) var scanning = false
    @Published private(set) var connectedName: String?
    @Published private(set) var watchPaired = false

    var onBpm: ((Int) -> Void)?

    static let service = CBUUID(string: "180D")
    static let measurement = CBUUID(string: "2A37")
    private static let savedKey = "heartSensor"

    private var central: CBCentralManager!
    private var found: [UUID: CBPeripheral] = [:]
    private var sensor: CBPeripheral?
    private var lastReading = Date.distantPast
    private var staleTimer: Timer?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
        if WCSession.isSupported() {
            WCSession.default.delegate = self
            WCSession.default.activate()
        }
        // No reading for 10 s: show "no heart rate" again.
        staleTimer = Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { [weak self] _ in
            guard let self, self.bpm != nil, Date().timeIntervalSince(self.lastReading) > 10 else { return }
            self.bpm = nil
        }
    }

    func startScan() {
        guard central.state == .poweredOn else { return }
        // Sensors already connected to this phone (paired in Settings, or by another app) don't advertise.
        for p in central.retrieveConnectedPeripherals(withServices: [Self.service]) { add(p) }
        central.scanForPeripherals(withServices: [Self.service])
        scanning = true
    }

    func stopScan() {
        central.stopScan()
        scanning = false
    }

    func connect(_ s: HeartSensor) {
        guard let p = found[s.id] else { return }
        stopScan()
        if let old = sensor, old != p { central.cancelPeripheralConnection(old) }
        sensor = p
        p.delegate = self
        central.connect(p)
        UserDefaults.standard.set(s.id.uuidString, forKey: Self.savedKey)
    }

    func forget() {
        if let p = sensor { central.cancelPeripheralConnection(p) }
        sensor = nil
        connectedName = nil
        UserDefaults.standard.removeObject(forKey: Self.savedKey)
    }

    private func add(_ p: CBPeripheral) {
        found[p.identifier] = p
        if !sensors.contains(where: { $0.id == p.identifier }) {
            sensors.append(HeartSensor(id: p.identifier, name: p.name ?? "Heart-rate sensor"))
        }
    }

    private func reading(_ value: Int, from: String) {
        guard (30...230).contains(value) else { return }
        lastReading = Date()
        bpm = value
        source = from
        onBpm?(value)
    }
}

extension HeartRate: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        guard central.state == .poweredOn else { return }
        // Reconnect to the sensor used last time.
        if let s = UserDefaults.standard.string(forKey: Self.savedKey), let id = UUID(uuidString: s),
           let p = central.retrievePeripherals(withIdentifiers: [id]).first {
            add(p)
            sensor = p
            p.delegate = self
            central.connect(p)
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        add(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        connectedName = peripheral.name ?? "Heart-rate sensor"
        peripheral.discoverServices([Self.service])
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        guard peripheral == sensor else { return }
        connectedName = nil
        central.connect(peripheral)  // back as soon as it's in range again
    }
}

extension HeartRate: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let svc = peripheral.services?.first(where: { $0.uuid == Self.service }) else { return }
        peripheral.discoverCharacteristics([Self.measurement], for: svc)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let ch = service.characteristics?.first(where: { $0.uuid == Self.measurement }) {
            peripheral.setNotifyValue(true, for: ch)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        // Heart Rate Measurement: flags, then the rate as 8 bits (flag bit 0
        // clear) or 16 bits little-endian (set).
        guard let d = characteristic.value, d.count >= 2 else { return }
        let b = [UInt8](d)
        let v = b[0] & 1 == 0 ? Int(b[1]) : (d.count >= 3 ? Int(b[1]) | Int(b[2]) << 8 : 0)
        reading(v, from: peripheral.name ?? "Sensor")
    }
}

extension HeartRate: WCSessionDelegate {
    func session(_ session: WCSession, activationDidCompleteWith state: WCSessionActivationState, error: Error?) {
        DispatchQueue.main.async { self.watchPaired = session.isPaired && session.isWatchAppInstalled }
    }

    func sessionWatchStateDidChange(_ session: WCSession) {
        DispatchQueue.main.async { self.watchPaired = session.isPaired && session.isWatchAppInstalled }
    }

    func session(_ session: WCSession, didReceiveMessage message: [String: Any]) {
        guard let v = message["bpm"] as? Int else { return }
        DispatchQueue.main.async { self.reading(v, from: "Apple Watch") }
    }

    func sessionDidBecomeInactive(_ session: WCSession) {}

    func sessionDidDeactivate(_ session: WCSession) { session.activate() }
}
