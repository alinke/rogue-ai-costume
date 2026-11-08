// The automatic brake light: the phone's GPS speed, and a brake when it drops
// fast. While slowing, "brake on" is re-sent with each reading; the jacket
// holds it 1.5 s after the last, so it goes off by itself once you stop slowing.
//
// GPS speed updates about once a second, so it lights ~1 s after you start
// braking -- fine for traffic behind you, not instant like a wired brake
// light. Works with the phone in a pocket or on the bars (it doesn't need to
// know which way the phone faces), and with the screen locked.
import CoreLocation
import Foundation

final class BikeBrake: NSObject, ObservableObject {
    @Published private(set) var enabled = false
    @Published private(set) var speed: Double?        // m/s, nil = no fix
    @Published private(set) var braking = false
    @Published private(set) var denied = false

    var onBrake: ((Bool) -> Void)?

    /// Slowing at least this fast (m/s^2) from at least minSpeed counts as braking.
    static let decel = 1.1
    static let minSpeed = 2.5   // ~9 km/h, ~5.6 mph

    private let manager = CLLocationManager()
    private var samples: [(t: Date, v: Double)] = []

    override init() {
        super.init()
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyBestForNavigation
        manager.activityType = .fitness
        manager.pausesLocationUpdatesAutomatically = false
    }

    func setEnabled(_ on: Bool) {
        enabled = on
        if on {
            // While In Use is enough: started while the app is open, updates keep
            // coming with the screen locked (the blue location pill shows).
            manager.requestWhenInUseAuthorization()
            manager.allowsBackgroundLocationUpdates = true
            manager.showsBackgroundLocationIndicator = true
            manager.startUpdatingLocation()
        } else {
            manager.stopUpdatingLocation()
            manager.allowsBackgroundLocationUpdates = false
            samples.removeAll()
            speed = nil
            if braking { braking = false; onBrake?(false) }
        }
    }
}

extension BikeBrake: CLLocationManagerDelegate {
    func locationManagerDidChangeAuthorization(_ m: CLLocationManager) {
        denied = m.authorizationStatus == .denied || m.authorizationStatus == .restricted
    }

    func locationManager(_ m: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        for loc in locations where loc.speed >= 0 && loc.speedAccuracy >= 0 && loc.speedAccuracy < 3 {
            samples.append((loc.timestamp, loc.speed))
        }
        let now = Date()
        samples.removeAll { now.timeIntervalSince($0.t) > 3 }
        guard let last = samples.last else { speed = nil; return }
        speed = last.v
        // Compare with the reading ~1.5 s back.
        let wasBraking = braking
        if let ref = samples.first(where: { last.t.timeIntervalSince($0.t) <= 2.2 && last.t.timeIntervalSince($0.t) >= 0.8 }) {
            let dt = last.t.timeIntervalSince(ref.t)
            let slowing = (ref.v - last.v) / dt
            braking = ref.v >= Self.minSpeed && slowing >= Self.decel
        } else {
            braking = false
        }
        if braking { onBrake?(true) }
        else if wasBraking { onBrake?(false) }
    }
}
