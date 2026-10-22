// Settings. Your own values (photo service, site, room name) go in
// Secrets.plist next to this file -- never committed; copy Secrets.example.plist.
// Without it the monitor still runs: no evidence feed, scans stay on the iPad.
import Foundation

final class Config {
    static let shared = Config()
    private let secrets: [String: Any]
    private let d = UserDefaults.standard

    private init() {
        if let url = Bundle.main.url(forResource: "Secrets", withExtension: "plist"),
           let dict = NSDictionary(contentsOf: url) as? [String: Any] {
            secrets = dict
        } else {
            secrets = [:]
        }
    }

    private func str(_ k: String, _ def: String) -> String {
        d.string(forKey: k) ?? (secrets[k] as? String) ?? def
    }
    private func int(_ k: String, _ def: Int) -> Int {
        d.object(forKey: k) != nil ? d.integer(forKey: k) : (secrets[k] as? Int) ?? def
    }

    var serviceURL: String { str("serviceURL", "") }
    var uploadKey: String { str("uploadKey", "") }
    var siteText: String { str("siteText", "THE ROGUE.AI WALL") }
    /// The room it stands in, on screen and on the cards ("SECTOR 3", "KITCHEN"...).
    var sector: String {
        get { str("sector", "SECTOR 3") }
        set { d.set(newValue.uppercased(), forKey: "sector") }
    }
    /// How often a guest comes out COMPROMISED (%).
    var compromisedPercent: Int { int("compromisedPercent", 20) }
    /// The costume's signal this strong (dBm): it's in the room -- intrusion.
    var intrusionDbm: Int {
        get { int("intrusionDbm", -62) }
        set { d.set(newValue, forKey: "intrusionDbm") }
    }
    /// ...and this weak (or gone) for a few seconds: it left.
    var clearDbm: Int { intrusionDbm - 10 }

    var serviceBase: String? {
        var b = serviceURL.trimmingCharacters(in: .whitespaces)
        while b.hasSuffix("/") { b.removeLast() }
        return b.isEmpty ? nil : b
    }
    var uploadConfigured: Bool { serviceBase != nil && !uploadKey.isEmpty }

    /// Checkpoint subject numbers, kept across launches.
    func nextSubject() -> Int {
        let n = d.integer(forKey: "subject") + 1
        d.set(n, forKey: "subject")
        return n
    }
}
