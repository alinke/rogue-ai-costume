// Settings: the LED controller (LEDLINK on a Gledopto), and its LED layout --
// the zones along the chain in wiring order, which are lit and which stay dark
// -- with test patterns to check it against the real strips. The layout comes
// from the controller itself (the one built into its firmware until it's saved
// here), so a working setup shows up as it is.
import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var link: JacketLink
    @Environment(\.dismiss) private var dismiss
    @State private var zones: [LedZone] = []
    @State private var loadedFrom: LedLayout?
    @State private var testing: String?
    @State private var rangeFrom = 0
    @State private var rangeTo = 9
    @State private var confirmReset = false

    var body: some View {
        NavigationStack {
            Form {
                controllerSection
                Section {
                    NavigationLink {
                        LayoutHelp()
                    } label: {
                        Label("How the LED layout works", systemImage: "questionmark.circle")
                    }
                }
                if let layout = link.layout {
                    testSection(layout)
                    zoneSection(layout)
                    saveSection(layout)
                }
                Section("About") {
                    Link(destination: URL(string: "https://pixelcade.org/app-privacy/")!) {
                        Label("Privacy policy", systemImage: "hand.raised")
                    }
                    Link(destination: URL(string: "https://github.com/alinke/rogue-ai-costume")!) {
                        Label("Source code and build guide", systemImage: "chevron.left.forwardslash.chevron.right")
                    }
                }
            }
            .navigationTitle("Settings")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) { if link.layout != nil { EditButton() } }
                ToolbarItem(placement: .topBarTrailing) { Button("Done") { dismiss() } }
            }
            .onAppear {
                link.readLayout()
                load()
            }
            .onChange(of: link.layout) { _, _ in load() }
            .onDisappear { if testing != nil { link.test(.off) } }
            .confirmationDialog("Go back to the layout built into the controller's firmware?", isPresented: $confirmReset,
                                titleVisibility: .visible) {
                Button("Reset layout", role: .destructive) { link.resetLayout() }
            }
        }
    }

    /// Takes the controller's layout, unless there are unsaved edits to keep.
    private func load() {
        guard let l = link.layout else { return }
        if loadedFrom == nil || zones == loadedFrom?.zones { zones = l.zones }
        loadedFrom = l
    }

    private var edited: Bool { loadedFrom.map { zones != $0.zones } ?? false }

    // MARK: Controller

    private var controllerSection: some View {
        Section {
            HStack(spacing: 12) {
                Image(systemName: detected ? "checkmark.circle.fill" : "xmark.circle.fill")
                    .font(.title2).foregroundStyle(detected ? .green : .orange)
                VStack(alignment: .leading, spacing: 2) {
                    Text(detected ? "Gledopto controller detected" : "No controller found").font(.headline)
                    Text(detail).font(.caption).foregroundStyle(.secondary)
                }
            }
            if link.demo {
                Text("Demo: the zones below are a sample. Edits and tests do nothing until a controller is connected.")
                    .font(.caption).foregroundStyle(.orange)
            }
            if let l = link.layout, !link.demo {
                LabeledContent("Name", value: link.controllerName.isEmpty ? "LEDLINK" : link.controllerName)
                LabeledContent("Zones", value: "\(l.zones.count)")
                LabeledContent("LEDs", value: "\(l.total) on the chain, \(l.lit) lit")
                LabeledContent("Data pin", value: "GPIO\(l.dataPin)")
                LabeledContent("Current limit", value: String(format: "%.1f A", Double(l.maxMilliamps) / 1000))
                LabeledContent("Wi-Fi", value: l.address.isEmpty ? "not connected" : l.address)
                LabeledContent("Layout", value: l.saved ? "set in this app" : "built into the firmware")
            }
        } header: {
            Text("Controller")
        }
    }

    private var detected: Bool { link.state == .connected }

    private var detail: String {
        if link.demo && link.state != .connecting && link.state != .bluetoothOff {
            return "Looking over Bluetooth -- is it powered and running LEDLINK? Below: a sample layout (demo)"
        }
        switch link.state {
        case .bluetoothOff: return "Bluetooth is off"
        case .searching: return "Looking over Bluetooth -- is the controller powered?"
        case .connecting: return "Found it, connecting..."
        case .connected:
            return link.layout == nil ? "Connected, but its firmware has no settings: update LEDLINK" : "Connected over Bluetooth"
        }
    }

    // MARK: Test

    private func testSection(_ l: LedLayout) -> some View {
        Section {
            testButton("Number every LED", key: "numbered", symbol: "number") { link.test(.numbered) }
            testButton("Show the lit LEDs", key: "lit", symbol: "lightbulb.max") { link.test(.lit) }
            HStack {
                Text("LEDs")
                TextField("from", value: $rangeFrom, format: .number).keyboardType(.numberPad).multilineTextAlignment(.center)
                    .frame(width: 60).textFieldStyle(.roundedBorder)
                Text("to")
                TextField("to", value: $rangeTo, format: .number).keyboardType(.numberPad).multilineTextAlignment(.center)
                    .frame(width: 60).textFieldStyle(.roundedBorder)
                Spacer()
                Button("Light") {
                    testing = "range"
                    link.test(.range, max(0, rangeFrom), max(rangeFrom, rangeTo))
                }
                .buttonStyle(.bordered)
            }
            if testing != nil {
                Button(role: .destructive) {
                    testing = nil
                    link.test(.off)
                } label: {
                    Label("Stop the test", systemImage: "stop.circle")
                }
            }
        } header: {
            Text("Test")
        } footer: {
            Text("Numbered: the first LED green, every 10th white, every 50th red, every 100th blue. Lit: exactly what the zones below light, in dim white. LED numbers count along the wire from 0. The test stops when you leave this page.")
        }
    }

    private func testButton(_ title: String, key: String, symbol: String, run: @escaping () -> Void) -> some View {
        Button {
            testing = key
            run()
        } label: {
            HStack {
                Label(title, systemImage: symbol)
                Spacer()
                if testing == key { Text("ON").font(.caption.bold()).foregroundStyle(.green) }
            }
        }
    }

    // MARK: Zones

    private func zoneSection(_ l: LedLayout) -> some View {
        var shown = l
        shown.zones = zones
        return Section {
            ForEach($zones) { $zone in
                let i = zones.firstIndex(where: { $0.id == zone.id }) ?? 0
                ZoneRow(zone: $zone, number: i + 1, start: shown.start(of: i),
                        skipped: l.matrixSkipped && zone.kind == .matrix, testing: testing == "zone\(i)") {
                    testing = "zone\(i)"
                    let a = shown.start(of: i)
                    link.test(.range, a, a + max(zone.count, 1) - 1)
                }
            }
            .onDelete { zones.remove(atOffsets: $0) }
            .onMove { zones.move(fromOffsets: $0, toOffset: $1) }
            if zones.count < 16 {
                Button {
                    zones.append(LedZone(kind: .front, side: .none, reversed: false, count: 10))
                } label: {
                    Label("Add a zone", systemImage: "plus.circle")
                }
            }
        } header: {
            Text("Zones, in wiring order")
        } footer: {
            Text("Each zone is a stretch of the chain, starting from the controller. Front and Sleeve are lit (the looks run up the front and down the sleeves); Dark stays off -- strip tucked out of sight or past the end of a run. Left and Right pick which turn signal a zone shows. Light a zone to check where it really is, then save.")
        }
    }

    // MARK: Save

    private func saveSection(_ l: LedLayout) -> some View {
        Section {
            Button {
                var n = l
                n.zones = zones
                link.saveLayout(n)
                loadedFrom = nil  // take what comes back
            } label: {
                Label("Save to the controller", systemImage: "square.and.arrow.down")
            }
            .disabled(!edited || zones.isEmpty || zones.reduce(0) { $0 + $1.count } > max(l.maxLeds, l.total))
            if edited {
                Button("Undo changes") { zones = l.zones }
            }
            if l.saved {
                Button("Reset to the firmware's layout", role: .destructive) { confirmReset = true }
            }
        } footer: {
            if zones.reduce(0, { $0 + $1.count }) > max(l.maxLeds, l.total) {
                Text("Too many LEDs: this controller takes up to \(l.maxLeds).").foregroundStyle(.red)
            }
        }
    }
}

private struct ZoneRow: View {
    @Binding var zone: LedZone
    let number: Int
    let start: Int
    let skipped: Bool
    let testing: Bool
    let light: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("\(number)").font(.caption.monospacedDigit().bold())
                    .frame(width: 24, height: 24)
                    .background(Circle().fill(zone.kind.lit ? Color.cyan.opacity(0.3) : Color.gray.opacity(0.3)))
                Picker("Type", selection: $zone.kind) {
                    ForEach(LedZone.Kind.allCases) { Text($0.title).tag($0) }
                }
                .labelsHidden()
                if zone.kind == .front || zone.kind == .sleeve {
                    Picker("Side", selection: $zone.side) {
                        ForEach(LedZone.Side.allCases) { Text($0.title).tag($0) }
                    }
                    .labelsHidden()
                }
                Spacer()
                Button(action: light) {
                    Image(systemName: testing ? "lightbulb.fill" : "lightbulb")
                }
                .buttonStyle(.bordered)
                .tint(testing ? .green : nil)
                .disabled(skipped || zone.count == 0)
                .accessibilityLabel("Light zone \(number)")
            }
            HStack {
                TextField("LEDs", value: $zone.count, format: .number)
                    .keyboardType(.numberPad).multilineTextAlignment(.center)
                    .frame(width: 70).textFieldStyle(.roundedBorder)
                Stepper("LEDs", value: $zone.count, in: 0...1200).labelsHidden()
                Spacer()
                Text(skipped ? "not wired now" : zone.count > 0 ? "\(start)-\(start + zone.count - 1)" : "empty")
                    .font(.caption.monospacedDigit()).foregroundStyle(.secondary)
            }
            if let ends = ends {
                Picker("First LED at the", selection: $zone.reversed) {
                    Text(ends.0).tag(false)
                    Text(ends.1).tag(true)
                }
                .pickerStyle(.segmented)
            }
        }
        .padding(.vertical, 4)
    }

    /// Where the zone's first LED is, not reversed / reversed (leds.cpp: a sleeve runs cuff -> shoulder unless reversed).
    private var ends: (String, String)? {
        switch zone.kind {
        case .front: ("Starts at the hem", "Starts at the collar")
        case .sleeve: ("Starts at the cuff", "Starts at the shoulder")
        default: nil
        }
    }
}

/// Help: what a zone is, lit and dark, sides, direction, and how to check it all with the tests.
private struct LayoutHelp: View {
    var body: some View {
        List {
            topic("One long chain", "point.topleft.down.to.point.bottomright.curvepath",
                  "Your strips are wired one after another into one chain, starting at the controller. The LEDs are numbered along the wire from 0: the first LED after the controller is 0, the next 1, and so on to the end of the last strip.")
            topic("Zones", "square.split.1x2",
                  "A zone is one stretch of that chain -- a run up the front, a sleeve, or a piece of strip you don't want lit. The zones are listed in wiring order, and each one's count says how many LEDs it has. Together they add up to the whole chain.")
            topic("Lit and dark", "lightbulb.slash",
                  "Front and Sleeve zones light up: the looks run up the front (hem to collar) and down the sleeves (shoulder to cuff). A Dark zone stays off -- strip tucked inside the jacket, the bit that runs across the back from one sleeve to the next, or strip left over past the end of a run. The 8x8 matrix and Eye pod are for the ROGUE.AI costume; leave them out otherwise.")
            topic("Left and right", "arrow.left.and.right",
                  "Left and Right are the wearer's left and right (as you wear it, not as someone facing you sees it). They decide which turn signal a zone shows; the hazard lights use both.")
            topic("Which end is first", "arrow.up.and.down",
                  "LEDs are numbered from the end where the data wire comes in. \"Starts at the hem\" or \"at the collar\" (Front), and \"at the cuff\" or \"at the shoulder\" (Sleeve), tell the jacket which end that is, so the looks run the right way: flames climb from the cuffs, code rain falls from the shoulders, a turn signal sweeps out to the cuff. If a look runs backwards on one zone, flip that setting.")
            topic("Checking it", "checklist",
                  "Number every LED shows the numbering: LED 0 green, every 10th white, every 50th red, every 100th blue -- count along to find where each zone really starts and ends. Each zone's light button lights just that zone, so you can see it lands exactly on the part it should. Show the lit LEDs lights everything that isn't dark, so stray lit LEDs inside the jacket stand out.")
            topic("Saving", "square.and.arrow.down",
                  "Changes only reach the jacket when you tap Save to the controller; it keeps the layout from then on, even when it's switched off. Reset to the firmware's layout goes back to the one it came with.")
        }
        .navigationTitle("LED layout")
        .navigationBarTitleDisplayMode(.inline)
    }

    private func topic(_ title: String, _ symbol: String, _ text: String) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Label(title, systemImage: symbol).font(.headline)
            Text(text).font(.subheadline).foregroundStyle(.secondary)
        }
        .padding(.vertical, 4)
    }
}
