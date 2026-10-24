// The evidence wall in the app: the event's photos (the survivor registry and
// the AI's specimens), the agent leaderboard, and a photo full screen with
// share / report / hide-this-phone.
import SwiftUI

struct WallView: View {
    @ObservedObject private var settings = Settings.shared
    @State private var lab: [WallPhoto] = []
    @State private var ai: [WallPhoto] = []
    @State private var leaders: [LeaderEntry] = []
    @State private var agentsTotal = 0
    @State private var infected = 0
    @State private var game: GamePublic?
    @State private var filter = 0          // 0 all, 1 humans, 2 specimens, 3 mine
    @State private var status = ""
    @State private var open: WallPhoto?
    @State private var showJoin = false
    @State private var demoImages: [String: UIImage] = [:]   // demo mode: the sample cards

    private func mine(_ p: WallPhoto) -> Bool { p.dev == settings.deviceTag }

    private var shown: [WallPhoto] {
        let all = filter == 1 ? lab : filter == 2 ? ai : filter == 3 ? lab.filter(mine)
            : (lab + ai).sorted { $0.t > $1.t }
        return all.filter { p in !(p.dev.map { !$0.isEmpty && settings.blocked.contains($0) } ?? false) }
    }

    var body: some View {
        NavigationStack {
            ZStack {
                Color.black.ignoresSafeArea()
                if settings.joined || settings.demoMode {
                    wall
                } else {
                    notJoined
                }
            }
            .toolbar(.hidden, for: .navigationBar)
        }
        .fullScreenCover(item: $open) { p in
            PhotoViewer(photo: p, localImage: demoImages[p.name]) { removed in
                lab.removeAll { $0.id == removed.id }
                ai.removeAll { $0.id == removed.id }
            }
        }
        .sheet(isPresented: $showJoin) { JoinView() }
        .task(id: "\(settings.serviceURL)|\(settings.demoMode)") {
            if !settings.joined && settings.demoMode { loadDemo() }
            // Refresh while this tab is showing.
            while !Task.isCancelled && settings.joined {
                await load()
                try? await Task.sleep(for: .seconds(20))
            }
        }
    }

    private var wall: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("EVIDENCE WALL").font(mono(24)).foregroundColor(.labGreen)
                    Text(settings.joined ? settings.eventName.uppercased() : "DEMO MODE: SAMPLE EVIDENCE")
                        .font(mono(12, .regular)).foregroundColor(settings.joined ? .labDim : .labAmber)
                }
                nightStatus
                board
                Picker("", selection: $filter) {
                    Text("ALL \(lab.count + ai.count)").tag(0)
                    Text("HUMANS \(lab.count)").tag(1)
                    Text("SPECIMENS \(ai.count)").tag(2)
                    Text("MINE \(lab.filter(mine).count)").tag(3)
                }
                .pickerStyle(.segmented)
                if shown.isEmpty {
                    Text(!status.isEmpty ? status : filter == 3 ? "NOTHING FROM YOU YET. RUN A HUMANITY TEST."
                         : "NO EVIDENCE YET. RUN A HUMANITY TEST.")
                        .font(mono(13, .regular)).foregroundColor(.labDim).padding(.vertical, 30)
                        .frame(maxWidth: .infinity)
                }
                LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 6), count: 3), spacing: 6) {
                    ForEach(shown) { p in
                        Button { open = p } label: {
                            // A fixed 3:4 cell; the photo fills it and is cropped to it.
                            Color(hex: 0x00240A)
                                .aspectRatio(0.75, contentMode: .fit)
                                .overlay {
                                    if let img = demoImages[p.name] {
                                        Image(uiImage: img).resizable().scaledToFill()
                                    } else {
                                        AsyncImage(url: URL(string: p.url)) { img in
                                            img.resizable().scaledToFill()
                                        } placeholder: {
                                            ProgressView().tint(.labDim)
                                        }
                                    }
                                }
                                .clipped()
                            .overlay(alignment: .topTrailing) {
                                if mine(p) {
                                    Text("YOURS").font(mono(8)).foregroundColor(.black)
                                        .padding(.horizontal, 4).padding(.vertical, 2).background(Color.labCyan)
                                }
                            }
                            .overlay(alignment: .bottomLeading) {
                                let rogue = p.label.hasPrefix("COMPROMISED")
                                Text(p.isAI ? "SPECIMEN" : rogue ? "COMPROMISED" : "HUMAN").font(mono(8)).foregroundColor(.black)
                                    .padding(.horizontal, 4).padding(.vertical, 2)
                                    .background(p.isAI || rogue ? Color.labRed : Color.labGreen)
                            }
                        }
                    }
                }
            }
            .padding(14)
        }
        .refreshable { await load() }
    }

    private var board: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text("TOP AGENTS").font(mono(14)).foregroundColor(.labAmber)
                Spacer()
                Text("\(agentsTotal) IN THE FIELD").font(mono(11, .regular)).foregroundColor(.labDim)
            }
            if leaders.isEmpty {
                Text("NO SCORES YET. FIRE A CONTAIN BEAM.").font(mono(12, .regular)).foregroundColor(.labDim)
            }
            ForEach(Array(leaders.prefix(5).enumerated()), id: \.offset) { i, a in
                let me = a.name == settings.leaderboardName.uppercased()
                HStack {
                    Text("\(i + 1).").frame(width: 26, alignment: .leading)
                    Text(a.name).lineLimit(1)
                    Spacer()
                    Text("\((a.squads ?? 0) > 0 ? "\(a.squads ?? 0) ★  " : "")\(a.contains) ⚡  \(a.tests) ◉")
                }
                .font(mono(13, me ? .heavy : .regular))
                .foregroundColor(me ? .labGreen : .labWhite)
            }
            Text("★ SQUAD WINS   ⚡ CONTAIN SHOTS   ◉ HUMANITY TESTS").font(mono(9, .regular)).foregroundColor(.labDim)
        }
        .padding(12)
        .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.labAmber.opacity(0.6)))
    }

    /// The night's state: infected scanners, the traitor game.
    @ViewBuilder private var nightStatus: some View {
        let lines: [(String, Color)] = {
            var l: [(String, Color)] = []
            if infected > 0 { l.append(("☣ INFECTED SCANNERS: \(infected)", .labRed)) }
            switch game?.phase {
            case "active": l.append(("ONE OF YOU WORKS FOR THE AI", .labAmber))
            case "exposed": l.append(("AGENT EXPOSED: \(game?.agentName ?? "") (BY \(game?.byName ?? ""))", .labGreen))
            case "agentWon": l.append(("THE AI'S AGENT WON: \(game?.agentName ?? "")", .labRed))
            default: break
            }
            return l
        }()
        if !lines.isEmpty {
            VStack(alignment: .leading, spacing: 4) {
                ForEach(lines, id: \.0) { Text($0.0).font(mono(13)).foregroundColor($0.1) }
            }
        }
    }

    private var notJoined: some View {
        VStack(spacing: 18) {
            Image(systemName: "lock.shield").font(.system(size: 54)).foregroundColor(.labDim)
            Text("NO EVENT JOINED").font(mono(22)).foregroundColor(.labGreen)
            Text("The evidence wall belongs to a party. Join its event with the link or QR code from your invitation.")
                .font(mono(13, .regular)).foregroundColor(.labWhite).multilineTextAlignment(.center)
            Button { showJoin = true } label: {
                Text("JOIN AN EVENT").font(mono(18)).foregroundColor(.black)
                    .padding(.horizontal, 24).frame(height: 50)
                    .background(RoundedRectangle(cornerRadius: 12).fill(Color.labGreen))
            }
            Text("The scanner and the agent dossier work without one.")
                .font(mono(11, .regular)).foregroundColor(.labDim).multilineTextAlignment(.center)
        }
        .padding(30)
    }

    /// Demo mode without an event: sample cards (silhouettes, no real people) and a sample leaderboard.
    private func loadDemo() {
        guard demoImages.isEmpty else { return }
        var photos: [WallPhoto] = [], images: [String: UIImage] = [:]
        for i in 0..<12 {
            let human = i % 4 != 2
            let pool = (human ? IDCard.humanFindings : IDCard.aiFindings)
            let img = IDCard.render(photo: IDCard.silhouette(i), subject: 40 - i, human: human,
                                    exposure: human ? 5 + (i * 7) % 24 : 70 + i * 2,
                                    findings: [pool[i % pool.count], pool[(i + 3) % pool.count]],
                                    researcher: "LAB-7 FIELD AGENT", site: "THE ROGUE.AI WALL")
            let name = "lab/demo-\(i).jpg"
            images[name] = img
            photos.append(WallPhoto(name: name, url: "", t: String(format: "2026-10-30T23:%02d:00", 59 - i),
                                    label: String(format: "%@ #%03d", human ? "VERIFIED HUMAN" : "COMPROMISED", 40 - i), dev: nil))
        }
        demoImages = images
        lab = photos
        ai = []
        leaders = [LeaderEntry(name: "SPARKLE GOBLIN", contains: 14, tests: 3),
                   LeaderEntry(name: "QUANTUM WAFFLE", contains: 11, tests: 6),
                   LeaderEntry(name: settings.leaderboardName.uppercased(), contains: settings.containCount, tests: settings.testCount),
                   LeaderEntry(name: "DISCO MODEM", contains: 4, tests: 2)]
            .sorted { $0.contains > $1.contains }
        agentsTotal = 23
    }

    private func load() async {
        do {
            let f = try await LabAPI.photos()
            let l = try? await LabAPI.leaders()
            await MainActor.run {
                lab = f.lab
                ai = f.ai
                if let l { leaders = l.agents; agentsTotal = l.total; infected = l.infected; game = l.game }
                status = ""
            }
        } catch {
            await MainActor.run { status = "NO CONNECTION TO THE LAB. PULL TO RETRY." }
        }
    }
}

/// One photo full screen: share it, report it, or hide every photo from that phone.
struct PhotoViewer: View {
    let photo: WallPhoto
    var localImage: UIImage? = nil      // demo cards: no download, nothing to report
    var removed: (WallPhoto) -> Void
    @Environment(\.dismiss) private var dismiss
    @ObservedObject private var settings = Settings.shared
    @State private var image: UIImage?
    @State private var askReport = false
    @State private var askHide = false
    @State private var askDelete = false
    private var isMine: Bool { photo.dev == settings.deviceTag }

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            VStack(spacing: 14) {
                HStack {
                    Text(photo.isAI ? "ROGUE.AI SPECIMEN FILE" : "LAB-7 SURVIVOR REGISTRY").font(mono(13))
                        .foregroundColor(photo.isAI ? .labRed : .labGreen)
                    Spacer()
                    Button { dismiss() } label: {
                        Image(systemName: "xmark").font(.system(size: 20, weight: .bold)).foregroundColor(.labWhite)
                            .frame(width: 44, height: 44)
                    }
                }
                Spacer(minLength: 0)
                if let image {
                    Image(uiImage: image).resizable().scaledToFit()
                } else {
                    ProgressView().tint(.labGreen)
                }
                Spacer(minLength: 0)
                if !photo.label.isEmpty {
                    Text(photo.label).font(mono(13, .regular)).foregroundColor(.labDim)
                }
                HStack(spacing: 12) {
                    if let image {
                        ShareLink(item: Image(uiImage: image), preview: SharePreview("LAB-7 evidence", image: Image(uiImage: image))) {
                            action("SHARE", "square.and.arrow.up", .labGreen)
                        }
                    }
                    if isMine {
                        Button { askDelete = true } label: { action("DELETE", "trash", .labRed) }
                    } else {
                        if localImage == nil {
                            Button { askReport = true } label: { action("REPORT", "flag", .labAmber) }
                        }
                        if let d = photo.dev, !d.isEmpty {
                            Button { askHide = true } label: { action("HIDE PHONE", "eye.slash", .labRed) }
                        }
                    }
                }
            }
            .padding(16)
        }
        .preferredColorScheme(.dark)
        .task {
            if let localImage { image = localImage; return }
            guard let url = URL(string: photo.url),
                  let (data, _) = try? await URLSession.shared.data(from: url) else { return }
            image = UIImage(data: data)
        }
        .confirmationDialog("Report this photo?", isPresented: $askReport, titleVisibility: .visible) {
            Button("Report and hide it", role: .destructive) {
                Task {
                    await LabAPI.report(photo)
                    removed(photo)
                    dismiss()
                }
            }
        } message: {
            Text("It disappears from the evidence wall for everyone right away, and the host reviews it.")
        }
        .confirmationDialog("Delete your photo?", isPresented: $askDelete, titleVisibility: .visible) {
            Button("Delete from the wall", role: .destructive) {
                Task {
                    if await LabAPI.deleteOwn(photo) {
                        removed(photo)
                        dismiss()
                    }
                }
            }
        } message: {
            Text("It's removed from the evidence wall for everyone. Your copy stays on this phone (Files › LAB-7 › photos).")
        }
        .confirmationDialog("Hide every photo from the phone that took this?", isPresented: $askHide,
                            titleVisibility: .visible) {
            Button("Hide them", role: .destructive) {
                if let d = photo.dev, !settings.blocked.contains(d) { settings.blocked.append(d) }
                dismiss()
            }
        } message: {
            Text("Only on your phone. Undo it in the scanner's settings (gear).")
        }
    }

    private func action(_ title: String, _ icon: String, _ color: Color) -> some View {
        VStack(spacing: 4) {
            Image(systemName: icon).font(.system(size: 20))
            Text(title).font(mono(11))
        }
        .foregroundColor(color)
        .frame(maxWidth: .infinity).frame(height: 60)
        .overlay(RoundedRectangle(cornerRadius: 10).stroke(color, lineWidth: 2))
    }
}
