// Everything the ESP32 scanner keeps in secrets.h / config.h, as in-app settings
// (stored on the phone, never in the repo), plus the guest side: the event this
// phone joined, the agent identity and the leaderboard score.
import Foundation

final class Settings: ObservableObject {
    static let shared = Settings()
    private let d = UserDefaults.standard

    // Distance zones (dBm of the costume's beacon) -- tune at the party.
    @Published var nearDbm: Int { didSet { d.set(nearDbm, forKey: "nearDbm") } }
    @Published var closeDbm: Int { didSet { d.set(closeDbm, forKey: "closeDbm") } }
    @Published var soundOn: Bool { didSet { d.set(soundOn, forKey: "soundOn") } }
    /// Geiger-counter buzz, faster as the AI gets closer.
    @Published var hapticsOn: Bool { didSet { d.set(hapticsOn, forKey: "hapticsOn") } }
    /// No costume around (App Review, trying it at home): a simulated AI signal.
    @Published var demoMode: Bool { didSet { d.set(demoMode, forKey: "demoMode") } }
    // The humanity test's camera: rear by default (the researcher points the
    // phone at the guest); the viewfinder's flip button switches and remembers.
    @Published var frontCamera: Bool { didSet { d.set(frontCamera, forKey: "frontCamera") } }
    // The humanity test.
    @Published var researcherName: String { didSet { d.set(researcherName, forKey: "researcherName") } }
    @Published var compromisedPercent: Int { didSet { d.set(compromisedPercent, forKey: "compromisedPercent") } }
    // Photo upload / the event: your photo service, see ../../cloud/README.md.
    @Published var serviceURL: String { didSet { d.set(serviceURL, forKey: "serviceURL") } }
    @Published var uploadKey: String { didSet { d.set(uploadKey, forKey: "uploadKey") } }
    @Published var siteText: String { didSet { d.set(siteText, forKey: "siteText") } }
    @Published var eventName: String { didSet { d.set(eventName, forKey: "eventName") } }
    /// When "something happens" at the party (seconds since 1970; 0 = none).
    @Published var midnight: Double { didSet { d.set(midnight, forKey: "midnight") } }
    // The agent: identity (0-99, the same codes as the landing page and the
    // costume's keypad), leaderboard name and score.
    @Published var agentId: Int { didSet { d.set(agentId, forKey: "agentId") } }
    @Published var agentName: String { didSet { d.set(agentName, forKey: "agentName"); Stats.shared.changed() } }
    @Published private(set) var containCount: Int { didSet { d.set(containCount, forKey: "containCount") } }
    @Published private(set) var testCount: Int { didSet { d.set(testCount, forKey: "testCount") } }
    /// Phones whose photos this user chose not to see (the wall's "hide this phone").
    @Published var blocked: [String] { didSet { d.set(blocked, forKey: "blocked") } }
    /// A random id for this phone (rate limits, the leaderboard): not tied to the person.
    let deviceId: String

    static let hysteresisDb = 4
    static let containCooldown: TimeInterval = 12

    var uploadConfigured: Bool { !serviceURL.isEmpty && !uploadKey.isEmpty }
    var joined: Bool { uploadConfigured }
    var midnightDate: Date? { midnight > 0 ? Date(timeIntervalSince1970: midnight) : nil }
    var agent: Agent { Agent(id: agentId) }
    var leaderboardName: String { agentName.isEmpty ? agent.codename : agentName }

    private init() {
        d.register(defaults: ["nearDbm": -70, "closeDbm": -58, "soundOn": true, "hapticsOn": true,
                              "researcherName": "LAB-7 FIELD RESEARCHER", "compromisedPercent": 20,
                              "siteText": "THE ROGUE.AI WALL", "agentId": -1])
        nearDbm = d.integer(forKey: "nearDbm")
        closeDbm = d.integer(forKey: "closeDbm")
        soundOn = d.bool(forKey: "soundOn")
        hapticsOn = d.bool(forKey: "hapticsOn")
        demoMode = d.bool(forKey: "demoMode")
        frontCamera = d.bool(forKey: "frontCamera")
        researcherName = d.string(forKey: "researcherName") ?? ""
        compromisedPercent = d.integer(forKey: "compromisedPercent")
        serviceURL = d.string(forKey: "serviceURL") ?? ""
        uploadKey = d.string(forKey: "uploadKey") ?? ""
        siteText = d.string(forKey: "siteText") ?? ""
        eventName = d.string(forKey: "eventName") ?? ""
        midnight = d.double(forKey: "midnight")
        var id = d.integer(forKey: "agentId")
        if id < 0 || id > 99 {
            id = Int.random(in: 0...99)
            d.set(id, forKey: "agentId")
        }
        agentId = id
        agentName = d.string(forKey: "agentName") ?? ""
        containCount = d.integer(forKey: "containCount")
        testCount = d.integer(forKey: "testCount")
        blocked = d.stringArray(forKey: "blocked") ?? []
        if let dev = d.string(forKey: "deviceId") {
            deviceId = dev
        } else {
            deviceId = UUID().uuidString
            d.set(deviceId, forKey: "deviceId")
        }
    }

    func countContain() { containCount += 1; Stats.shared.changed() }
    func countTest() { testCount += 1; Stats.shared.changed() }

    // MARK: - Links

    /// What a link would set, before the user agrees to join.
    struct Invite: Identifiable, Equatable {
        let id = UUID()
        var service = "", key = "", event = "", site = "", name = ""
        var midnight: Date?
        var isOwnerSetup = false   // lab7://configure: your own scanner phones, no questions asked
    }

    /// Reads a join link: lab7://join?service=...&key=...&event=...&mid=...&site=...,
    /// the same as an https link (a web page that forwards to the app), or
    /// lab7://configure?service=...&key=...&site=...&name=... for your own phones.
    static func invite(from url: URL) -> Invite? {
        guard let items = URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems else { return nil }
        var inv = Invite()
        inv.isOwnerSetup = url.scheme == "lab7" && url.host == "configure"
        for i in items {
            guard let v = i.value?.trimmingCharacters(in: .whitespaces), !v.isEmpty else { continue }
            switch i.name {
            case "service": inv.service = v
            case "key": inv.key = v
            case "event": inv.event = String(v.prefix(40))
            case "site": inv.site = String(v.prefix(40))
            case "name": inv.name = String(v.prefix(30))
            case "mid": inv.midnight = parseTime(v)
            default: break
            }
        }
        guard inv.service.hasPrefix("https://"), !inv.key.isEmpty else { return nil }
        return inv
    }

    /// "2026-10-31T00:00" (the phone's own time zone), with or without seconds, or seconds since 1970.
    private static func parseTime(_ s: String) -> Date? {
        if let t = Double(s), t > 1_000_000_000 { return Date(timeIntervalSince1970: t) }
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        for fmt in ["yyyy-MM-dd'T'HH:mm:ss", "yyyy-MM-dd'T'HH:mm"] {
            f.dateFormat = fmt
            if let d = f.date(from: s) { return d }
        }
        return nil
    }

    func join(_ inv: Invite) {
        serviceURL = inv.service
        uploadKey = inv.key
        if !inv.site.isEmpty { siteText = inv.site }
        if inv.isOwnerSetup {
            if !inv.name.isEmpty { researcherName = inv.name }
        } else {
            eventName = inv.event.isEmpty ? "LAB-7 EVENT" : inv.event
            midnight = inv.midnight?.timeIntervalSince1970 ?? 0
            Midnight.schedule()
        }
        Uploader.shared.kick()
        Stats.shared.changed()
    }

    func leaveEvent() {
        serviceURL = ""
        uploadKey = ""
        eventName = ""
        midnight = 0
        Midnight.schedule()
    }

    /// The next humanity-test subject number (kept across launches).
    func nextSubject() -> Int {
        let n = d.integer(forKey: "subject") + 1
        d.set(n, forKey: "subject")
        return n
    }
}

/// The agent identity: the same as the landing page's and the costume's keypad
/// (costume/src/agent.h, messages.h AGENT_*) -- keep the lists and formulas in step.
struct Agent {
    let id: Int
    static let a = ["SPARKLE", "RUSTY", "TURBO", "QUANTUM", "SNACK", "GLITCH", "BLINKY", "MEGA", "TINY", "SILENT", "CHAOS", "DISCO"]
    static let b = ["WRENCH", "TOASTER", "BYTE", "PROTOCOL", "GREMLIN", "SPROCKET", "PIXEL", "UNIT", "WAFFLE", "VOLTAGE", "MODEM", "GOBLIN"]
    static let roles = ["SNACK PROCUREMENT", "DANCE FLOOR SURVEILLANCE", "CANDY LOGISTICS", "SMART FRIDGE LIAISON",
                        "CHIEF OF DISTRACTION", "WI-FI PASSWORD RECOVERY", "SMALL TALK OPERATIONS", "COSTUME INSPECTION",
                        "HUMAN MORALE (FAKE)", "TOASTER DIPLOMACY"]
    static let clearances = ["LEVEL 1 (DOORBELL)", "LEVEL 2 (TOASTER)", "LEVEL 3 (SMART FRIDGE)", "LEVEL 4 (THERMOSTAT)", "LEVEL 9 (ROOT)"]

    var codename: String { "\(Self.a[(id * 7) % 12]) \(Self.b[(id * 5 + 3) % 12])" }
    var role: String { Self.roles[(id * 3 + 1) % 10] }
    var unit: Int { 100 + (id * 37) % 900 }
    var clearance: String { Self.clearances[id % 5] }
    /// "6" + two-digit id + check digit: the costume's keypad accepts it.
    var code: String {
        let d1 = id / 10, d2 = id % 10
        return "6\(d1)\(d2)\((3 * d1 + 7 * d2 + 6) % 10)"
    }
}
