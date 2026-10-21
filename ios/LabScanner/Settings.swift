// Everything the ESP32 scanner keeps in secrets.h / config.h, as in-app settings
// (stored on the phone, never in the repo).
import Foundation

final class Settings: ObservableObject {
    static let shared = Settings()
    private let d = UserDefaults.standard

    // Distance zones (dBm of the costume's beacon) -- tune at the party.
    @Published var nearDbm: Int { didSet { d.set(nearDbm, forKey: "nearDbm") } }
    @Published var closeDbm: Int { didSet { d.set(closeDbm, forKey: "closeDbm") } }
    @Published var soundOn: Bool { didSet { d.set(soundOn, forKey: "soundOn") } }
    // The humanity test's camera: rear by default (the researcher points the
    // phone at the guest); the viewfinder's flip button switches and remembers.
    @Published var frontCamera: Bool { didSet { d.set(frontCamera, forKey: "frontCamera") } }
    // The humanity test.
    @Published var researcherName: String { didSet { d.set(researcherName, forKey: "researcherName") } }
    @Published var compromisedPercent: Int { didSet { d.set(compromisedPercent, forKey: "compromisedPercent") } }
    // Photo upload (optional): your photo service, see ../../cloud/README.md.
    @Published var serviceURL: String { didSet { d.set(serviceURL, forKey: "serviceURL") } }
    @Published var uploadKey: String { didSet { d.set(uploadKey, forKey: "uploadKey") } }
    @Published var siteText: String { didSet { d.set(siteText, forKey: "siteText") } }

    static let hysteresisDb = 4
    static let containCooldown: TimeInterval = 12

    var uploadConfigured: Bool { !serviceURL.isEmpty && !uploadKey.isEmpty }

    private init() {
        d.register(defaults: ["nearDbm": -70, "closeDbm": -58, "soundOn": true,
                              "researcherName": "LAB-7 FIELD RESEARCHER", "compromisedPercent": 20,
                              "siteText": "THE ROGUE.AI WALL"])
        nearDbm = d.integer(forKey: "nearDbm")
        closeDbm = d.integer(forKey: "closeDbm")
        soundOn = d.bool(forKey: "soundOn")
        frontCamera = d.bool(forKey: "frontCamera")
        researcherName = d.string(forKey: "researcherName") ?? ""
        compromisedPercent = d.integer(forKey: "compromisedPercent")
        serviceURL = d.string(forKey: "serviceURL") ?? ""
        uploadKey = d.string(forKey: "uploadKey") ?? ""
        siteText = d.string(forKey: "siteText") ?? ""
    }

    /// The next humanity-test subject number (kept across launches).
    func nextSubject() -> Int {
        let n = d.integer(forKey: "subject") + 1
        d.set(n, forKey: "subject")
        return n
    }
}
