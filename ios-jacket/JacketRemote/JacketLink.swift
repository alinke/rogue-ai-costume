// Bluetooth to the jacket: LEDLINK (the Gledopto driving the LED strips),
// ../../ledlink/src/jacket_link.h. The app finds it by its GATT service and
// connects (an iPhone can't put manufacturer data in its own adverts, which is
// how the armband talks to it instead):
//   COMMAND (write):                cmd a b c
//   LIVE (write without response):  "M" bass mid treble level beat  |  "H" bpm
//   STATUS (read, notify):          look bright speed flags ackSeq r g b turn bpm
//   LAYOUT (read, write):           the LED layout (settings): see LedLayout
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

/// One stretch of the LED chain, in wiring order (ledlink/src/jacket_link.h LAYOUT).
struct LedZone: Identifiable, Equatable {
    enum Kind: UInt8, CaseIterable, Identifiable {
        case sleeve = 0, front, eye, matrix, dark
        var id: UInt8 { rawValue }
        var title: String {
            switch self {
            case .sleeve: "Sleeve"
            case .front: "Front"
            case .eye: "Eye pod"
            case .matrix: "8x8 matrix"
            case .dark: "Dark"
            }
        }
        var lit: Bool { self != .dark }
    }
    enum Side: Int8, CaseIterable, Identifiable {
        case left = -1, none = 0, right = 1
        var id: Int8 { rawValue }
        var title: String { self == .left ? "Left" : self == .right ? "Right" : "None" }
    }

    var id = UUID()
    var kind: Kind
    var side: Side
    var reversed: Bool
    var count: Int

    static func == (a: LedZone, b: LedZone) -> Bool {
        a.kind == b.kind && a.side == b.side && a.reversed == b.reversed && a.count == b.count
    }
}

/// The controller's LED layout and what it says about itself.
struct LedLayout: Equatable {
    var zones: [LedZone] = []
    var saved = false        // the app's layout, not the firmware's built-in one
    var matrixSkipped = false // an 8x8 matrix zone isn't wired in right now (no costume)
    var maxLeds = 0
    var dataPin = 0
    var maxMilliamps = 0
    var address = ""         // Wi-Fi address ("" without Wi-Fi)

    var total: Int { zones.reduce(0) { $0 + $1.count } }
    var lit: Int { zones.filter { $0.kind.lit && !(matrixSkipped && $0.kind == .matrix) }.reduce(0) { $0 + $1.count } }

    /// Where zone i starts on the wire: an 8x8 matrix that isn't wired in takes no room.
    func start(of i: Int) -> Int {
        zones[..<i].filter { !(matrixSkipped && $0.kind == .matrix) }.reduce(0) { $0 + $1.count }
    }

    init() {}

    init?(_ d: Data) {
        let b = [UInt8](d)
        guard b.count >= 11, b[0] == UInt8(ascii: "L"), b[1] == 1 else { return nil }
        let n = Int(b[3])
        guard b.count >= 11 + n * 5 else { return nil }
        saved = b[2] & 1 != 0
        matrixSkipped = b[2] & 2 != 0
        maxLeds = Int(b[6]) | Int(b[7]) << 8
        dataPin = Int(b[8])
        maxMilliamps = Int(b[9]) | Int(b[10]) << 8
        for k in 0..<n {
            let p = 11 + k * 5
            zones.append(LedZone(kind: LedZone.Kind(rawValue: b[p]) ?? .dark, side: LedZone.Side(rawValue: Int8(bitPattern: b[p + 1])) ?? .none,
                                 reversed: b[p + 2] != 0, count: Int(b[p + 3]) | Int(b[p + 4]) << 8))
        }
        address = String(decoding: b[(11 + n * 5)...], as: UTF8.self)
    }

    var bytes: Data {
        var b: [UInt8] = [UInt8(ascii: "L"), 1, UInt8(zones.count)]
        for z in zones {
            b += [z.kind.rawValue, UInt8(bitPattern: z.side.rawValue), z.reversed ? 1 : 0, UInt8(z.count & 255), UInt8(z.count >> 8)]
        }
        return Data(b)
    }
}

extension LedLayout {
    /// Demo mode's layout (no controller): the model jacket the preview draws.
    static let demo: LedLayout = {
        var l = LedLayout()
        l.zones = [LedZone(kind: .front, side: .right, reversed: false, count: 78),
                   LedZone(kind: .front, side: .left, reversed: true, count: 77),
                   LedZone(kind: .dark, side: .none, reversed: false, count: 8),
                   LedZone(kind: .sleeve, side: .left, reversed: true, count: 113),
                   LedZone(kind: .dark, side: .none, reversed: false, count: 30),
                   LedZone(kind: .sleeve, side: .right, reversed: true, count: 113)]
        l.maxLeds = 1200
        l.dataPin = 16
        l.maxMilliamps = 2000
        return l
    }()
}

enum TestPattern: UInt8 { case off = 0, numbered, range, lit }

final class JacketLink: NSObject, ObservableObject {
    enum State: Equatable { case bluetoothOff, searching, connecting, connected }

    static let service = CBUUID(string: "524A4B31-0000-4C41-4237-000000000000")
    static let commandUUID = CBUUID(string: "524A4B31-0001-4C41-4237-000000000000")
    static let liveUUID = CBUUID(string: "524A4B31-0002-4C41-4237-000000000000")
    static let statusUUID = CBUUID(string: "524A4B31-0003-4C41-4237-000000000000")
    static let layoutUUID = CBUUID(string: "524A4B31-0004-4C41-4237-000000000000")

    @Published private(set) var state = State.searching
    @Published var status = JacketStatus()
    @Published private(set) var layout: LedLayout? = .demo  // the controller's once read; nil = its firmware has no settings
    /// No controller connected: the app runs on the on-screen preview, with a sample layout.
    var demo: Bool { state != .connected }
    @Published private(set) var controllerName = ""

    private var central: CBCentralManager!
    private var jacket: CBPeripheral?
    private var commandChar: CBCharacteristic?
    private var liveChar: CBCharacteristic?
    private var statusChar: CBCharacteristic?
    private var layoutChar: CBCharacteristic?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
        // Screenshots: launch with -look N (no jacket needed).
        if UserDefaults.standard.object(forKey: "look") != nil { status.look = UserDefaults.standard.integer(forKey: "look") }
        jp_command(JacketCommand.look.rawValue, UInt8(status.look), 0, 0)
        if UserDefaults.standard.object(forKey: "turn") != nil { send(.turn, UInt8(UserDefaults.standard.integer(forKey: "turn"))) }
        // Screenshots: -bpm 72 (a heart rate) and -music 1 (a 120 bpm beat) for the preview.
        let bpm = UserDefaults.standard.integer(forKey: "bpm"), music = UserDefaults.standard.bool(forKey: "music")
        if bpm > 0 || music {
            var t = 0.0
            Timer.scheduledTimer(withTimeInterval: 1.0 / 30, repeats: true) { _ in
                t += 1.0 / 30
                if bpm > 0, Int(t * 30) % 30 == 0 { jp_heart(UInt8(bpm)) }
                if music {
                    let beat = t.truncatingRemainder(dividingBy: 0.5) < 1.0 / 30
                    let b = 0.5 + 0.5 * sin(t * 2.3), m = 0.5 + 0.5 * sin(t * 3.1 + 1)
                    jp_music(UInt8(60 + 190 * b), UInt8(50 + 170 * m), UInt8(80 + 120 * (0.5 + 0.5 * sin(t * 7))), 180, beat ? 1 : 0)
                }
            }
        }
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
        jp_command(cmd.rawValue, a, b, c)  // the preview follows along (it's what demo mode shows)
        guard let p = jacket, let ch = commandChar, state == .connected else { return }
        p.writeValue(Data([cmd.rawValue, a, b, c]), for: ch, type: .withResponse)
    }

    func setLook(_ id: Int) { send(.look, UInt8(id)) }

    func setColor(_ rgb: UInt32) { send(.color, UInt8(rgb >> 16 & 255), UInt8(rgb >> 8 & 255), UInt8(rgb & 255)) }

    /// Music levels or a heart rate: dropped rather than queued if the link is busy.
    func sendLive(_ bytes: [UInt8]) {
        if bytes.count >= 6, bytes[0] == UInt8(ascii: "M") { jp_music(bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]) }
        if bytes.count >= 2, bytes[0] == UInt8(ascii: "H") { jp_heart(bytes[1]) }
        guard let p = jacket, let ch = liveChar, state == .connected, p.canSendWriteWithoutResponse else { return }
        p.writeValue(Data(bytes), for: ch, type: .withoutResponse)
    }

    // MARK: Settings

    func readLayout() {
        guard let p = jacket, let ch = layoutChar, state == .connected else { return }
        p.readValue(for: ch)
    }

    /// Saves the layout on the controller (it keeps it), then reads it back.
    func saveLayout(_ l: LedLayout) {
        if demo { layout = l; return }
        guard let p = jacket, let ch = layoutChar, state == .connected else { return }
        p.writeValue(l.bytes, for: ch, type: .withResponse)
        p.readValue(for: ch)
    }

    /// Back to the layout built into the firmware.
    func resetLayout() {
        if demo { layout = .demo; return }
        guard let p = jacket, let ch = layoutChar, state == .connected else { return }
        p.writeValue(Data([UInt8(ascii: "D")]), for: ch, type: .withResponse)
        p.readValue(for: ch)
    }

    /// A test pattern on the LEDs (wire positions a...b for .range); .off goes back to the looks.
    func test(_ t: TestPattern, _ a: Int = 0, _ b: Int = 0) {
        guard let p = jacket, let ch = layoutChar, state == .connected else { return }
        p.writeValue(Data([UInt8(ascii: "T"), t.rawValue, UInt8(a & 255), UInt8(a >> 8 & 255), UInt8(b & 255), UInt8(b >> 8 & 255)]),
                     for: ch, type: .withResponse)
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
        controllerName = p.name ?? ""
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
        } else if central.state == .unsupported {
            state = .searching  // no Bluetooth at all (the simulator): just the demo
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
        layoutChar = nil
        layout = .demo
        // Walked out of range or the jacket's power went off: reconnect as soon
        // as it's back (a pending connect never times out).
        state = .connecting
        central.connect(peripheral)
    }
}

extension JacketLink: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let svc = peripheral.services?.first(where: { $0.uuid == Self.service }) else { return }
        peripheral.discoverCharacteristics([Self.commandUUID, Self.liveUUID, Self.statusUUID, Self.layoutUUID], for: svc)
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
            case Self.layoutUUID:
                layoutChar = ch
                peripheral.readValue(for: ch)
            default: break
            }
        }
        if commandChar != nil {
            state = .connected
            if layoutChar == nil { layout = nil }  // older LEDLINK: no settings
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if characteristic.uuid == Self.statusUUID, let d = characteristic.value { parse(d) }
        if characteristic.uuid == Self.layoutUUID, let d = characteristic.value { layout = LedLayout(d) }
    }
}
