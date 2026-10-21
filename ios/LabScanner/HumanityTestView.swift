// The humanity test: the camera as a live viewfinder -- the rear one by
// default, or the front one (mirrored, like a selfie) with the flip button --
// tap to scan the guest, a white flash, then their LAB-7 ID card.
import AVFoundation
import SwiftUI

final class Camera: NSObject, ObservableObject, AVCapturePhotoCaptureDelegate {
    let session = AVCaptureSession()
    private let output = AVCapturePhotoOutput()
    private var done: ((UIImage?) -> Void)?
    @Published var denied = false

    private let work = DispatchQueue(label: "camera")

    func start(front: Bool) {
        AVCaptureDevice.requestAccess(for: .video) { ok in
            DispatchQueue.main.async { self.denied = !ok }
            guard ok else { return }
            self.work.async { self.configure(front: front) }
        }
    }

    func use(front: Bool) { work.async { self.configure(front: front) } }

    private func configure(front: Bool) {
        session.beginConfiguration()
        session.sessionPreset = .photo
        for i in session.inputs { session.removeInput(i) }
        if let dev = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: front ? .front : .back),
           let input = try? AVCaptureDeviceInput(device: dev), session.canAddInput(input) {
            session.addInput(input)
        }
        if session.outputs.isEmpty, session.canAddOutput(output) { session.addOutput(output) }
        session.commitConfiguration()
        if let conn = output.connection(with: .video) {
            // Front: mirrored like the preview, so the card shows what the
            // viewfinder showed. Rear: as the world is.
            if conn.isVideoMirroringSupported {
                conn.automaticallyAdjustsVideoMirroring = false
                conn.isVideoMirrored = front
            }
            if conn.isVideoRotationAngleSupported(90) { conn.videoRotationAngle = 90 }  // portrait
        }
        if !session.isRunning { session.startRunning() }
    }

    func stop() { work.async { self.session.stopRunning() } }

    func capture(_ then: @escaping (UIImage?) -> Void) {
        done = then
        output.capturePhoto(with: AVCapturePhotoSettings(), delegate: self)
    }

    func photoOutput(_ o: AVCapturePhotoOutput, didFinishProcessingPhoto p: AVCapturePhoto, error: Error?) {
        let img = p.fileDataRepresentation().flatMap { UIImage(data: $0) }
        DispatchQueue.main.async { self.done?(img); self.done = nil }
    }
}

struct CameraPreview: UIViewRepresentable {
    let session: AVCaptureSession
    func makeUIView(context: Context) -> PreviewView {
        let v = PreviewView()
        v.layer.session = session
        v.layer.videoGravity = .resizeAspectFill
        return v
    }
    func updateUIView(_ v: PreviewView, context: Context) {}
    final class PreviewView: UIView {
        override class var layerClass: AnyClass { AVCaptureVideoPreviewLayer.self }
        override var layer: AVCaptureVideoPreviewLayer { super.layer as! AVCaptureVideoPreviewLayer }
    }
}

struct HumanityTestView: View {
    @Environment(\.dismiss) private var dismiss
    @StateObject private var camera = Camera()
    @ObservedObject private var settings = Settings.shared
    @State private var flash = false
    @State private var card: IDCard.Result?
    @State private var banner = false
    @State private var lastTouch = Date()
    @State private var blink = false
    private let tick = Timer.publish(every: 0.4, on: .main, in: .common).autoconnect()

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            if let card {
                Image(uiImage: card.image).resizable().scaledToFit().ignoresSafeArea()
                if banner {
                    VStack {
                        Text(card.human ? "ADDED TO THE SURVIVOR REGISTRY" : "SUBJECT FLAGGED: AI EXPOSURE")
                            .font(.system(size: 20, weight: .bold)).foregroundColor(.white)
                            .frame(maxWidth: .infinity).padding(.vertical, 22)
                            .background(Color(hex: card.human ? 0x1E8449 : 0xC0392B))
                        Spacer()
                    }
                    .ignoresSafeArea()
                }
            } else {
                CameraPreview(session: camera.session).ignoresSafeArea()
                hud
                if camera.denied {
                    Text("CAMERA ACCESS DENIED.\nAllow it in Settings > LAB-7.").font(mono(18))
                        .foregroundColor(.labRed).multilineTextAlignment(.center)
                }
            }
            if flash { Color.white.ignoresSafeArea() }
        }
        .statusBarHidden()
        .contentShape(Rectangle())
        .onTapGesture { if card == nil { shoot() } }
        .onAppear { camera.start(front: settings.frontCamera) }
        .onDisappear { camera.stop() }
        .onReceive(tick) { t in
            blink.toggle()
            if card == nil && t.timeIntervalSince(lastTouch) > 30 { dismiss() }  // nobody: close (camera off)
        }
    }

    private var hud: some View {
        ZStack {
            Brackets().stroke(blink ? Color.labRed : Color.labAmber, lineWidth: 6).padding(28).ignoresSafeArea()
            VStack {
                HStack {
                    Text("LAB-7 // HUMANITY TEST").font(mono(16)).foregroundColor(.labGreen)
                    Spacer()
                    Button {  // rear / front camera (remembered)
                        lastTouch = Date()
                        settings.frontCamera.toggle()
                        camera.use(front: settings.frontCamera)
                    } label: {
                        Image(systemName: "arrow.triangle.2.circlepath.camera").font(.system(size: 26, weight: .bold))
                            .foregroundColor(.labCyan).frame(width: 64, height: 64)
                            .background(RoundedRectangle(cornerRadius: 14).fill(Color(hex: 0x002a33)))
                    }
                    Button { dismiss() } label: {
                        Image(systemName: "xmark").font(.system(size: 28, weight: .bold)).foregroundColor(.labRed)
                            .frame(width: 64, height: 64)
                            .background(RoundedRectangle(cornerRadius: 14).fill(Color(hex: 0x300004)))
                    }
                }
                .padding(.horizontal, 44).padding(.top, 20)
                Spacer()
                Text("TAP TO SCAN SUBJECT").font(mono(26)).foregroundColor(blink ? .labWhite : .labGreen)
                    .padding(.bottom, 50)
            }
        }
    }

    private func shoot() {
        lastTouch = Date()
        Sound.shared.play(.beep)
        flash = true
        camera.capture { img in
            flash = false
            guard let img else { return }
            let r = IDCard.make(photo: img, settings: Settings.shared)
            Uploader.shared.add(r.image, label: r.label)  // the card, before the on-screen banner
            card = r
            Sound.shared.play(r.human ? .lock : .fail)
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.6) { banner = true }
            DispatchQueue.main.asyncAfter(deadline: .now() + 4.2) { dismiss() }
        }
    }
}

/// Corner brackets around the frame.
struct Brackets: Shape {
    func path(in r: CGRect) -> Path {
        var p = Path()
        let l: CGFloat = 70
        for (c, dx, dy) in [(CGPoint(x: r.minX, y: r.minY), 1.0, 1.0), (CGPoint(x: r.maxX, y: r.minY), -1.0, 1.0),
                            (CGPoint(x: r.minX, y: r.maxY), 1.0, -1.0), (CGPoint(x: r.maxX, y: r.maxY), -1.0, -1.0)] {
            p.move(to: CGPoint(x: c.x + dx * l, y: c.y))
            p.addLine(to: c)
            p.addLine(to: CGPoint(x: c.x, y: c.y + dy * l))
        }
        return p
    }
}
