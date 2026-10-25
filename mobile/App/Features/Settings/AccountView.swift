import SwiftUI

struct AccountView: View {
    @Environment(AppModel.self) private var model
    @State private var demoEnabled = false
    @State private var changingMode = false

    var body: some View {
        @Bindable var model = model
        Form {
            Section("Il tuo stile") {
                Picker("Aspetto", selection: $model.appearance) {
                    ForEach(AppAppearance.allCases) { appearance in
                        Text(appearance.titleIT).tag(appearance)
                    }
                }.accessibilityIdentifier("settings.appearance")
            }
            Section("La tua raccolta") {
                LabeledContent("Tema in uso", value: model.displayTheme.title)
                    .accessibilityElement(children: .ignore)
                    .accessibilityLabel("Tema in uso")
                    .accessibilityValue(model.displayTheme.title)
                    .accessibilityIdentifier("settings.currentTheme")
                Button("Ripristina acquisti", systemImage: "arrow.clockwise") {
                    Task { await model.themes.restore(demo: model.isDemoMode) }
                }.accessibilityIdentifier("themes.restore")
                if let notice = model.themes.notice { Text(notice).font(.subheadline).foregroundStyle(.secondary) }
            }
            Section {
                Toggle("Modalità demo", isOn: $demoEnabled)
                    .accessibilityIdentifier("settings.demo")
                    .disabled(changingMode)
                    .onChange(of: demoEnabled) { _, enabled in changeMode(enabled) }
                if changingMode { ProgressView("Cambio modalità") }
            } footer: {
                Text("Esplora tutte le funzioni senza collegare un dispositivo.")
            }
        }
        .navigationTitle("Impostazioni")
        .onAppear { demoEnabled = model.isDemoMode }
        .onChange(of: model.isDemoMode) { _, enabled in demoEnabled = enabled }
    }

    private func changeMode(_ enabled: Bool) {
        guard enabled != model.isDemoMode, !changingMode else { return }
        changingMode = true
        Task {
            await model.setDemoMode(enabled)
            demoEnabled = model.isDemoMode
            changingMode = false
        }
    }
}
