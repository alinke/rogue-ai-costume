// LAB-7 scanner for iPhone: the researcher's side of the ROGUE.AI costume.
// Same game as the ESP32 scanner (../../scanner): track the AI's beacon, fire
// CONTAIN (it always escapes), run the humanity test on guests. For guests
// who join the party's event: the evidence wall, the agent dossier with its
// kill-switch code, the leaderboard and the midnight alert.
import SwiftUI

@main
struct LabScannerApp: App {
    @StateObject private var scanner = Scanner()

    init() {
        let bar = UITabBarAppearance()
        bar.configureWithOpaqueBackground()
        bar.backgroundColor = .black
        bar.shadowColor = UIColor(red: 0, green: 0.51, blue: 0.23, alpha: 1)
        UITabBar.appearance().standardAppearance = bar
        UITabBar.appearance().scrollEdgeAppearance = bar
        UITabBar.appearance().unselectedItemTintColor = UIColor(red: 0, green: 0.51, blue: 0.23, alpha: 1)
    }

    var body: some Scene {
        WindowGroup {
            RootView().environmentObject(scanner)
        }
    }
}

struct RootView: View {
    @EnvironmentObject private var scanner: Scanner
    @ObservedObject private var settings = Settings.shared
    @ObservedObject private var game = Game.shared
    @State private var tab = 0
    @State private var invite: Settings.Invite?
    @State private var manualJoin = false   // DEBUG screenshots: the join screen with its scan button

    /// Off the scanner tab and the AI comes close: say so (tap to go hunt it).
    @ViewBuilder private var nearBanner: some View {
        if settings.infected {
            Button { tab = 0 } label: {
                Text("☣ SCANNER INFECTED: GET CURED AT THE CHECKPOINT")
                    .font(mono(12)).foregroundColor(.black)
                    .frame(maxWidth: .infinity).padding(.vertical, 8).background(Color.labRed)
            }
        }
        if scanner.zone >= .near {
            Button { tab = 0 } label: {
                Text(scanner.zone == .close ? "⚠ THE ENTITY IS RIGHT HERE" : "⚠ THE ENTITY IS NEAR")
                    .font(mono(15)).foregroundColor(.black)
                    .frame(maxWidth: .infinity).padding(.vertical, 10)
                    .background(scanner.zone == .close ? Color.labRed : Color.labAmber)
            }
            .transition(.move(edge: .top))
        }
    }

    var body: some View {
        TabView(selection: $tab) {
            ContentView()
                .tabItem { Label("SCANNER", systemImage: "dot.radiowaves.left.and.right") }.tag(0)
            WallView().safeAreaInset(edge: .top, spacing: 0) { nearBanner }
                .tabItem { Label("EVIDENCE", systemImage: "square.grid.3x3.fill") }.tag(1)
            AgentView().safeAreaInset(edge: .top, spacing: 0) { nearBanner }
                .tabItem { Label("AGENT", systemImage: "person.text.rectangle") }.tag(2)
        }
        .tint(.labGreen)
        .preferredColorScheme(.dark)
        .animation(.easeOut(duration: 0.25), value: scanner.zone)
        .onAppear {
            UIApplication.shared.isIdleTimerDisabled = true  // the beacon needs the app in front
#if DEBUG
            // Simulator checks: -tab N, -invite <link> (the join sheet), -join <link> (joined at once), -leave.
            let a = ProcessInfo.processInfo.arguments
            func arg(_ k: String) -> String? { a.firstIndex(of: k).flatMap { $0 + 1 < a.count ? a[$0 + 1] : nil } }
            if a.contains("-leave") { settings.leaveEvent() }
            // Screenshots: a pretend event (no server), an infected scanner, a game in progress.
            if a.contains("-fakeEvent") {
                settings.serviceURL = "https://demo.invalid"; settings.uploadKey = "demo"
                settings.eventName = "HALLOWEEN 2026"
                settings.midnight = Date().addingTimeInterval(3 * 3600 + 17 * 60 + 42).timeIntervalSince1970
            }
            if a.contains("-demoInfected") { settings.infected = true; settings.infectedSince = Date().timeIntervalSince1970 }
            if let p = arg("-demoGame") { game.demo(phase: p) }
            if a.contains("-joinSheet") { manualJoin = true }
            if let t = arg("-tab").flatMap(Int.init) { tab = t }
            if let u = arg("-invite").flatMap(URL.init(string:)) { invite = Settings.invite(from: u) }
            if let u = arg("-join").flatMap(URL.init(string:)), let inv = Settings.invite(from: u) { settings.join(inv) }
#endif
        }
        .onOpenURL { url in
            guard let inv = Settings.invite(from: url) else { return }
            if inv.isOwnerSetup {
                settings.join(inv)   // your own scanner phones: lab7://configure
            } else {
                invite = inv         // a guest: show what joining means first
            }
        }
        .task { game.start() }
        .fullScreenCover(isPresented: $game.showOrders) { SealedOrdersView() }
        .sheet(isPresented: $manualJoin) { JoinView() }
        .sheet(item: $invite) { inv in
            JoinView(invite: inv) { tab = 1 }
        }
    }
}
