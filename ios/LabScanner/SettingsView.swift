// In-app settings: everything the ESP32 scanner keeps in secrets.h / config.h.
import SwiftUI

struct SettingsView: View {
    @ObservedObject private var s = Settings.shared
    @ObservedObject private var up = Uploader.shared
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Name on the ID card", text: $s.researcherName).textInputAutocapitalization(.characters)
                    Stepper("Compromised: \(s.compromisedPercent)%", value: $s.compromisedPercent, in: 0...100, step: 5)
                } header: { Text("Humanity test") } footer: {
                    Text("\"AUTHORISED: …\" on every card, and how often a guest comes out COMPROMISED.")
                }
                Section {
                    Stepper("Signal strong: \(s.nearDbm) dBm", value: $s.nearDbm, in: -100...(-30))
                    Stepper("Target locked: \(s.closeDbm) dBm", value: $s.closeDbm, in: -100...(-30))
                } header: { Text("Distance") } footer: {
                    Text("Stand at each distance from the costume and read the dBm on the main screen. Defaults -70 / -58.")
                }
                Section {
                    TextField("https://your-service….run.app", text: $s.serviceURL)
                        .keyboardType(.URL).textInputAutocapitalization(.never).autocorrectionDisabled()
                    SecureField("Upload key", text: $s.uploadKey)
                    TextField("Shown on the card (e.g. YOURSITE.COM/HALLOWEEN)", text: $s.siteText)
                        .textInputAutocapitalization(.characters)
                    LabeledContent("Waiting", value: "\(up.pending)")
                    LabeledContent("Last", value: s.uploadConfigured ? up.lastStatus : "not set up")
                    Button("Send now") { up.kick() }.disabled(!s.uploadConfigured)
                } header: { Text("Photo upload (optional)") } footer: {
                    Text("Your own photo service: cloud/README.md in the rogue-ai-costume repo. Empty = cards stay on this phone (Files app › LAB-7 › photos).")
                }
                Section {
                    Text("Keep this app open with the screen on: iOS only sends the CONTAIN beacon while the app is in front.")
                        .font(.footnote).foregroundColor(.secondary)
                }
            }
            .navigationTitle("LAB-7 settings")
            .toolbar { Button("Done") { dismiss() } }
        }
        .preferredColorScheme(.dark)
    }
}
