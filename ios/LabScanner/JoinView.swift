// Joining a party's event: from its link (or QR code), or by pasting the link.
// Says plainly what joining does, and asks the guest to agree to the house
// rules before anything is uploaded (App Store guideline 1.2).
import SwiftUI

let privacyURL = URL(string: "https://pixelcade.org/privacy/")!

struct JoinView: View {
    /// From a link; nil = the guest pastes one.
    var invite: Settings.Invite?
    var joined: () -> Void = {}
    @Environment(\.dismiss) private var dismiss
    @State private var pasted = ""
    @State private var agreed = false
    @State private var resolved: Settings.Invite?   // from a scanned / pasted link
    @State private var scanning = false
    @State private var looking = false
    @State private var lookFailed = false

    private var current: Settings.Invite? { invite ?? resolved }

    /// A scanned or pasted link: the event's settings, read from the page if need be.
    private func lookUp(_ text: String) {
        let t = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty else { resolved = nil; lookFailed = false; return }
        looking = true
        Task {
            let inv = await EventLink.resolve(t)
            looking = false
            resolved = inv
            lookFailed = inv == nil
        }
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    Text("JOIN A LAB-7 EVENT").font(mono(22)).foregroundColor(.labGreen)
                    if invite == nil {
                        Text("Scan the event's QR code (on the invitation or at the party), or paste the event link.")
                            .font(mono(13, .regular)).foregroundColor(.labWhite)
                        Button { scanning = true } label: {
                            HStack(spacing: 10) {
                                Image(systemName: "qrcode.viewfinder").font(.system(size: 24))
                                Text("SCAN THE EVENT QR CODE").font(mono(16))
                            }
                            .foregroundColor(.black).frame(maxWidth: .infinity).frame(height: 54)
                            .background(RoundedRectangle(cornerRadius: 12).fill(Color.labCyan))
                        }
                        TextField("or paste: https://…  /  lab7://join?…", text: $pasted, axis: .vertical)
                            .font(mono(13, .regular)).textInputAutocapitalization(.never).autocorrectionDisabled()
                            .padding(10).overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.labDim))
                            .onChange(of: pasted) { _, v in lookUp(v) }
                        if looking {
                            HStack { ProgressView().tint(.labGreen); Text("Contacting the lab…").font(mono(12, .regular)) }
                        } else if lookFailed {
                            Text("That doesn't look like a LAB-7 event.").font(mono(12, .regular)).foregroundColor(.labRed)
                        }
                    }
                    if let inv = current {
                        VStack(alignment: .leading, spacing: 6) {
                            Text("EVENT").font(mono(11)).foregroundColor(.labDim)
                            Text(inv.event.isEmpty ? "LAB-7 EVENT" : inv.event).font(mono(20)).foregroundColor(.labWhite)
                            if let m = inv.midnight {
                                Text("MIDNIGHT PROTOCOL: \(m.formatted(date: .abbreviated, time: .shortened))")
                                    .font(mono(12, .regular)).foregroundColor(.labAmber)
                            }
                        }
                        .padding(12).frame(maxWidth: .infinity, alignment: .leading)
                        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.labDim))
                    }
                    VStack(alignment: .leading, spacing: 10) {
                        Text("WHAT JOINING DOES").font(mono(13)).foregroundColor(.labGreen)
                        bullet("ID cards from your humanity tests go to the event's evidence wall, which everyone at the event can see. Cards also stay on your phone.")
                        bullet("Your CONTAIN shots and humanity tests count toward the agent leaderboard, under the agent name you choose.")
                        bullet("You see the evidence wall and the leaderboard in the app.")
                        bullet("A reminder before the event's midnight protocol, if you allow notifications.")
                        bullet("No account, no email, no contacts, no location.")
                    }
                    VStack(alignment: .leading, spacing: 10) {
                        Text("HOUSE RULES").font(mono(13)).foregroundColor(.labGreen)
                        bullet("Only photograph people who are happy to be on the wall.")
                        bullet("Nothing offensive, explicit or hurtful. The host removes it, and anyone can report a photo: reported photos disappear at once.")
                        bullet("You can hide all photos from any phone on the wall.")
                    }
                    Toggle(isOn: $agreed) {
                        Text("I agree to the house rules").font(mono(14)).foregroundColor(.labWhite)
                    }
                    .tint(.labGreen)
                    Link("Privacy policy", destination: privacyURL).font(mono(13, .regular))
                    Button {
                        guard let inv = current else { return }
                        Settings.shared.join(inv)
                        dismiss()
                        joined()
                    } label: {
                        Text("JOIN EVENT").font(mono(22)).foregroundColor(.black)
                            .frame(maxWidth: .infinity).frame(height: 56)
                            .background(RoundedRectangle(cornerRadius: 14).fill(agreed && current != nil ? Color.labGreen : Color.labDim))
                    }
                    .disabled(!agreed || current == nil)
                }
                .padding(20)
            }
            .background(Color.black.ignoresSafeArea())
            .toolbar { Button("Cancel") { dismiss() } }
            .sheet(isPresented: $scanning) {
                ZStack(alignment: .bottom) {
                    QRScannerView { code in
                        scanning = false
                        pasted = code      // shows what was scanned, and looks it up
                    }
                    .ignoresSafeArea()
                    Text("POINT AT THE EVENT'S QR CODE").font(mono(15)).foregroundColor(.labGreen)
                        .padding(12).background(Color.black.opacity(0.75)).padding(.bottom, 40)
                }
            }
        }
        .preferredColorScheme(.dark)
    }

    private func bullet(_ s: String) -> some View {
        HStack(alignment: .top, spacing: 8) {
            Text("›").font(mono(13)).foregroundColor(.labGreen)
            Text(s).font(mono(13, .regular)).foregroundColor(.labWhite).fixedSize(horizontal: false, vertical: true)
        }
    }
}
