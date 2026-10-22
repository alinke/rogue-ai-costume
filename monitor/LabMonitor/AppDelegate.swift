// LAB-7 Monitor: a stationary iPad at the party -- the lab's containment monitor
// for one room. Watches for the ROGUE.AI costume's Bluetooth beacon, shows the
// evidence feed and scoreboard, and runs a biometric checkpoint for guests.
// iOS 12+ (an original iPad Air works), UIKit only. See ../README.md.
import UIKit

@UIApplicationMain
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?

    func application(_ app: UIApplication, didFinishLaunchingWithOptions opts: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        UIApplication.shared.isIdleTimerDisabled = true  // a wall display: never sleep
        let w = UIWindow(frame: UIScreen.main.bounds)
        w.rootViewController = MonitorViewController()
        w.makeKeyAndVisible()
        window = w
        return true
    }
}
