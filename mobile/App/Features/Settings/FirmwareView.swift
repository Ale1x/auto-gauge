import SwiftUI

struct FirmwareView: View {
    @Environment(AppModel.self) private var model
    @State private var confirmInstall = false
    var body: some View {
        Group {
            if model.isDemoMode { demoUpdates }
            else if model.isNativeSession && model.firmwareInstallation.configured && model.firmwareTrust != nil {
                FirmwarePreparationView(trust: model.firmwareTrust, installation: model.firmwareInstallation)
            } else {
                ContentUnavailableView("Aggiornamenti non disponibili", systemImage: "arrow.down.circle", description: Text(model.isNativeSession ? "L’aggiornamento di questo dispositivo non è ancora disponibile." : "Collega Auto Gauge per controllare gli aggiornamenti."))
                    .navigationTitle("Aggiornamenti")
            }
        }
        .navigationTitle("Aggiornamenti")
        .navigationBarTitleDisplayMode(.inline)
    }
    private var demoUpdates: some View {
        Form {
            Section {
                Label("Software del dispositivo", systemImage: "circle.hexagongrid")
                LabeledContent("Versione", value: model.firmwareCurrentVersion)
                Text(model.firmwareStatusText).foregroundStyle(.secondary)
            }
            Section {
                switch model.demoFirmwareState {
                case .idle, .completed:
                    Button("Controlla aggiornamenti", systemImage: "arrow.clockwise") { Task { await model.checkFirmwareUpdate() } }
                        .accessibilityIdentifier("firmware.check")
                case .checking: ProgressView("Controllo in corso…")
                case .available:
                    if let version = model.firmwareAvailableVersion { LabeledContent("Nuova versione", value: version) }
                    Button("Scarica aggiornamento", systemImage: "arrow.down.circle") { Task { await model.downloadFirmwareUpdate() } }
                        .accessibilityIdentifier("firmware.download")
                case .downloading:
                    ProgressView(value: model.demoFirmwareProgress) { Text("Download in corso…") }
                    Button("Annulla", role: .cancel, action: model.cancelFirmwareUpdate)
                        .accessibilityIdentifier("firmware.cancel")
                case .downloaded:
                    Label("Aggiornamento pronto", systemImage: "checkmark.circle")
                    Button("Installa ora", systemImage: "arrow.down.circle") { confirmInstall = true }
                        .accessibilityIdentifier("firmware.install")
                        .confirmationDialog("Installare l’aggiornamento?", isPresented: $confirmInstall, titleVisibility: .visible) {
                            Button("Installa aggiornamento") { Task { await model.installFirmwareUpdate() } }
                            Button("Annulla", role: .cancel) { }
                        } message: { Text("Il display non sarà disponibile durante l’aggiornamento. Tieni il dispositivo acceso e il telefono vicino.") }
                case .installing:
                    ProgressView(value: model.demoFirmwareProgress) { Text("Installazione in corso…") }
                }
            }
            if model.demoFirmwareState == .available || model.demoFirmwareState == .downloaded {
                Section("Novità") {
                    Label("Miglioramenti alla stabilità", systemImage: "checkmark")
                    Label("Risposta più rapida ai tocchi", systemImage: "checkmark")
                }
            }
        }.navigationTitle("Aggiornamenti")
    }
}
