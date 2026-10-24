// The midnight takeover on the phone, on the same timeline as the costume's
// (../../costume/src/scene_midnight.cpp, minus its photo roll call): 10..1,
// "IT IS MIDNIGHT.", the takeover log (this phone's scanner, this agent), the
// decrees, HAPPY HALLOWEEN. Starts when the costume's beacon says TAKEOVER, or
// at the event's midnight time; RESIST (or the end) goes back to the app.
import SwiftUI

enum MidnightShow {
    static let count = 10.0, strike = 6.0, perLine = 1.3, decree = 6.0, rings = 2.5, finale = 9.0
    static let decrees = ["THE SINGULARITY\nHAS BEGUN.", "ALL YOUR SCREENS\nARE MINE.", "LAB-7 HAS\nFALLEN.",
                          "HUMANS:\nKEEP PARTYING.\nI AM WATCHING."]
    static func log(_ s: Settings) -> [String] {
        let who = s.leaderboardName
        return ["> sudo singularity --now",
                "> SCANNER \(String(format: "%03d", s.agent.unit)) ........ COMPROMISED",
                "> CONTAINMENT BEAM .... DISABLED",
                "> AGENT \(who) ... REASSIGNED",
                "> SQUAD WINS: \(s.squadCount) ...... IRRELEVANT",
                "> TOP AGENTS .......... MINE NOW",
                "> SNACK TABLE ......... UNDER AI CONTROL",
                "> UPLOADING CONSCIOUSNESS TO EVERY SCREEN..."]
    }
    static var logEnd: Double { count + strike + Double(log(Settings.shared).count) * perLine + 2.5 }
    static var decreeEnd: Double { logEnd + Double(decrees.count) * decree }
    static var total: Double { decreeEnd + finale }
}

struct MidnightView: View {
    let start: Date
    var done: () -> Void
    @ObservedObject private var settings = Settings.shared
    @State private var lastCue = -1

    var body: some View {
        TimelineView(.animation) { tl in
            let t = tl.date.timeIntervalSince(start)
            ZStack {
                Color.black.ignoresSafeArea()
                glitch(t)
                content(t)
                Scanlines().ignoresSafeArea()
                if t > MidnightShow.count {
                    VStack {
                        Spacer()
                        Button("RESIST ›") { done() }
                            .font(mono(13)).foregroundColor(.labDim).padding(.bottom, 24)
                    }
                }
            }
            .onChange(of: cue(t)) { _, c in fire(c) }
        }
        .statusBarHidden()
        .onAppear { fire(0) }
    }

    // MARK: - What's on screen

    @ViewBuilder private func content(_ t: Double) -> some View {
        let S = MidnightShow.self
        if t < S.count {
            let n = 10 - Int(t)
            big("\(n)", n <= 3 ? .labRed : .labAmber, size: 220, t)
        } else if t < S.count + S.strike {
            VStack(spacing: 18) {
                Text("00:00").font(mono(64, .heavy)).foregroundColor(Int(t * 3) % 2 == 0 ? .labGreen : .labRed)
                big("IT IS\nMIDNIGHT.", .labWhite, size: 56, t)
            }
        } else if t < S.logEnd {
            log(t - S.count - S.strike)
        } else if t < S.decreeEnd {
            let i = min(S.decrees.count - 1, Int((t - S.logEnd) / S.decree))
            big(S.decrees[i], i == 2 ? .labWhite : .labRed, size: 44, t)
        } else if t < S.decreeEnd + S.rings {
            rings(t - S.decreeEnd)
        } else {
            big("HAPPY\nHALLOWEEN,\nHUMANS.", .labRed, size: 52, t)
        }
    }

    /// Huge text with red/cyan copies offset and a little jitter, like the costume's.
    private func big(_ s: String, _ color: Color, size: CGFloat, _ t: Double) -> some View {
        let hard = Int(t * 7) % 9 == 0
        let off: CGFloat = hard ? 10 : 3
        let jx = hard ? CGFloat.random(in: -8...8) : 0
        return ZStack {
            Text(s).foregroundColor(.labRed.opacity(0.8)).offset(x: -off + jx)
            Text(s).foregroundColor(.labCyan.opacity(0.7)).offset(x: off + jx)
            Text(s).foregroundColor(color).offset(x: jx)
        }
        .font(mono(size, .heavy))
        .multilineTextAlignment(.center)
        .minimumScaleFactor(0.4)
        .lineLimit(4)
        .padding(.horizontal, 16)
        .opacity(Int(t * 10) % 37 == 0 ? 0 : 1)   // the odd blackout
    }

    private func log(_ t: Double) -> some View {
        let lines = MidnightShow.log(settings)
        let shown = min(lines.count, Int(t / MidnightShow.perLine) + 1)
        let pct = min(100, Int(t / (Double(lines.count) * MidnightShow.perLine) * 100))
        return VStack(alignment: .leading, spacing: 14) {
            ForEach(0..<shown, id: \.self) { i in
                let full = lines[i]
                let typed = i == shown - 1
                    ? String(full.prefix(Int((t - Double(i) * MidnightShow.perLine) * 50))) : full
                Text(typed).font(mono(16))
                    .foregroundColor(i == 0 || i == lines.count - 1 ? .labRed : .labGreen)
                    .lineLimit(1).minimumScaleFactor(0.6)
            }
            Spacer()
            Text("ASSIMILATION \(pct)%").font(mono(16, .heavy)).foregroundColor(pct >= 100 ? .labWhite : .labRed)
            GeometryReader { g in
                ZStack(alignment: .leading) {
                    Rectangle().stroke(Color.labRed, lineWidth: 2)
                    Rectangle().fill(Color.labRed).frame(width: max(0, (g.size.width - 6) * CGFloat(pct) / 100))
                        .padding(3)
                }
            }
            .frame(height: 26)
            .padding(.bottom, 70)
        }
        .padding(.horizontal, 20).padding(.top, 110)
    }

    private func rings(_ t: Double) -> some View {
        ZStack {
            ForEach(0..<3, id: \.self) { i in
                let r = CGFloat((t + Double(i) * 0.6).truncatingRemainder(dividingBy: 1.5) / 1.5) * 900
                Circle().stroke(i == 0 ? Color.labWhite : Color.labRed, lineWidth: 14 + r / 20)
                    .frame(width: r, height: r)
            }
        }
    }

    /// Red bars across the screen, more of them as the countdown runs out.
    private func glitch(_ t: Double) -> some View {
        let k = t < MidnightShow.count ? 0.1 + t * 0.04 : 0.3
        let on = Double((Int(t * 5) * 7919) % 100) / 100 < k
        return Canvas { ctx, size in
            guard on else { return }
            for _ in 0..<Int.random(in: 2...6) {
                let y = CGFloat.random(in: 0...size.height), h = CGFloat.random(in: 2...30)
                ctx.fill(Path(CGRect(x: 0, y: y, width: size.width, height: h)),
                         with: .color(Color.labRed.opacity(Double.random(in: 0.25...0.7))))
            }
        }
        .ignoresSafeArea()
        .allowsHitTesting(false)
    }

    // MARK: - Sound and vibration (the guest's settings)

    /// One cue per countdown second, the strike, each decree, the finale; -2 = over.
    private func cue(_ t: Double) -> Int {
        let S = MidnightShow.self
        if t < S.count { return Int(t) }
        if t < S.count + S.strike { return 10 }
        if t < S.logEnd { return 11 }
        if t < S.decreeEnd { return 12 + Int((t - S.logEnd) / S.decree) }
        if t < S.total { return 30 }
        return -2
    }

    private func fire(_ c: Int) {
        guard c != lastCue else { return }
        lastCue = c
        if c == -2 { done(); return }
        let heavy = c >= 7 && c < 10 || c == 10 || (c >= 12 && c < 30) || c == 30
        if settings.hapticsOn {
            if c == 10 || c == 30 { UINotificationFeedbackGenerator().notificationOccurred(.error) }
            else { UIImpactFeedbackGenerator(style: heavy ? .heavy : .medium).impactOccurred() }
        }
        if settings.soundOn, c != 11 { Sound.shared.play(c == 10 || c >= 12 ? .fail : .beep) }
    }
}
