// The evidence feed from your photo service (GET /photos, the same one the
// Halloween page uses): the latest photos for the slideshow and the scoreboard
// (verified humans vs assimilated), refreshed every 30 s.
import UIKit

struct EvidencePhoto {
    let url: URL
    let label: String
    let fromAI: Bool
}

final class Feed {
    private(set) var photos: [EvidencePhoto] = []
    private(set) var verified = 0
    private(set) var assimilated = 0
    private(set) var online = false
    private var images: [URL: UIImage] = [:]
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
                self.photos = Array(all.prefix(12).map { $0.1 })
                let keep = Set(self.photos.map { $0.url })
                self.images = self.images.filter { keep.contains($0.key) }
            }
        }.resume()
    }

    /// The photo's image if downloaded; starts the download otherwise.
    func image(_ p: EvidencePhoto) -> UIImage? {
        if let i = images[p.url] { return i }
        if !loading.contains(p.url) {
            loading.insert(p.url)
            URLSession.shared.dataTask(with: p.url) { [weak self] data, _, _ in
                let img = data.flatMap(UIImage.init(data:))
                DispatchQueue.main.async {
                    self?.loading.remove(p.url)
                    if let img = img { self?.images[p.url] = img }
                }
            }.resume()
        }
        return nil
    }
}
