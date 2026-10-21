// The scanner screen, laid out like the Tab5: header, signal meter, status +
// intel from the costume's beacon, CONTAIN, HUMANITY TEST.
import SwiftUI

extension Color {
    init(hex: UInt32) {
        self.init(red: Double((hex >> 16) & 255) / 255, green: Double((hex >> 8) & 255) / 255,
                  blue: Double(hex & 255) / 255)
    }
    static let labGreen = Color(hex: 0x28FF5A), labDim = Color(hex: 0x00823A), labRed = Color(hex: 0xFF1E28)
    static let labAmber = Color(hex: 0xFFAA00), labCyan = Color(hex: 0x00DCFF), labWhite = Color(hex: 0xEBFFF0)
}

func mono(_ size: CGFloat, _ weight: Font.Weight = .bold) -> Font {
    .system(size: size, weight: weight, design: .monospaced)
}

private let THREAT = ["LOW (FOR NOW)", "GUARDED", "ELEVATED", "HIGH", "SEVERE", "EXISTENTIAL"]
private let ACTIVITY = ["IDLE", "SCANNING A HUMAN", "PLAYING A GAME", "MIDNIGHT TAKEOVER", "BEING CONTAINED", "ESCAPED"]

struct ContentView: View {
    @StateObject private var scanner = Scanner()
    @ObservedObject private var settings = Settings.shared
    @ObservedObject private var uploads = Uploader.shared
    @State private var showCamera = false
    @State private var showSettings = false

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            Scanlines().ignoresSafeArea()
            VStack(alignment: .leading, spacing: 0) {
                header
                Spacer(minLength: 12)
                meter
                    .contentShape(Rectangle())
                    .onTapGesture {  // tap the meter: sound on / quiet (CONTAIN still sounds)
                        settings.soundOn.toggle()
                        if settings.soundOn { Sound.shared.play(.beep) }
                    }
                Spacer(minLength: 16)
                status
                Spacer(minLength: 16)
                containButton
                    .padding(.bottom, 22)
                humanityButton
            }
            .padding(.horizontal, 18)
            .padding(.vertical, 8)
            if scanner.contain == .fired && scanner.now.timeIntervalSince(scanner.containAt) < 0.25 {
                Color.labCyan.opacity(0.85).ignoresSafeArea().allowsHitTesting(false)  // the beam flash
            }
        }
        .preferredColorScheme(.dark)
        .statusBarHidden()
        .onAppear { UIApplication.shared.isIdleTimerDisabled = true }  // the beacon needs the app in front
        .fullScreenCover(isPresented: $showCamera) { HumanityTestView() }
#if DEBUG
        // Simulator check of the ID card (no camera there): launch with -demoCard [human|compromised].
        .overlay {
            if let kind = demoCard {
                Image(uiImage: IDCard.render(photo: Self.testPhoto(), subject: 7, human: kind == "human",
                                             exposure: kind == "human" ? 12 : 88,
                                             findings: Array((kind == "human" ? IDCard.humanFindings : IDCard.aiFindings).prefix(2)),
                                             researcher: settings.researcherName, site: settings.siteText))
                    .resizable().scaledToFit().background(Color.black).ignoresSafeArea()
            }
        }
#endif
        .sheet(isPresented: $showSettings) { SettingsView() }
    }

#if DEBUG
    private var demoCard: String? {
        let a = ProcessInfo.processInfo.arguments
        guard let i = a.firstIndex(of: "-demoCard") else { return nil }
        return i + 1 < a.count ? a[i + 1] : "human"
    }
    static func testPhoto() -> UIImage {
        UIGraphicsImageRenderer(size: CGSize(width: 900, height: 1200)).image { r in
            UIColor.darkGray.setFill(); r.fill(CGRect(x: 0, y: 0, width: 900, height: 1200))
            UIColor.lightGray.setFill(); r.cgContext.fillEllipse(in: CGRect(x: 250, y: 220, width: 400, height: 500))
            UIColor.gray.setFill(); r.cgContext.fillEllipse(in: CGRect(x: 120, y: 760, width: 660, height: 600))
        }
    }
#endif

    private var header: some View {
        HStack(alignment: .top) {
            VStack(alignment: .leading, spacing: 4) {
                Text("LAB-7 SCANNER // ROGUE.AI").font(mono(14)).foregroundColor(.labDim)
                Text("CONTAINMENT UNIT // TARGET v6.6.6").font(mono(11, .regular)).foregroundColor(.labDim)
            }
            Spacer()
            VStack(alignment: .trailing, spacing: 4) {
                Button { showSettings = true } label: {
                    Image(systemName: "gearshape").font(.system(size: 18)).foregroundColor(.labDim)
                }
                Text(settings.soundOn ? "SOUND: ON" : "SOUND: QUIET").font(mono(11))
                    .foregroundColor(settings.soundOn ? .labGreen : .labAmber)
                if uploads.pending > 0 {
                    Text("\(uploads.pending) TO UPLOAD").font(mono(10, .regular)).foregroundColor(.labDim)
                }
            }
        }
    }

    private var meter: some View {
        let a = scanner.ai
        let lit = a.seen ? max(1, min(8, (a.rssi + 96) * 8 / 44)) : 0
        return VStack(spacing: 12) {
            GeometryReader { g in
                let bw = g.size.width * 14 / 150, gap = (g.size.width - 8 * bw) / 7
                ZStack(alignment: .bottomLeading) {
                    ForEach(0..<8, id: \.self) { i in
                        let h = g.size.height / 6 + CGFloat(i) * (g.size.height * 5 / 6) / 7
                        let c: Color = i < 4 ? .labGreen : i < 6 ? .labAmber : .labRed
                        Group {
                            if i < lit { Rectangle().fill(c) } else { Rectangle().stroke(Color.labDim, lineWidth: 2) }
                        }
                        .frame(width: bw, height: h)
                        .offset(x: CGFloat(i) * (bw + gap))
                    }
                    if !a.seen {  // searching: a sweep across the meter
                        let x = CGFloat(scanner.now.timeIntervalSince1970.truncatingRemainder(dividingBy: 2) / 2)
                            * g.size.width
                        Rectangle().fill(Color.labDim).frame(width: 2).offset(x: x)
                    }
                }
            }
            .frame(height: 230)
            Text(a.seen ? "\(a.rssi) dBm" : "-- dBm").font(mono(22)).foregroundColor(a.seen ? .labGreen : .labDim)
                .frame(maxWidth: .infinity)
        }
    }

    private var status: some View {
        let a = scanner.ai
        var line = "NO SIGNAL", color = Color.labDim, blink = false
        switch scanner.contain {
        case .fired: line = "BEAM FIRED"; color = .labCyan
        case .containing: line = "CONTAINING \(scanner.containingPercent)%"; color = .labCyan
        case .failed: line = "CONTAINMENT FAILED"; color = .labRed; blink = true
        case .ready:
            switch scanner.zone {
            case .lost: line = "NO SIGNAL"; color = .labDim
            case .detected: line = "ROGUE SIGNAL"; color = .labAmber
            case .near: line = "SIGNAL STRONG"; color = .labAmber
            case .close: line = "TARGET LOCKED"; color = .labRed; blink = true
            }
        }
        let on = !blink || Int(scanner.now.timeIntervalSince1970 * 1000 / 300) % 2 == 0
        return VStack(alignment: .leading, spacing: 8) {
            Text(line).font(mono(30)).foregroundColor(color).opacity(on ? 1 : 0)
                .lineLimit(1).minimumScaleFactor(0.6)
            Group {
                if scanner.contain == .failed {
                    Text("THE AI ESCAPED.").foregroundColor(.labWhite)
                    Text("IT ALWAYS DOES.").foregroundColor(.labWhite)
                    Text(" ")
                } else if !a.seen {
                    Text(scanner.contain == .ready ? "SEARCHING..." : " ").foregroundColor(.labDim)
                    Text("SWEEP THE AREA").foregroundColor(.labDim)
                    Text(" ")
                } else {
                    Text("THREAT: \(THREAT[min(a.threat, 5)])").foregroundColor(.labWhite)
                    Text("AI: \(ACTIVITY[min(a.activity, 5)])").foregroundColor(.labWhite)
                    Text("SCANNED \(a.scanned)  CONTAINED \(a.contained)").foregroundColor(.labGreen)
                }
            }
            .font(mono(17))
            .lineLimit(1).minimumScaleFactor(0.7)
            if !scanner.radio.bluetoothOn {
                Text("BLUETOOTH IS OFF").font(mono(14)).foregroundColor(.labRed)
            }
        }
    }

    private var containButton: some View {
        Button { scanner.fire() } label: {
            ZStack {
                RoundedRectangle(cornerRadius: 22).fill(scanner.cooling ? Color(hex: 0x300004) : .labRed)
                if scanner.cooling {
                    VStack(spacing: 6) {
                        Text("RECHARGING").font(mono(38)).foregroundColor(.labRed)
                        Text("BEAM READY IN \(scanner.cooldownLeft)s").font(mono(16)).foregroundColor(.labWhite)
                        GeometryReader { g in
                            Rectangle().fill(Color.labRed).frame(width: g.size.width * scanner.cooldownProgress, height: 10)
                        }
                        .frame(height: 10).padding(.horizontal, 22)
                    }
                } else {
                    Text("CONTAIN").font(mono(52)).foregroundColor(.labWhite)
                }
            }
            .frame(height: 170)
        }
        .buttonStyle(PressStyle())
    }

    private var humanityButton: some View {
        Button { showCamera = true } label: {
            HStack(spacing: 18) {
                Image(systemName: "camera.fill").font(.system(size: 34))
                Text("HUMANITY TEST").font(mono(26))
            }
            .foregroundColor(.labCyan)
            .frame(maxWidth: .infinity).frame(height: 96)
            .overlay(RoundedRectangle(cornerRadius: 22).stroke(Color.labCyan, lineWidth: 3))
        }
        .buttonStyle(PressStyle())
    }
}

/// Fires on touch-down feel: a quick dim while pressed.
struct PressStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label.opacity(configuration.isPressed ? 0.7 : 1)
    }
}

/// Faint scan lines, like the costume.
struct Scanlines: View {
    var body: some View {
        Canvas { ctx, size in
            var y: CGFloat = 0
            while y < size.height {
                ctx.fill(Path(CGRect(x: 0, y: y, width: size.width, height: 1)), with: .color(Color(hex: 0x001204)))
                y += 4
            }
        }
        .allowsHitTesting(false)
    }
}
