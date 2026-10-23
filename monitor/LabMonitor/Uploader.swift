// Upload queue for the checkpoint cards, like the iPhone app's: each card waits
// in Documents/queue and goes to your photo service (set=lab, so it lands in the
// LAB-7 evidence on the Halloween page) whenever it can; a copy stays in
// Documents/photos. Each card has an id, so a retry never doubles it.
import UIKit

final class Uploader {
    static let shared = Uploader()
    private(set) var pending = 0
    private(set) var lastOK = false
    private let fm = FileManager.default
    private let queue: URL
    private let keep: URL
    private var busy = false

    private init() {
        let docs = fm.urls(for: .documentDirectory, in: .userDomainMask)[0]
        queue = docs.appendingPathComponent("queue", isDirectory: true)
        keep = docs.appendingPathComponent("photos", isDirectory: true)
        try? fm.createDirectory(at: queue, withIntermediateDirectories: true)
        try? fm.createDirectory(at: keep, withIntermediateDirectories: true)
        refreshCount()
        Timer.scheduledTimer(withTimeInterval: 20, repeats: true) { [weak self] _ in self?.kick() }
    }

    /// Queues the card; returns its id (the photo service keeps it under that
    /// name, so /p/<id> finds it -- the QR code on the result screen).
    @discardableResult
    func add(_ image: UIImage, label: String) -> String? {
        guard let jpg = image.jpegData(compressionQuality: 0.85) else { return nil }
        let id = String(format: "ipad%08x-%08x", UInt32(Date().timeIntervalSince1970), UInt32.random(in: 0...UInt32.max))
        try? jpg.write(to: keep.appendingPathComponent("\(id).jpg"))
        try? jpg.write(to: queue.appendingPathComponent("\(id).jpg"))
        try? label.write(to: queue.appendingPathComponent("\(id).txt"), atomically: true, encoding: .utf8)
        refreshCount()
        kick()
        return id
    }

    private func files() -> [String] {
        ((try? fm.contentsOfDirectory(atPath: queue.path)) ?? []).filter { $0.hasSuffix(".jpg") }.sorted()
    }

    private func refreshCount() { pending = files().count }

    func kick() {
        guard !busy, let base = Config.shared.serviceBase, Config.shared.uploadConfigured,
              let f = files().first, let url = URL(string: base + "/upload?set=lab") else { return }
        let id = String(f.dropLast(4))
        let jpgURL = queue.appendingPathComponent(f), txtURL = queue.appendingPathComponent("\(id).txt")
        guard let data = try? Data(contentsOf: jpgURL) else { return }
        let label = (try? String(contentsOf: txtURL, encoding: .utf8)) ?? ""
        var req = URLRequest(url: url, timeoutInterval: 30)
        req.httpMethod = "POST"
        req.setValue("image/jpeg", forHTTPHeaderField: "Content-Type")
        req.setValue(Config.shared.uploadKey, forHTTPHeaderField: "X-Key")
        req.setValue(label, forHTTPHeaderField: "X-Label")
        req.setValue(id, forHTTPHeaderField: "X-Id")
        busy = true
        URLSession.shared.uploadTask(with: req, from: data) { [weak self] _, resp, _ in
            let code = (resp as? HTTPURLResponse)?.statusCode ?? -1
            DispatchQueue.main.async {
                guard let self = self else { return }
                self.busy = false
                self.lastOK = code == 200
                if code == 200 || (400..<500).contains(code) {  // done, or never going to be accepted
                    try? self.fm.removeItem(at: jpgURL)
                    try? self.fm.removeItem(at: txtURL)
                    self.refreshCount()
                    if code == 200 { self.kick() }
                }
            }
        }.resume()
    }
}
