// The evidence feed from your photo service (GET /photos, the same one the
// Halloween page uses): every photo of the night (newest first), the
// scoreboard (verified humans vs assimilated) and the guests found to be AI,
// refreshed every 30 s. Images download on demand into a small cache.
import UIKit

struct EvidencePhoto {
    let url: URL
    let label: String
    let fromAI: Bool
    var rogue: Bool { label.hasPrefix("COMPROMISED") }
}

final class Feed {
    private(set) var photos: [EvidencePhoto] = []      // the latest 12 (the monitor's slideshow)
    private(set) var all: [EvidencePhoto] = []         // everything, newest first (the evidence wall)
    private(set) var rogues: [EvidencePhoto] = []      // guests found to be AI, newest first
    private(set) var verified = 0
    private(set) var assimilated = 0
    private(set) var online = false
    private var images: [URL: UIImage] = [:]
    private var lru: [URL] = []          // most recently used last
    private let cacheSize = 30
    private var loading = Set<URL>()
    private var timer: Timer?

    func start() {
        fetch()
        timer = Timer.scheduledTimer(withTimeInterval: 30, repeats: true) { [weak self] _ in self?.fetch() }
    }

    func fetch() {
        guard let base = Config.shared.serviceBase, let url = URL(string: base + "/photos") else { return }
        URLSession.shared.dataTask(with: url) { [weak self] data, resp, _ in
            guard let self = self, let data = data, (resp as? HTTPURLResponse)?.statusCode == 200,
                  let json = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else {
                DispatchQueue.main.async { self?.online = false }
                return
            }
            func list(_ k: String) -> [[String: Any]] { json[k] as? [[String: Any]] ?? [] }
            let ai = list("ai"), lab = list("lab")
            let bad = lab.filter { ($0["label"] as? String ?? "").hasPrefix("COMPROMISED") }
            var all: [(String, EvidencePhoto)] = []
            for (entries, fromAI) in [(ai, true), (lab, false)] {
                for e in entries {
                    guard let u = (e["url"] as? String).flatMap(URL.init(string:)) else { continue }
                    all.append((e["t"] as? String ?? "", EvidencePhoto(url: u, label: e["label"] as? String ?? "", fromAI: fromAI)))
                }
            }
            all.sort { $0.0 > $1.0 }
            DispatchQueue.main.async {
                self.online = true
                self.verified = lab.count - bad.count
                self.assimilated = ai.count + bad.count
                self.all = all.map { $0.1 }
                self.photos = Array(self.all.prefix(12))
                self.rogues = self.all.filter { $0.rogue }
            }
        }.resume()
    }

    /// The photo's image if downloaded; starts the download otherwise.
    func image(_ p: EvidencePhoto) -> UIImage? {
        if let i = images[p.url] {
            if lru.last != p.url { lru.removeAll { $0 == p.url }; lru.append(p.url) }
            return i
        }
        if !loading.contains(p.url) {
            loading.insert(p.url)
            URLSession.shared.dataTask(with: p.url) { [weak self] data, _, _ in
                let img = data.flatMap(UIImage.init(data:))
                DispatchQueue.main.async {
                    self?.loading.remove(p.url)
                    guard let self = self, let img = img else { return }
                    self.images[p.url] = img
                    self.lru.append(p.url)
                    while self.lru.count > self.cacheSize { self.images[self.lru.removeFirst()] = nil }
                }
            }.resume()
        }
        return nil
    }
}
