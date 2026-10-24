// The event's photo service (../../cloud/service/main.py), as the guest app
// uses it: the evidence wall's feed, the agent leaderboard, reporting a photo.
// Uploads go through Uploader.
import Foundation
import UserNotifications

struct WallPhoto: Decodable, Identifiable, Hashable {
    let name: String    // "lab/....jpg" (survivor registry) or "ai/....jpg" (the AI's specimens)
    let url: String
    let t: String
    let label: String
    let dev: String?    // a short tag of the phone that took it (for "hide this phone")
    var id: String { name }
    var isAI: Bool { name.hasPrefix("ai/") }
}

struct LeaderEntry: Decodable, Hashable {
    let name: String
    let contains: Int
    let tests: Int
}

enum LabAPI {
    private static var base: String? {
        var b = Settings.shared.serviceURL.trimmingCharacters(in: .whitespaces)
        while b.hasSuffix("/") { b.removeLast() }
        return b.isEmpty ? nil : b
    }

    private static func request(_ path: String, method: String = "GET", json: [String: Any]? = nil) -> URLRequest? {
        guard let b = base, let url = URL(string: b + path) else { return nil }
        var r = URLRequest(url: url, timeoutInterval: 20)
        r.httpMethod = method
        r.setValue(Settings.shared.uploadKey, forHTTPHeaderField: "X-Key")
        r.setValue(Settings.shared.deviceId, forHTTPHeaderField: "X-Device")
        if let json {
            r.setValue("application/json", forHTTPHeaderField: "Content-Type")
            r.httpBody = try? JSONSerialization.data(withJSONObject: json)
        }
        return r
    }

    /// Both sets, newest first.
    static func photos() async throws -> (lab: [WallPhoto], ai: [WallPhoto]) {
        struct Feed: Decodable { let ai: [WallPhoto]; let lab: [WallPhoto] }
        guard var r = request("/photos") else { return ([], []) }
        r.cachePolicy = .reloadIgnoringLocalCacheData
        let (data, _) = try await URLSession.shared.data(for: r)
        let f = try JSONDecoder().decode(Feed.self, from: data)
        return (f.lab, f.ai)
    }

    static func leaders() async throws -> (agents: [LeaderEntry], total: Int) {
        struct Board: Decodable { let agents: [LeaderEntry]; let total: Int? }
        guard var r = request("/leaders") else { return ([], 0) }
        r.cachePolicy = .reloadIgnoringLocalCacheData
        let (data, _) = try await URLSession.shared.data(for: r)
        let b = try JSONDecoder().decode(Board.self, from: data)
        return (b.agents, b.total ?? b.agents.count)
    }

    @discardableResult
    static func report(_ p: WallPhoto) async -> Bool {
        guard let r = request("/report", method: "POST", json: ["name": p.name]) else { return false }
        let resp = try? await URLSession.shared.data(for: r)
        return (resp?.1 as? HTTPURLResponse)?.statusCode == 200
    }

    /// Deletes a photo this phone uploaded (the service checks it's ours).
    @discardableResult
    static func deleteOwn(_ p: WallPhoto) async -> Bool {
        guard let r = request("/delete-own", method: "POST", json: ["name": p.name]) else { return false }
        let resp = try? await URLSession.shared.data(for: r)
        return (resp?.1 as? HTTPURLResponse)?.statusCode == 200
    }

    static func postStats() async -> Int {
        let s = Settings.shared
        guard let r = request("/stats", method: "POST", json: ["device": s.deviceId, "name": s.leaderboardName,
                                                              "contains": s.containCount, "tests": s.testCount])
        else { return -1 }
        let resp = try? await URLSession.shared.data(for: r)
        return (resp?.1 as? HTTPURLResponse)?.statusCode ?? -1
    }
}

/// Sends this phone's score to the leaderboard a moment after it changes (one
/// request for a burst of CONTAIN presses), and retries later if offline.
final class Stats {
    static let shared = Stats()
    private var pending: DispatchWorkItem?

    func changed() {
        pending?.cancel()
        let w = DispatchWorkItem {
            guard Settings.shared.joined else { return }
            Task {
                let code = await LabAPI.postStats()
                if code != 200 && code != 403 {   // offline or too soon: try again later
                    await MainActor.run { self.changed() }
                }
            }
        }
        pending = w
        DispatchQueue.main.asyncAfter(deadline: .now() + 4, execute: w)
    }
}

/// The local notification when the event's big moment comes (no push server).
enum Midnight {
    static let id = "lab7-midnight"

    static func schedule() {
        let c = UNUserNotificationCenter.current()
        c.removePendingNotificationRequests(withIdentifiers: [id, id + "-warn"])
        guard let at = Settings.shared.midnightDate, at > Date() else { return }
#if DEBUG
        if ProcessInfo.processInfo.arguments.contains("-noPrompt") { return }  // simulator screenshots
#endif
        c.requestAuthorization(options: [.alert, .sound]) { ok, _ in
            guard ok else { return }
            add(c, id: id, at: at, title: "IT IS HAPPENING.",
                body: "The entity's midnight protocol has started. Find it. Fire everything.")
            let warn = at.addingTimeInterval(-15 * 60)
            if warn > Date() {
                add(c, id: id + "-warn", at: warn, title: "LAB-7: 15 MINUTES",
                    body: "The entity is preparing something for midnight. Stay close to a scanner.")
            }
        }
    }

    private static func add(_ c: UNUserNotificationCenter, id: String, at: Date, title: String, body: String) {
        let n = UNMutableNotificationContent()
        n.title = title
        n.body = body
        n.sound = .default
        let comps = Calendar.current.dateComponents([.year, .month, .day, .hour, .minute, .second], from: at)
        c.add(UNNotificationRequest(identifier: id, content: n,
                                    trigger: UNCalendarNotificationTrigger(dateMatching: comps, repeats: false)))
    }
}
