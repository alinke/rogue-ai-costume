// Scanning the event's QR code inside the app (the join screen): the camera,
// looking for a QR code, and turning what it finds into an event invite. The
// QR codes on the party's pages (pixelcade.org/lab7-join, the AI's landing
// page) point at the page itself, so the app reads the event settings from
// that page; a QR code with the settings in it (lab7://join?..., or a page
// link with ?service=...&key=...) works directly.
import AVFoundation
import SwiftUI

struct QRScannerView: UIViewControllerRepresentable {
    var found: (String) -> Void

    func makeUIViewController(context: Context) -> ScannerController {
        let c = ScannerController()
        c.found = found
        return c
    }

    func updateUIViewController(_ c: ScannerController, context: Context) {}

    final class ScannerController: UIViewController, AVCaptureMetadataOutputObjectsDelegate {
        var found: ((String) -> Void)?
        private let session = AVCaptureSession()
        private let preview = AVCaptureVideoPreviewLayer()
        private var done = false

        override func viewDidLoad() {
            super.viewDidLoad()
            view.backgroundColor = .black
            guard let cam = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .back),
                  let input = try? AVCaptureDeviceInput(device: cam), session.canAddInput(input) else { return }
            session.addInput(input)
            let out = AVCaptureMetadataOutput()
            guard session.canAddOutput(out) else { return }
            session.addOutput(out)
            out.setMetadataObjectsDelegate(self, queue: .main)
            out.metadataObjectTypes = [.qr]
            preview.session = session
            preview.videoGravity = .resizeAspectFill
            view.layer.addSublayer(preview)
            DispatchQueue.global(qos: .userInitiated).async { self.session.startRunning() }
        }

        override func viewDidLayoutSubviews() {
            super.viewDidLayoutSubviews()
            preview.frame = view.bounds
        }

        override func viewWillDisappear(_ animated: Bool) {
            super.viewWillDisappear(animated)
            DispatchQueue.global(qos: .userInitiated).async { self.session.stopRunning() }
        }

        func metadataOutput(_ o: AVCaptureMetadataOutput, didOutput objs: [AVMetadataObject], from c: AVCaptureConnection) {
            guard !done, let s = (objs.first as? AVMetadataMachineReadableCodeObject)?.stringValue else { return }
            done = true
            UINotificationFeedbackGenerator().notificationOccurred(.success)
            found?(s)
        }
    }
}

enum EventLink {
    /// An invite from a scanned or pasted link: directly, or by reading the
    /// event settings (CONFIG) out of the party page the link points at.
    static func resolve(_ text: String) async -> Settings.Invite? {
        let s = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let url = URL(string: s) else { return nil }
        if let inv = Settings.invite(from: url) { return inv }
        guard url.scheme == "https", let (data, _) = try? await URLSession.shared.data(from: url),
              let html = String(data: data, encoding: .utf8) else { return nil }
        func field(_ name: String) -> String? {
            guard let re = try? NSRegularExpression(pattern: name + #"\s*:\s*"([^"]*)""#),
                  let m = re.firstMatch(in: html, range: NSRange(html.startIndex..., in: html)),
                  let r = Range(m.range(at: 1), in: html) else { return nil }
            return String(html[r])
        }
        // The page's join settings: service (https://...run.app), key, event, mid.
        guard let re = try? NSRegularExpression(pattern: #"service\s*:\s*"(https://[^"]+)""#),
              let m = re.firstMatch(in: html, range: NSRange(html.startIndex..., in: html)),
              let r = Range(m.range(at: 1), in: html) else { return nil }
        var c = URLComponents(string: "lab7://join")!
        c.queryItems = [URLQueryItem(name: "service", value: String(html[r])),
                        URLQueryItem(name: "key", value: field("key") ?? ""),
                        URLQueryItem(name: "event", value: field("event") ?? ""),
                        URLQueryItem(name: "mid", value: field("mid") ?? "")]
        return c.url.flatMap(Settings.invite(from:))
    }
}
