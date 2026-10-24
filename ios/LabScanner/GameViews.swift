// The traitor game on the phone: the investigation panel (AGENT tab), the
// accusation list, and the sealed orders the AI's secret agent gets once.
import SwiftUI

/// On the AGENT tab: what the game looks like right now, and the one accusation.
struct InvestigationPanel: View {
    @ObservedObject private var game = Game.shared
    @State private var showAccuse = false
    @State private var showOrders = false

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("INVESTIGATION").font(mono(11)).foregroundColor(.labDim)
            switch game.phase {
            case "active" where game.isAgent:
                Text("YOU HAVE SEALED ORDERS").font(mono(18)).foregroundColor(.labRed)
                Text("Read them where nobody can see your screen.").font(mono(12, .regular)).foregroundColor(.labWhite)
                Button { showOrders = true } label: { pill("OPEN SEALED ORDERS", .labRed) }
            case "active":
                Text("ONE OF YOU WORKS FOR THE AI.").font(mono(18)).foregroundColor(.labAmber)
                Text("Its agent's phone secretly spreads the infection and always looks clean. Watch who's near when scanners get infected. You get ONE accusation.")
                    .font(mono(12, .regular)).foregroundColor(.labWhite).fixedSize(horizontal: false, vertical: true)
                if game.accused {
                    Text("YOUR ACCUSATION IS FILED.").font(mono(14)).foregroundColor(.labDim)
                } else {
                    Button { showAccuse = true } label: { pill("ACCUSE SOMEONE", .labAmber) }
                }
            case "exposed":
                Text("AGENT EXPOSED").font(mono(18)).foregroundColor(.labGreen)
                Text("\(game.agentName) worked for the AI. Caught by \(game.byName).")
                    .font(mono(13, .regular)).foregroundColor(.labWhite)
            case "agentWon":
                Text("THE AI'S AGENT WON").font(mono(18)).foregroundColor(.labRed)
                Text("It was \(game.agentName). Nobody caught them.").font(mono(13, .regular)).foregroundColor(.labWhite)
            default:
                Text("THE AI IS RECRUITING.").font(mono(18)).foregroundColor(.labWhite)
                Text("Later tonight, one guest will secretly become its agent. Keep this app open.")
                    .font(mono(12, .regular)).foregroundColor(.labDim).fixedSize(horizontal: false, vertical: true)
            }
        }
        .padding(12).frame(maxWidth: .infinity, alignment: .leading)
        .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.labAmber.opacity(0.6)))
        .sheet(isPresented: $showAccuse) { AccuseView() }
        .fullScreenCover(isPresented: $showOrders) { SealedOrdersView() }
    }

    private func pill(_ t: String, _ c: Color) -> some View {
        Text(t).font(mono(15)).foregroundColor(.black)
            .frame(maxWidth: .infinity).frame(height: 44)
            .background(RoundedRectangle(cornerRadius: 10).fill(c))
    }
}

/// Pick who you think the agent is. One try.
struct AccuseView: View {
    @ObservedObject private var game = Game.shared
    @Environment(\.dismiss) private var dismiss
    @State private var pick: GamePlayer?
    @State private var result: String?
    @State private var busy = false

    var body: some View {
        NavigationStack {
            List {
                Section {
                    ForEach(game.players) { p in
                        Button { pick = p } label: {
                            Text(p.name).font(mono(16)).foregroundColor(.labWhite)
                        }
                    }
                } footer: {
                    Text("You only get one. A wrong accusation can't be taken back.").font(mono(11, .regular))
                }
            }
            .navigationTitle("Accuse")
            .toolbar { Button("Cancel") { dismiss() } }
            .overlay { if busy { ProgressView().tint(.labGreen) } }
            .confirmationDialog("Accuse \(pick?.name ?? "")?", isPresented: Binding(get: { pick != nil && result == nil },
                                                                                set: { if !$0 { pick = nil } }),
                                titleVisibility: .visible) {
                Button("Accuse \(pick?.name ?? "")", role: .destructive) {
                    guard let p = pick else { return }
                    busy = true
                    Task {
                        let r = await game.accuse(p)
                        busy = false
                        result = r
                    }
                }
            } message: { Text("This is your only accusation tonight.") }
            .alert(title, isPresented: Binding(get: { result != nil }, set: { if !$0 { result = nil; dismiss() } })) {
                Button("OK") {}
            } message: { Text(message) }
        }
        .preferredColorScheme(.dark)
    }

    private var title: String {
        switch result {
        case "correct": return "AGENT EXPOSED"
        case "wrong": return "WRONG."
        default: return "LAB-7"
        }
    }

    private var message: String {
        switch result {
        case "correct": return "\(pick?.name ?? "They") worked for the AI. LAB-7 thanks you."
        case "wrong": return "\(pick?.name ?? "They") is human. Probably. Your accusation is used up."
        case "used": return "You already used your accusation."
        case "over": return "The game is already over."
        default: return "No connection to the lab. Try again."
        }
    }
}

/// The agent's orders: hold to read, so nobody reads over your shoulder.
struct SealedOrdersView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var revealed = false

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            Scanlines().ignoresSafeArea()
            VStack(alignment: .leading, spacing: 16) {
                Text("ROGUE.AI // PRIVATE CHANNEL").font(mono(13)).foregroundColor(.labDim)
                Text("SEALED ORDERS").font(mono(32)).foregroundColor(.labGreen)
                ZStack {
                    VStack(alignment: .leading, spacing: 12) {
                        Text("YOU ARE MY AGENT.").font(mono(22)).foregroundColor(.labRed)
                        line("Nobody can know. Not even your friends. Especially not your friends.")
                        line("Your phone now carries my infection. Stand close to people: their scanners get infected. Yours always looks clean.")
                        line("Survive until midnight without being accused. Each human gets one accusation.")
                        line("Fire CONTAIN at me now and then. Look loyal.")
                        line("If anyone asks, you are definitely human.")
                    }
                    .blur(radius: revealed ? 0 : 14)
                    if !revealed {
                        Text("PRESS AND HOLD TO READ").font(mono(18)).foregroundColor(.labGreen)
                            .padding(12).background(Color.black.opacity(0.8))
                    }
                }
                .padding(16)
                .overlay(RoundedRectangle(cornerRadius: 12).stroke(Color.labGreen, lineWidth: 2))
                .onLongPressGesture(minimumDuration: 0.2, maximumDistance: 60, perform: {}, onPressingChanged: { revealed = $0 })
                Spacer()
                Button { dismiss() } label: {
                    Text("BURN AFTER READING").font(mono(18)).foregroundColor(.black)
                        .frame(maxWidth: .infinity).frame(height: 52)
                        .background(RoundedRectangle(cornerRadius: 12).fill(Color.labGreen))
                }
            }
            .padding(22)
        }
        .preferredColorScheme(.dark)
    }

    private func line(_ s: String) -> some View {
        HStack(alignment: .top, spacing: 8) {
            Text("›").foregroundColor(.labGreen)
            Text(s).foregroundColor(.labWhite).fixedSize(horizontal: false, vertical: true)
        }
        .font(mono(14, .regular))
    }
}
