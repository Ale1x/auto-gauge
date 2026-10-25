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
            Section {
                Toggle("Backup iCloud", isOn: $model.backupEnabled)
                    .accessibilityIdentifier("settings.backup")
                    .disabled(!model.isDemoMode && !model.cloud.configured)
                Label(!model.isDemoMode && !model.cloud.configured ? "Il backup iCloud non è disponibile in questa versione. I viaggi restano sul telefono." : model.backupStatusText, systemImage: "icloud")
                    .font(.subheadline).foregroundStyle(.secondary)
                if let date = model.lastBackupDate {
                    LabeledContent("Ultimo backup") {
                        Text(date, format: .dateTime.day().month(.abbreviated).hour().minute())
                    }.font(.subheadline)
                }
                Button { synchronize() } label: {
                    Label("Sincronizza", systemImage: "arrow.triangle.2.circlepath.icloud")
                        .frame(minHeight: AppTheme.minimumTouch)
                }.disabled(!model.backupEnabled || model.backupBusy || (!model.isDemoMode && !model.cloud.configured))
                    .accessibilityIdentifier("backup.sync")
                Button { restore() } label: {
                    Label("Recupera viaggi", systemImage: "icloud.and.arrow.down")
                        .frame(minHeight: AppTheme.minimumTouch)
                }.disabled(!model.backupEnabled || model.backupBusy || (!model.isDemoMode && !model.cloud.configured))
                    .accessibilityIdentifier("backup.restore")
                if model.backupBusy { ProgressView("Aggiornamento del backup") }
            } header: { Text("I tuoi viaggi al sicuro") }
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

    private func synchronize() { Task { await model.synchronizeBackup() } }
    private func restore() { Task { await model.restoreBackup() } }
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
