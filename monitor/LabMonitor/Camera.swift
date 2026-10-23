// The checkpoint camera: the front camera, always running, mirrored like a
// mirror. Every few frames Apple's face detection (Vision) finds faces;
// snapshot() gives the latest frame as a photo.
import AVFoundation
import UIKit
import Vision

final class Camera: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    let session = AVCaptureSession()
    let preview: AVCaptureVideoPreviewLayer
    private let output = AVCaptureVideoDataOutput()
    private let q = DispatchQueue(label: "camera")
    private let lock = NSLock()
    private var latest: CVPixelBuffer?
    private var frameN = 0
    private var _faces: [CGRect] = []
    private let ci = CIContext()
    private(set) var available = false

    /// Faces in the latest checked frame: 0...1, top-left origin, as on screen
    /// (mirrored), biggest first.
    var faces: [CGRect] {
        lock.lock(); defer { lock.unlock() }
        return _faces
    }

    override init() {
        preview = AVCaptureVideoPreviewLayer(session: session)
        preview.videoGravity = .resizeAspectFill
        super.init()
    }

    func start(_ orientation: AVCaptureVideoOrientation) {
        AVCaptureDevice.requestAccess(for: .video) { ok in
            guard ok else { return }
            self.q.async { self.configure(orientation) }
        }
    }

    private func configure(_ orientation: AVCaptureVideoOrientation) {
        guard let dev = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .front),
              let input = try? AVCaptureDeviceInput(device: dev) else { return }
        session.beginConfiguration()
        session.sessionPreset = session.canSetSessionPreset(.hd1280x720) ? .hd1280x720 : .vga640x480
        if session.canAddInput(input) { session.addInput(input) }
        output.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA]
        output.alwaysDiscardsLateVideoFrames = true
        output.setSampleBufferDelegate(self, queue: q)
        if session.canAddOutput(output) { session.addOutput(output) }
        session.commitConfiguration()
        applyOrientation(orientation)
        session.startRunning()
        available = true
    }

    /// Frames upright for the screen's orientation and mirrored, so they match
    /// what the preview shows.
    func setOrientation(_ o: AVCaptureVideoOrientation) {
        preview.connection?.videoOrientation = o
        q.async { self.applyOrientation(o) }
    }

    private func applyOrientation(_ o: AVCaptureVideoOrientation) {
        if let c = output.connection(with: .video) {
            if c.isVideoOrientationSupported { c.videoOrientation = o }
            if c.isVideoMirroringSupported {
                c.automaticallyAdjustsVideoMirroring = false
                c.isVideoMirrored = true
            }
        }
        DispatchQueue.main.async { self.preview.connection?.videoOrientation = o }
    }

    func captureOutput(_ o: AVCaptureOutput, didOutput sb: CMSampleBuffer, from c: AVCaptureConnection) {
        guard let px = CMSampleBufferGetImageBuffer(sb) else { return }
        lock.lock(); latest = px; lock.unlock()
        frameN += 1
        guard frameN % 5 == 0 else { return }  // ~6 checks a second is plenty
        let req = VNDetectFaceRectanglesRequest()
        try? VNImageRequestHandler(cvPixelBuffer: px, orientation: .up, options: [:]).perform([req])
        let found = (req.results as? [VNFaceObservation] ?? [])
            .map { b -> CGRect in
                let r = b.boundingBox  // bottom-left origin
                return CGRect(x: r.minX, y: 1 - r.maxY, width: r.width, height: r.height)
            }
            .sorted { $0.width > $1.width }
        lock.lock(); _faces = found; lock.unlock()
    }

    func stop() { q.async { if self.session.isRunning { self.session.stopRunning() } } }
    func resume() { q.async { if !self.session.isRunning && !self.session.inputs.isEmpty { self.session.startRunning() } } }

    /// The latest frame, as on screen.
    func snapshot() -> UIImage? {
        lock.lock(); let px = latest; lock.unlock()
        guard let p = px, let cg = ci.createCGImage(CIImage(cvPixelBuffer: p), from: CIImage(cvPixelBuffer: p).extent)
        else { return nil }
        return UIImage(cgImage: cg)
    }
}
