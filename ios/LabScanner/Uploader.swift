// Upload queue for the humanity-test cards, like the ESP32 scanner's
// (../../scanner/src/uploader.cpp): each card waits in the app's Documents
// folder and goes to your photo service (Settings) whenever it can; a keepsake
// copy stays in Documents/photos (visible in the Files app). Each card has an
// id, so a retry never makes a second copy on the page.
import Foundation
import UIKit

final class Uploader: ObservableObject {
    static let shared = Uploader()
    @Published private(set) var pending = 0

    private let fm = FileManager.default
    private let queue: URL
    private let keep: URL
    private var running = false

    private init() {
        let docs = fm.urls(for: .documentDirectory, in: .userDomainMask)[0]
        queue = docs.appendingPathComponent("queue", isDirectory: true)
        keep = docs.appendingPathComponent("photos", isDirectory: true)
        try? fm.createDirectory(at: queue, withIntermediateDirectories: true)
        try? fm.createDirectory(at: keep, withIntermediateDirectories: true)
        refreshCount()
        Timer.scheduledTimer(withTimeInterval: 15, repeats: true) { [weak self] _ in self?.kick() }
    }

    func add(_ image: UIImage, label: String) {
        guard let jpg = image.jpegData(compressionQuality: 0.82) else { return }
        let id = String(format: "ios%08x-%08x", UInt32(Date().timeIntervalSince1970), UInt32.random(in: 0...UInt32.max))
        try? jpg.write(to: keep.appendingPathComponent("\(id).jpg"))
        if Settings.shared.uploadConfigured {
            try? jpg.write(to: queue.appendingPathComponent("\(id).jpg"))
            try? label.write(to: queue.appendingPathComponent("\(id).txt"), atomically: true, encoding: .utf8)
        }
        refreshCount()
        kick()
    }

    private func refreshCount() {
        let n = ((try? fm.contentsOfDirectory(atPath: queue.path)) ?? []).filter { $0.hasSuffix(".jpg") }.count
        DispatchQueue.main.async { self.pending = n }
    }

    private func kick() {
        guard !running, Settings.shared.uploadConfigured else { return }
        running = true
        Task {
            defer { running = false; refreshCount() }
            let files = ((try? fm.contentsOfDirectory(atPath: queue.path)) ?? []).filter { $0.hasSuffix(".jpg") }.sorted()
            for f in files {
                let id = String(f.dropLast(4))
                let jpgURL = queue.appendingPathComponent(f), txtURL = queue.appendingPathComponent("\(id).txt")
                guard let data = try? Data(contentsOf: jpgURL) else { continue }
                let label = (try? String(contentsOf: txtURL, encoding: .utf8)) ?? ""
                let code = await send(data, id: id, label: label)
                if code == 200 || (400..<500).contains(code) {  // done, or never going to be accepted
                    try? fm.removeItem(at: jpgURL)
                    try? fm.removeItem(at: txtURL)
                } else {
                    break  // offline / server trouble: try again later, in order
                }
            }
        }
    }

    private func send(_ data: Data, id: String, label: String) async -> Int {
        let s = Settings.shared
        var base = s.serviceURL.trimmingCharacters(in: .whitespaces)
        while base.hasSuffix("/") { base.removeLast() }
        guard let url = URL(string: base + "/upload?set=lab") else { return 400 }
        var req = URLRequest(url: url, timeoutInterval: 30)
        req.httpMethod = "POST"
        req.setValue("image/jpeg", forHTTPHeaderField: "Content-Type")
        req.setValue(s.uploadKey, forHTTPHeaderField: "X-Key")
        req.setValue(label, forHTTPHeaderField: "X-Label")
        req.setValue(id, forHTTPHeaderField: "X-Id")
        do {
            let (_, resp) = try await URLSession.shared.upload(for: req, from: data)
            return (resp as? HTTPURLResponse)?.statusCode ?? -1
        } catch {
            return -1
        }
    }
}
