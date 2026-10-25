import SwiftUI

struct AccountView: View {
    @Environment(AppModel.self) private var model
    @State private var demoEnabled = false
    @State private var changingMode = false
    var body: some View {
        @Bindable var model = model
        Form {
            Section {
                Picker("Tema dell’app", selection: $model.appearance) {
                    ForEach(AppAppearance.allCases) { Text($0.titleIT).tag($0) }
                }.accessibilityIdentifier("settings.appearance")
            } header: { Text("Aspetto") } footer: { Text("Automatico segue l’aspetto del telefono.") }
            Section("I tuoi temi") {
                LabeledContent("Tema in uso", value: model.displayTheme.title)
                    .accessibilityElement(children: .ignore).accessibilityLabel("Tema in uso")
                    .accessibilityValue(model.displayTheme.title).accessibilityIdentifier("settings.currentTheme")
                Button("Ripristina acquisti", systemImage: "arrow.clockwise") {
                    Task { await model.themes.restore(demo: model.isDemoMode) }
                }.frame(minHeight: AppTheme.minimumTouch).accessibilityIdentifier("themes.restore")
                if let notice = model.themes.notice { Text(notice).font(.subheadline).foregroundStyle(.secondary) }
            }
            Section {
                Toggle("Modalità demo", isOn: $demoEnabled)
                    .accessibilityIdentifier("settings.demo").disabled(changingMode)
                    .onChange(of: demoEnabled) { _, enabled in changeMode(enabled) }
                if changingMode { ProgressView("Cambio modalità") }
            } header: { Text("Esplora l’app") }
              footer: { Text("Prova le funzioni senza collegare Auto Gauge.") }
            Section {
                Button("Rivedi introduzione", systemImage: "sparkles") { model.onboardingPresented = true }
                    .frame(minHeight: AppTheme.minimumTouch)
                    .disabled(changingMode || model.isSaving || model.gaugeBusy)
                    .accessibilityIdentifier("settings.onboarding")
            } header: { Text("Primi passi") }
              footer: { Text("Ripercorri la guida iniziale quando vuoi.") }
        }
        .navigationTitle("Impostazioni").navigationBarTitleDisplayMode(.inline)
        .onAppear { demoEnabled = model.isDemoMode }
        .onChange(of: model.isDemoMode) { _, enabled in demoEnabled = enabled }
    }
    private func changeMode(_ enabled: Bool) {
        guard enabled != model.isDemoMode, !changingMode else { return }
        changingMode = true
        Task { await model.setDemoMode(enabled); demoEnabled = model.isDemoMode; changingMode = false }
    }
}
