// The workout session that gives a live heart rate (HealthKit), and the link
// to the iPhone (WatchConnectivity): each new reading goes over as ["bpm": n].
import Foundation
import HealthKit
import WatchConnectivity

final class WatchHeart: NSObject, ObservableObject {
    @Published private(set) var bpm = 0
    @Published private(set) var running = false
    @Published private(set) var phoneReachable = false
    @Published private(set) var error: String?

    private let store = HKHealthStore()
    private var session: HKWorkoutSession?
    private var builder: HKLiveWorkoutBuilder?
    private let heartType = HKQuantityType(.heartRate)

    override init() {
        super.init()
        if WCSession.isSupported() {
            WCSession.default.delegate = self
            WCSession.default.activate()
        }
    }

    func start() {
        error = nil
        store.requestAuthorization(toShare: [HKObjectType.workoutType()], read: [heartType]) { ok, err in
            DispatchQueue.main.async {
                if ok { self.begin() } else { self.error = err?.localizedDescription ?? "Health access was declined" }
            }
        }
    }

    func stop() {
        session?.end()
        builder?.endCollection(withEnd: Date()) { [weak self] _, _ in
            self?.builder?.discardWorkout()   // nothing saved to Health
        }
        running = false
    }

    private func begin() {
        let config = HKWorkoutConfiguration()
        config.activityType = .other
        config.locationType = .unknown
        do {
            let s = try HKWorkoutSession(healthStore: store, configuration: config)
            let b = s.associatedWorkoutBuilder()
            b.dataSource = HKLiveWorkoutDataSource(healthStore: store, workoutConfiguration: config)
            s.delegate = self
            b.delegate = self
            session = s
            builder = b
            let now = Date()
            s.startActivity(with: now)
            b.beginCollection(withStart: now) { _, _ in }
            running = true
        } catch {
            self.error = error.localizedDescription
        }
    }

    private func send(_ value: Int) {
        let wc = WCSession.default
        guard wc.activationState == .activated, wc.isReachable else { return }
        wc.sendMessage(["bpm": value], replyHandler: nil, errorHandler: nil)
    }
}

extension WatchHeart: HKWorkoutSessionDelegate {
    func workoutSession(_ workoutSession: HKWorkoutSession, didChangeTo toState: HKWorkoutSessionState,
                        from fromState: HKWorkoutSessionState, date: Date) {
        DispatchQueue.main.async { self.running = toState == .running }
    }

    func workoutSession(_ workoutSession: HKWorkoutSession, didFailWithError error: Error) {
        DispatchQueue.main.async {
            self.error = error.localizedDescription
            self.running = false
        }
    }
}

extension WatchHeart: HKLiveWorkoutBuilderDelegate {
    func workoutBuilderDidCollectEvent(_ workoutBuilder: HKLiveWorkoutBuilder) {}

    func workoutBuilder(_ workoutBuilder: HKLiveWorkoutBuilder, didCollectDataOf collectedTypes: Set<HKSampleType>) {
        guard collectedTypes.contains(heartType),
              let q = workoutBuilder.statistics(for: heartType)?.mostRecentQuantity() else { return }
        let v = Int(q.doubleValue(for: HKUnit.count().unitDivided(by: .minute())).rounded())
        send(v)
        DispatchQueue.main.async { self.bpm = v }
    }
}

extension WatchHeart: WCSessionDelegate {
    func session(_ session: WCSession, activationDidCompleteWith state: WCSessionActivationState, error: Error?) {
        DispatchQueue.main.async { self.phoneReachable = session.isReachable }
    }

    func sessionReachabilityDidChange(_ session: WCSession) {
        DispatchQueue.main.async { self.phoneReachable = session.isReachable }
    }
}
