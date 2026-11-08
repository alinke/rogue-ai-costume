// The one screen: the jacket's link and power along the top, the five
// categories (Themes, Holidays, Bike, Live, Arcade), and brightness / speed along the
// bottom. What's shown follows the jacket's own status, so it matches the
// armband too.
import SwiftUI

private let neon = Color(rgb: 0x00DCFF)
private let panel = Color.white.opacity(0.06)

struct ContentView: View {
    @EnvironmentObject var link: JacketLink
    @EnvironmentObject var music: MusicListener
    @State private var category = LookCategory.themes
    @State private var pickedCategory = false

    var body: some View {
        VStack(spacing: 0) {
            Header()
            if link.demo { DemoBanner() }
            CategoryBar(category: $category, current: Look.of(link.status.look).category) { pickedCategory = true }
            ScrollViewReader { scroll in
                ScrollView {
                    Group {
                        switch category {
                        case .themes, .holidays: LookGrid(looks: Look.in(category))
                        case .bike: BikePanel()
                        case .live: LivePanel()
                        case .arcade: ArcadePanel(scroll: scroll)
                        }
                    }
                    .padding(16)
                }
            }
            Controls()
        }
        .background(Color.black.ignoresSafeArea())
        .onChange(of: link.status.look) { _, look in
            // The music modes listen only while they're on.
            if look == Look.music || look == Look.musicBeat { music.start() } else { music.stop() }
            if !pickedCategory { category = Look.of(look).category }
        }
        .onAppear {
            category = Look.of(link.status.look).category
            // Screenshots: launch with -tab themes|holidays|bike|live|arcade.
            if let t = UserDefaults.standard.string(forKey: "tab"),
               let c = LookCategory.allCases.first(where: { $0.title.lowercased() == t }) {
                category = c
                pickedCategory = true
            }
        }
    }
}

// MARK: - Header

private struct Header: View {
    @EnvironmentObject var link: JacketLink
    @State private var settings = false

    var body: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                Text("ARCADE JACKET").font(.system(size: 24, weight: .black, design: .monospaced)).foregroundStyle(neon)
                    .lineLimit(1).minimumScaleFactor(0.7)
                HStack(spacing: 6) {
                    Circle().fill(dot).frame(width: 8, height: 8)
                    Text(label).font(.caption.monospaced()).foregroundStyle(.secondary)
                }
            }
            Spacer()
            Button {
                settings = true
            } label: {
                Image(systemName: "gearshape.fill")
                    .font(.title2)
                    .frame(width: 44, height: 52)
                    .foregroundStyle(.secondary)
            }
            .accessibilityLabel("Settings")
            .sheet(isPresented: $settings) { SettingsView() }
            .onAppear { settings = UserDefaults.standard.bool(forKey: "settings") }  // screenshots: -settings 1
            Button {
                link.send(.power, link.status.on ? 0 : 1)
            } label: {
                Image(systemName: "power")
                    .font(.title2.weight(.bold))
                    .frame(width: 52, height: 52)
                    .background(Circle().fill(link.status.on ? neon.opacity(0.25) : panel))
                    .overlay(Circle().stroke(link.status.on ? neon : .gray, lineWidth: 2))
                    .foregroundStyle(link.status.on ? neon : .gray)
            }
            .accessibilityLabel(link.status.on ? "Lights off" : "Lights on")
        }
        .padding(.horizontal, 16)
        .padding(.top, 8)
        .padding(.bottom, 10)
    }

    private var label: String {
        switch link.state {
        case .bluetoothOff: "Bluetooth is off"
        case .searching: "Looking for the jacket..."
        case .connecting: "Connecting..."
        case .connected: link.status.on ? "Connected · \(Look.of(link.status.look).name)" : "Connected · lights off"
        }
    }

    private var dot: Color {
        switch link.state {
        case .connected: .green
        case .connecting, .searching: .orange
        case .bluetoothOff: .red
        }
    }
}

// MARK: - Categories

private struct CategoryBar: View {
    @Binding var category: LookCategory
    let current: LookCategory
    let picked: () -> Void

    var body: some View {
        HStack(spacing: 6) {
            ForEach(LookCategory.allCases) { c in
                Button {
                    category = c
                    picked()
                } label: {
                    VStack(spacing: 4) {
                        Image(systemName: c.symbol).font(.title3)
                        Text(c.title).font(.caption.weight(.semibold))
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                    .background(RoundedRectangle(cornerRadius: 10).fill(category == c ? neon.opacity(0.2) : panel))
                    .overlay(alignment: .topTrailing) {
                        if c == current { Circle().fill(.green).frame(width: 7, height: 7).padding(6) }
                    }
                    .foregroundStyle(category == c ? neon : .secondary)
                }
            }
        }
        .padding(.horizontal, 16)
    }
}

// MARK: - Looks

private struct LookCard: View {
    @EnvironmentObject var link: JacketLink
    let look: Look

    var body: some View {
        let on = link.status.look == look.id && link.status.on
        Button {
            link.setLook(look.id)
        } label: {
            VStack(alignment: .leading, spacing: 8) {
                RoundedRectangle(cornerRadius: 8)
                    .fill(LinearGradient(colors: swatch, startPoint: .leading, endPoint: .trailing))
                    .frame(height: 34)
                    .shadow(color: (swatch.first ?? neon).opacity(on ? 0.8 : 0), radius: 8)
                Text(look.name).font(.headline).foregroundStyle(on ? .white : .secondary)
            }
            .padding(10)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 14).fill(panel))
            .overlay(RoundedRectangle(cornerRadius: 14).stroke(on ? neon : .clear, lineWidth: 2))
        }
    }

    private var swatch: [Color] {
        if look.usesColor {
            let c = Color(rgb: link.status.color)
            return look.id == Look.solid ? [c, c] : [c.opacity(0.25), c, c.opacity(0.25)]
        }
        return look.colors.map { Color(rgb: $0) }
    }
}

private struct LookGrid: View {
    @EnvironmentObject var link: JacketLink
    let looks: [Look]

    var body: some View {
        VStack(spacing: 16) {
            if Look.of(link.status.look).usesColor && looks.contains(where: { $0.id == link.status.look }) {
                ColorRow()
            }
            LazyVGrid(columns: [GridItem(.flexible(), spacing: 12), GridItem(.flexible(), spacing: 12)], spacing: 12) {
                ForEach(looks) { LookCard(look: $0) }
            }
        }
    }
}

/// ARCADE: Consoles or Games (each A-Z), a letter bar to jump down the list, and a search across both.
private struct ArcadePanel: View {
    @EnvironmentObject var link: JacketLink
    let scroll: ScrollViewProxy
    @State private var games = false
    @State private var search = ""

    var body: some View {
        let list = search.isEmpty ? Look.in(.arcade).filter { $0.isConsole != games }
                                  : Look.in(.arcade).filter { $0.name.localizedCaseInsensitiveContains(search) }
        let looks = list.sorted { $0.name.localizedStandardCompare($1.name) == .orderedAscending }
        VStack(alignment: .leading, spacing: 14) {
            Picker("", selection: $games) {
                Text("Consoles").tag(false)
                Text("Games").tag(true)
            }
            .pickerStyle(.segmented)
            .disabled(!search.isEmpty)
            HStack {
                Image(systemName: "magnifyingglass").foregroundStyle(.secondary)
                TextField("Find a console or game", text: $search)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()
                if !search.isEmpty {
                    Button { search = "" } label: { Image(systemName: "xmark.circle.fill").foregroundStyle(.secondary) }
                }
            }
            .padding(12)
            .background(RoundedRectangle(cornerRadius: 12).fill(panel))
            if search.isEmpty { letterBar(looks) }
            LazyVGrid(columns: [GridItem(.flexible(), spacing: 12), GridItem(.flexible(), spacing: 12)], spacing: 12) {
                ForEach(looks) { LookCard(look: $0).id($0.id) }
            }
            if looks.isEmpty {
                Text("Nothing called \"\(search)\"").foregroundStyle(.secondary).frame(maxWidth: .infinity)
            }
            Text("The LED strip effects Pixelcade plays on arcade cabinets, one for each console and game.")
                .font(.footnote).foregroundStyle(.secondary)
        }
        .onAppear { games = !Look.of(link.status.look).isConsole && Look.of(link.status.look).category == .arcade }
    }

    /// Tap a letter: the list scrolls to the first name starting with it (# for numbers).
    private func letterBar(_ looks: [Look]) -> some View {
        let first = Dictionary(looks.map { (letter($0.name), $0.id) }, uniquingKeysWith: { a, _ in a })
        let letters = first.keys.sorted { $0 == "#" ? true : $1 == "#" ? false : $0 < $1 }
        return ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 6) {
                ForEach(letters, id: \.self) { l in
                    Button {
                        withAnimation { scroll.scrollTo(first[l], anchor: .top) }
                    } label: {
                        Text(l).font(.subheadline.weight(.bold).monospaced())
                            .frame(width: 30, height: 30)
                            .background(RoundedRectangle(cornerRadius: 8).fill(panel))
                            .foregroundStyle(neon)
                    }
                }
            }
        }
    }

    private func letter(_ name: String) -> String {
        guard let c = name.uppercased().first else { return "#" }
        return c.isLetter ? String(c) : "#"
    }
}

/// Solid / Breathe / Sparkle / Comet: the colour, from presets or the picker.
private struct ColorRow: View {
    @EnvironmentObject var link: JacketLink
    private let presets: [UInt32] = [0x00DCFF, 0xFF0000, 0x00FF40, 0x0040FF, 0x9000FF, 0xFFA000, 0xFFFFFF, 0xFF40A0]

    var body: some View {
        HStack(spacing: 10) {
            ForEach(presets, id: \.self) { c in
                Circle().fill(Color(rgb: c)).frame(width: 30, height: 30)
                    .overlay(Circle().stroke(.white, lineWidth: link.status.color == c ? 3 : 0))
                    .onTapGesture { link.setColor(c) }
            }
            ColorPicker("", selection: Binding(get: { Color(rgb: link.status.color) }, set: { link.setColor($0.rgb) }),
                        supportsOpacity: false)
                .labelsHidden()
        }
        .padding(12)
        .frame(maxWidth: .infinity)
        .background(RoundedRectangle(cornerRadius: 14).fill(panel))
    }
}

// MARK: - Bike

private struct BikePanel: View {
    @EnvironmentObject var link: JacketLink
    @EnvironmentObject var brake: BikeBrake
    @State private var holding = false

    var body: some View {
        VStack(spacing: 14) {
            HStack(spacing: 10) {
                turnButton(.left, "arrow.left", "Left")
                turnButton(.hazard, "exclamationmark.triangle", "Hazard")
                turnButton(.right, "arrow.right", "Right")
            }
            // Press and hold: the brake light for as long as you hold it.
            Text(link.status.braking || holding ? "BRAKING" : "HOLD TO BRAKE")
                .font(.title3.weight(.black).monospaced())
                .frame(maxWidth: .infinity, minHeight: 64)
                .background(RoundedRectangle(cornerRadius: 14).fill(link.status.braking || holding ? .red : .red.opacity(0.18)))
                .foregroundStyle(link.status.braking || holding ? .white : .red)
                .gesture(DragGesture(minimumDistance: 0)
                    .onChanged { _ in if !holding { holding = true; link.send(.brake, 1) } }
                    .onEnded { _ in holding = false; link.send(.brake, 0) })
                .onReceive(Timer.publish(every: 0.8, on: .main, in: .common).autoconnect()) { _ in
                    if holding { link.send(.brake, 1) }  // the jacket lets go 1.5 s after the last
                }
            VStack(alignment: .leading, spacing: 8) {
                Toggle(isOn: Binding(get: { brake.enabled }, set: { brake.setEnabled($0) })) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Automatic brake light").font(.headline)
                        Text("Lights red when GPS says you're slowing down (about a second behind). Works in a pocket and with the screen locked.")
                            .font(.caption).foregroundStyle(.secondary)
                    }
                }
                .tint(.red)
                if brake.enabled {
                    HStack {
                        Text(brake.speed.map { String(format: "%.0f km/h · %.0f mph", $0 * 3.6, $0 * 2.237) } ?? "Waiting for GPS...")
                            .font(.caption.monospaced())
                        Spacer()
                        if brake.braking { Text("BRAKE").font(.caption.bold()).foregroundStyle(.red) }
                    }
                }
                if brake.denied {
                    Text("Location is off for Arcade Jacket: Settings › Arcade Jacket › Location › While Using the App.").font(.caption).foregroundStyle(.orange)
                }
            }
            .padding(14)
            .background(RoundedRectangle(cornerRadius: 14).fill(panel))
            LazyVGrid(columns: [GridItem(.flexible(), spacing: 12), GridItem(.flexible(), spacing: 12)], spacing: 12) {
                ForEach(Look.in(.bike)) { LookCard(look: $0) }
            }
            Text("Turn signals sweep amber down that side's sleeve and front, and cancel themselves after 30 s. The armband has the same buttons, easier to reach while riding.")
                .font(.caption).foregroundStyle(.secondary)
            Label("Wear the jacket as well as proper bike lights, never instead of them. The automatic brake light is about a second behind your braking, and check your local rules on flashing lights.",
                  systemImage: "exclamationmark.triangle")
                .font(.caption).foregroundStyle(.orange)
        }
    }

    private func turnButton(_ t: Turn, _ symbol: String, _ label: String) -> some View {
        let on = link.status.turn == t
        return Button {
            link.send(.turn, on ? Turn.off.rawValue : t.rawValue)
        } label: {
            VStack(spacing: 6) {
                Image(systemName: symbol).font(.system(size: 34, weight: .bold))
                Text(on ? "Tap: off" : label).font(.caption.weight(.semibold))
            }
            .frame(maxWidth: .infinity, minHeight: 96)
            .background(RoundedRectangle(cornerRadius: 14).fill(on ? Color.orange : Color.orange.opacity(0.15)))
            .foregroundStyle(on ? .black : .orange)
        }
    }
}

// MARK: - Live

private struct LivePanel: View {
    @EnvironmentObject var link: JacketLink
    @EnvironmentObject var music: MusicListener
    @EnvironmentObject var heart: HeartRate

    var body: some View {
        VStack(spacing: 14) {
            LazyVGrid(columns: [GridItem(.flexible(), spacing: 12), GridItem(.flexible(), spacing: 12)], spacing: 12) {
                ForEach(Look.in(.live)) { LookCard(look: $0) }
            }
            // Music
            VStack(alignment: .leading, spacing: 10) {
                Label("Music", systemImage: "music.note").font(.headline)
                if music.denied {
                    Text("The microphone is off for Arcade Jacket: Settings › Arcade Jacket › Microphone.").font(.caption).foregroundStyle(.orange)
                } else if music.running {
                    HStack(alignment: .bottom, spacing: 8) {
                        meter("Bass", music.bass, .green)
                        meter("Mids", music.mid, .yellow)
                        meter("Treble", music.treble, .pink)
                        Spacer()
                        BeatDot(beats: music.beats)
                    }
                    .frame(height: 70)
                    Text("Listening -- nothing is recorded. Your music can play from this phone too.")
                        .font(.caption).foregroundStyle(.secondary)
                } else {
                    Text("Pick Music or Music Beat: the phone listens to the music around you (a concert, a party, a speaker) and the jacket moves with it.")
                        .font(.caption).foregroundStyle(.secondary)
                }
            }
            .padding(14)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 14).fill(panel))
            HeartPanel()
        }
    }

    private func meter(_ name: String, _ v: Float, _ c: Color) -> some View {
        VStack(spacing: 4) {
            GeometryReader { g in
                VStack { Spacer(); RoundedRectangle(cornerRadius: 4).fill(c).frame(height: g.size.height * CGFloat(v)) }
            }
            .frame(width: 28)
            .animation(.linear(duration: 0.08), value: v)
            Text(name).font(.caption2).foregroundStyle(.secondary)
        }
    }
}

/// Flashes on each beat the listener hears.
private struct BeatDot: View {
    let beats: Int
    @State private var flash = false

    var body: some View {
        Circle().fill(Color.purple)
            .frame(width: 44, height: 44)
            .opacity(flash ? 1 : 0.25)
            .scaleEffect(flash ? 1.15 : 0.9)
            .onChange(of: beats) { _, _ in
                flash = true
                withAnimation(.easeOut(duration: 0.25)) { flash = false }
            }
    }
}

private struct HeartPanel: View {
    @EnvironmentObject var heart: HeartRate

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Label("Heart rate", systemImage: "heart.fill").font(.headline).foregroundStyle(.red)
                Spacer()
                if let bpm = heart.bpm {
                    Text("\(bpm) BPM").font(.title3.monospacedDigit().bold())
                }
            }
            if heart.bpm != nil {
                Text("From \(heart.source)").font(.caption).foregroundStyle(.secondary)
            }
            if let name = heart.connectedName {
                HStack {
                    Text("Sensor: \(name)").font(.subheadline)
                    Spacer()
                    Button("Forget") { heart.forget() }.font(.caption)
                }
            } else {
                Button(heart.scanning ? "Searching for sensors..." : "Find a heart-rate sensor") {
                    heart.scanning ? heart.stopScan() : heart.startScan()
                }
                .buttonStyle(.borderedProminent).tint(.red)
                ForEach(heart.sensors) { s in
                    Button { heart.connect(s) } label: {
                        HStack { Image(systemName: "dot.radiowaves.left.and.right"); Text(s.name); Spacer(); Text("Connect") }
                    }
                    .padding(.vertical, 4)
                }
            }
            Text("""
            Works with anything that shares heart rate over Bluetooth: a Fitbit Charge 6 or Pixel Watch 2-4 \
            (turn on heart-rate sharing on the watch), Polar, Wahoo and Garmin chest straps and arm bands. \
            Older Fitbits (Versa, Sense, Inspire) can't. Apple Watch: open Arcade Jacket on the watch and tap Start.
            """)
            .font(.caption).foregroundStyle(.secondary)
            if heart.watchPaired {
                Label("Arcade Jacket is on your Apple Watch", systemImage: "applewatch").font(.caption).foregroundStyle(.green)
            }
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 14).fill(panel))
    }
}

// MARK: - Brightness / speed

private struct Controls: View {
    @EnvironmentObject var link: JacketLink
    @State private var bright: Double = 60
    @State private var speed: Double = 100
    // While a slider is dragged (and a moment after), the jacket's status is
    // still catching up with the values just sent: showing those would pull the
    // slider back, and moving it would send them again.
    @State private var dragging = false
    @State private var releasedAt = Date.distantPast

    var body: some View {
        VStack(spacing: 6) {
            row("sun.max.fill", value: $bright, range: 10...100, text: "\(Int(bright))%") { link.send(.bright, UInt8($0)) }
            row("hare.fill", value: $speed, range: 20...240, text: String(format: "%.1fx", speed / 100)) { link.send(.speed, UInt8($0)) }
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 10)
        .background(Color.white.opacity(0.04))
        .onAppear { bright = Double(link.status.bright); speed = Double(link.status.speed) }
        .onChange(of: link.status.bright) { _, v in if following { bright = Double(v) } }
        .onChange(of: link.status.speed) { _, v in if following { speed = Double(v) } }
    }

    private var following: Bool { !dragging && Date().timeIntervalSince(releasedAt) > 1.5 }

    private func row(_ icon: String, value: Binding<Double>, range: ClosedRange<Double>, text: String,
                     send: @escaping (Double) -> Void) -> some View {
        HStack(spacing: 12) {
            Image(systemName: icon).frame(width: 24).foregroundStyle(neon)
            Slider(value: value, in: range, step: range.upperBound > 200 ? 10 : 5) { editing in
                dragging = editing
                if !editing {
                    releasedAt = Date()
                    send(value.wrappedValue)
                }
            }
            .tint(neon)
            .onChange(of: value.wrappedValue) { _, v in if dragging { send(v) } }
            Text(text).font(.caption.monospacedDigit()).frame(width: 44, alignment: .trailing)
        }
    }
}

private extension Color {
    var rgb: UInt32 {
        let c = UIColor(self)
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        c.getRed(&r, green: &g, blue: &b, alpha: &a)
        func ch(_ v: CGFloat) -> UInt32 { UInt32(max(0, min(1, v)) * 255) }
        return ch(r) << 16 | ch(g) << 8 | ch(b)
    }
}
