// Bluetooth to the jacket: LEDLINK (the Gledopto driving the LED strips),
// ../../ledlink/src/jacket_link.h. The app finds it by its GATT service and
// connects (an iPhone can't put manufacturer data in its own adverts, which is
// how the armband talks to it instead):
//   COMMAND (write):                cmd a b c
//   LIVE (write without response):  "M" bass mid treble level beat  |  "H" bpm
//   STATUS (read, notify):          look bright speed flags ackSeq r g b turn bpm
import CoreBluetooth
import Foundation

enum JacketCommand: UInt8 {
    case look = 1, bright, speed, color, turn, brake, power, next
}

enum Turn: UInt8 {
    case off = 0, left, right, hazard
}

struct JacketStatus: Equatable {
    var look = 0
    var bright = 60
    var speed = 100
    var flags = 1        // on, until the jacket says otherwise
    var color: UInt32 = 0x00DCFF
    var turn = Turn.off
    var bpm = 0

    var on: Bool { flags & 1 != 0 }
    var followingCostume: Bool { flags & 2 != 0 }
    var musicLive: Bool { flags & 4 != 0 }
    var heartLive: Bool { flags & 8 != 0 }
    var braking: Bool { flags & 16 != 0 }
}

final class JacketLink: NSObject, ObservableObject {
    enum State: Equatable { case bluetoothOff, searching, connecting, connected }

    static let service = CBUUID(string: "524A4B31-0000-4C41-4237-000000000000")
    static let commandUUID = CBUUID(string: "524A4B31-0001-4C41-4237-000000000000")
    static let liveUUID = CBUUID(string: "524A4B31-0002-4C41-4237-000000000000")
    static let statusUUID = CBUUID(string: "524A4B31-0003-4C41-4237-000000000000")

    @Published private(set) var state = State.searching
    @Published var status = JacketStatus()

    private var central: CBCentralManager!
    private var jacket: CBPeripheral?
    private var commandChar: CBCharacteristic?
    private var liveChar: CBCharacteristic?
    private var statusChar: CBCharacteristic?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
        // Screenshots: launch with -look N (no jacket needed).
        if UserDefaults.standard.object(forKey: "look") != nil { status.look = UserDefaults.standard.integer(forKey: "look") }
    }

    // MARK: Sending

    /// A command; the shown status changes at once (the jacket's own status follows).
    func send(_ cmd: JacketCommand, _ a: UInt8 = 0, _ b: UInt8 = 0, _ c: UInt8 = 0) {
        switch cmd {
        case .look: status.look = Int(a); status.flags |= 1
        case .bright: status.bright = Int(a)
        case .speed: status.speed = Int(a)
        case .color: status.color = UInt32(a) << 16 | UInt32(b) << 8 | UInt32(c)
        case .turn: status.turn = Turn(rawValue: a) ?? .off
        case .power: status.flags = a != 0 ? status.flags | 1 : status.flags & ~1
        default: break
        }
        guard let p = jacket, let ch = commandChar, state == .connected else { return }
        p.writeValue(Data([cmd.rawValue, a, b, c]), for: ch, type: .withResponse)
    }

    func setLook(_ id: Int) { send(.look, UInt8(id)) }

    func setColor(_ rgb: UInt32) { send(.color, UInt8(rgb >> 16 & 255), UInt8(rgb >> 8 & 255), UInt8(rgb & 255)) }

    /// Music levels or a heart rate: dropped rather than queued if the link is busy.
    func sendLive(_ bytes: [UInt8]) {
        guard let p = jacket, let ch = liveChar, state == .connected, p.canSendWriteWithoutResponse else { return }
        p.writeValue(Data(bytes), for: ch, type: .withoutResponse)
    }

    // MARK: Connecting

    private func search() {
        guard central.state == .poweredOn else { return }
        // A jacket the system already knows is connected (e.g. after relaunching).
        if let p = central.retrieveConnectedPeripherals(withServices: [Self.service]).first {
            connect(p)
            return
        }
        state = .searching
        central.scanForPeripherals(withServices: [Self.service])
    }

    private func connect(_ p: CBPeripheral) {
        central.stopScan()
        jacket = p
        p.delegate = self
        state = .connecting
        central.connect(p)
    }

    private func parse(_ d: Data) {
        guard d.count >= 10 else { return }
        let b = [UInt8](d)
        status = JacketStatus(look: Int(b[0]), bright: Int(b[1]), speed: Int(b[2]), flags: Int(b[3]),
                              color: UInt32(b[5]) << 16 | UInt32(b[6]) << 8 | UInt32(b[7]),
                              turn: Turn(rawValue: b[8]) ?? .off, bpm: Int(b[9]))
    }
}

extension JacketLink: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            search()
        } else {
            state = .bluetoothOff
            jacket = nil
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        if jacket == nil { connect(peripheral) }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.discoverServices([Self.service])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        jacket = nil
        search()
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        commandChar = nil
        liveChar = nil
        statusChar = nil
        // Walked out of range or the jacket's power went off: reconnect as soon
        // as it's back (a pending connect never times out).
        state = .connecting
        central.connect(peripheral)
    }
}

extension JacketLink: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let svc = peripheral.services?.first(where: { $0.uuid == Self.service }) else { return }
        peripheral.discoverCharacteristics([Self.commandUUID, Self.liveUUID, Self.statusUUID], for: svc)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        for ch in service.characteristics ?? [] {
            switch ch.uuid {
            case Self.commandUUID: commandChar = ch
            case Self.liveUUID: liveChar = ch
            case Self.statusUUID:
                statusChar = ch
                peripheral.setNotifyValue(true, for: ch)
                peripheral.readValue(for: ch)
            default: break
            }
        }
        if commandChar != nil { state = .connected }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if characteristic.uuid == Self.statusUUID, let d = characteristic.value { parse(d) }
    }
}
