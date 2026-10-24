// The agent dossier: the identity LAB-7 planted in the AI's own agent network
// (the same codes as the costume's QR landing page), whose override code works
// on the costume's kill-switch keypad; the leaderboard name and score; the
// countdown to the event's midnight protocol; joining / leaving the event.
import SwiftUI

struct AgentView: View {
    @ObservedObject private var settings = Settings.shared
    @State private var showJoin = false
    @State private var askLeave = false
    @State private var nameDraft = ""
    @FocusState private var editingName: Bool

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            Scanlines().ignoresSafeArea()
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("AGENT DOSSIER").font(mono(24)).foregroundColor(.labGreen)
                        Text("LAB-7 // DOUBLE AGENT PROGRAM").font(mono(11, .regular)).foregroundColor(.labDim)
                    }
                    if settings.joined { InvestigationPanel() }
                    dossier
                    leaderboardName
                    if settings.joined, let m = settings.midnightDate { countdown(m) }
                    event
                    howTo
                }
                .padding(18)
            }
            .scrollDismissesKeyboard(.interactively)
        }
        .sheet(isPresented: $showJoin) { JoinView() }
        .onAppear { nameDraft = settings.agentName }
        .confirmationDialog("Leave \(settings.eventName)?", isPresented: $askLeave, titleVisibility: .visible) {
            Button("Leave event", role: .destructive) { settings.leaveEvent() }
        } message: {
            Text("Nothing more is uploaded. Photos already on the wall stay there until the host clears it.")
        }
    }

    private var dossier: some View {
        let a = settings.agent
        return VStack(alignment: .leading, spacing: 10) {
            row("UNIT", "\(a.unit)")
            row("CODENAME", a.codename)
            row("COVER ROLE", a.role)
            row("CLEARANCE", a.clearance)
            Divider().background(Color.labDim)
            Text("OVERRIDE CODE").font(mono(11)).foregroundColor(.labDim)
            Text(a.code.map(String.init).joined(separator: " ")).font(mono(54, .heavy)).foregroundColor(.labRed)
                .frame(maxWidth: .infinity)
            Text("LAB-7 broke into the AI's agent network and enrolled you as one of its agents. Find the entity, open its kill-switch keypad on its chest screen, and type this code. It thinks you work for it. Use that.")
                .font(mono(12, .regular)).foregroundColor(.labWhite).fixedSize(horizontal: false, vertical: true)
        }
        .padding(14)
        .overlay(RoundedRectangle(cornerRadius: 12).stroke(Color.labRed.opacity(0.7), lineWidth: 2))
    }

    private var leaderboardName: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("LEADERBOARD NAME").font(mono(11)).foregroundColor(.labDim)
            HStack {
                TextField(settings.agent.codename, text: $nameDraft)
                    .font(mono(18)).foregroundColor(.labGreen)
                    .textInputAutocapitalization(.characters).autocorrectionDisabled()
                    .focused($editingName)
                    .submitLabel(.done)
                    .onSubmit(saveName)
                    .onChange(of: nameDraft) { _, v in
                        if v.count > 20 { nameDraft = String(v.prefix(20)) }
                    }
                if editingName {
                    Button("SAVE", action: saveName).font(mono(14)).foregroundColor(.labGreen)
                }
            }
            .padding(10).overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.labDim))
            HStack(spacing: 24) {
                stat("\(settings.containCount)", "CONTAIN SHOTS")
                stat("\(settings.testCount)", "HUMANITY TESTS")
                stat("\(settings.squadCount)", "SQUAD WINS")
            }
            Text(settings.joined ? "Shown on the event's TOP AGENTS board. Leave it empty to use your codename."
                                 : "Join an event to get on its TOP AGENTS board.")
                .font(mono(11, .regular)).foregroundColor(.labDim)
        }
    }

    private func saveName() {
        settings.agentName = nameDraft.trimmingCharacters(in: .whitespaces).uppercased()
        nameDraft = settings.agentName
        editingName = false
    }

    private func countdown(_ m: Date) -> some View {
        TimelineView(.periodic(from: .now, by: 1)) { ctx in
            let left = Int(m.timeIntervalSince(ctx.date))
            VStack(alignment: .leading, spacing: 4) {
                Text("MIDNIGHT PROTOCOL").font(mono(11)).foregroundColor(.labDim)
                if left > 0 {
                    Text(left >= 86400 ? String(format: "%dD %02d:%02d:%02d", left / 86400, left / 3600 % 24, left / 60 % 60, left % 60)
                                       : String(format: "%02d:%02d:%02d", left / 3600, left / 60 % 60, left % 60))
                        .font(mono(40, .heavy)).foregroundColor(left < 900 ? .labRed : .labAmber)
                    Text("Something happens at \(m.formatted(date: .omitted, time: .shortened)). Be near the entity.")
                        .font(mono(11, .regular)).foregroundColor(.labWhite)
                } else {
                    Text("IT IS HAPPENING").font(mono(32, .heavy)).foregroundColor(.labRed)
                }
            }
            .padding(12).frame(maxWidth: .infinity, alignment: .leading)
            .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.labAmber.opacity(0.6)))
        }
    }

    private var event: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("EVENT").font(mono(11)).foregroundColor(.labDim)
            if settings.joined {
                Text(settings.eventName.isEmpty ? "CONNECTED TO THE LAB" : settings.eventName.uppercased())
                    .font(mono(18)).foregroundColor(.labWhite)
                Button("Leave event") { askLeave = true }.font(mono(13)).foregroundColor(.labRed)
            } else {
                Text("NOT JOINED").font(mono(18)).foregroundColor(.labWhite)
                Button { showJoin = true } label: {
                    Text("JOIN AN EVENT").font(mono(16)).foregroundColor(.black)
                        .frame(maxWidth: .infinity).frame(height: 46)
                        .background(RoundedRectangle(cornerRadius: 10).fill(Color.labGreen))
                }
            }
            Link("Privacy policy", destination: privacyURL).font(mono(12, .regular))
        }
    }

    private var howTo: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("FIELD MANUAL").font(mono(11)).foregroundColor(.labDim)
            ForEach(["SCANNER tab: the meter climbs (and the phone buzzes faster) as the entity gets closer.",
                     "When it says SIGNAL STRONG, press CONTAIN. Alone, it always escapes. Three scanners firing together can actually contain it.",
                     "INFECTION: stay close to an infected scanner (or the entity) and yours gets infected: CONTAIN jams. Stand at the LAB-7 checkpoint iPad during a humanity check to be cured.",
                     "HUMANITY TEST: photograph a guest. LAB-7 decides if they are human.",
                     "EVIDENCE tab: every card, every specimen, and the top agents."], id: \.self) { s in
                HStack(alignment: .top, spacing: 8) {
                    Text("›").foregroundColor(.labGreen)
                    Text(s).foregroundColor(.labWhite).fixedSize(horizontal: false, vertical: true)
                }
                .font(mono(12, .regular))
            }
        }
    }

    private func row(_ k: String, _ v: String) -> some View {
        HStack(alignment: .firstTextBaseline) {
            Text(k).font(mono(11)).foregroundColor(.labDim).frame(width: 100, alignment: .leading)
            Text(v).font(mono(15)).foregroundColor(.labWhite)
        }
    }

    private func stat(_ n: String, _ label: String) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            Text(n).font(mono(26, .heavy)).foregroundColor(.labGreen)
            Text(label).font(mono(10)).foregroundColor(.labDim)
        }
    }
}
