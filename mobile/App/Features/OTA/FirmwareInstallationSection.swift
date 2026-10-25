import SwiftUI
import GaugeCore

struct FirmwareInstallationSection: View {
    let model: FirmwarePreparationModel
    @State private var confirmSelection = false

    var body: some View {
        Section("3. Aggiorna il dispositivo") {
            if !model.installationAvailable {
                Label("Installazione non disponibile", systemImage: "lock")
                Text("L’app non è ancora configurata per aggiornare questo dispositivo. Nessuna installazione è stata avviata.")
                    .font(.callout).foregroundStyle(.secondary)
            } else {
                Button("Trasferisci aggiornamento", systemImage: "arrow.up.circle", action: model.upload)
                    .frame(minHeight: AppTheme.minimumTouch)
                    .disabled(!model.canUpload)
                if let manifest = model.installationSnapshot.manifest {
                    LabeledContent("Versione selezionata", value: manifest.firmwareVersion)
                    ProgressView(value: Double(model.installationSnapshot.progress.imageAcknowledged), total: Double(max(1, manifest.imageBytes))) {
                        Text("Trasferimento al dispositivo")
                    } currentValueLabel: {
                        Text(progressPercentage)
                    }
                }
                switch model.installationSnapshot.state {
                case .idle: EmptyView()
                case .opening: ProgressView("Collegamento al dispositivo…")
                case .uploading: Text("Invio dell’aggiornamento in corso…")
                case .ready:
                    Label("File trasferito e controllato", systemImage: "checkmark.circle")
                    Button("Usa al prossimo avvio", systemImage: "power", action: askSelection)
                        .frame(minHeight: AppTheme.minimumTouch)
                        .disabled(!model.canSelect)
                case .selecting: ProgressView("Conferma dell’aggiornamento…")
                case .selected:
                    Label("Versione scelta per il prossimo avvio", systemImage: "checkmark.circle")
                    Text("Il corretto avvio della nuova versione deve ancora essere confermato sul dispositivo.")
                case .unknown:
                    Label("Controlla lo stato del dispositivo", systemImage: "exclamationmark.triangle")
                    Text("L’esito non è ancora confermato. Non avviare un altro aggiornamento prima di controllare il dispositivo.")
                }
                if model.installationSnapshot.state == .opening || model.installationSnapshot.state == .uploading || model.installationSnapshot.state == .ready || model.installationSnapshot.state == .selecting {
                    Button("Interrompi aggiornamento", role: .cancel, action: model.cancelInstallation)
                        .frame(minHeight: AppTheme.minimumTouch)
                }
            }
        }
        .alert("Usare questa versione al prossimo avvio?", isPresented: $confirmSelection) {
            Button("Usa al prossimo avvio", action: model.selectInstalledFirmware)
            Button("Mantieni la versione attuale", role: .cancel) { }
        } message: {
            Text("Il file è stato trasferito e controllato. Conferma per scegliere la nuova versione al prossimo avvio del dispositivo.")
        }
    }

    private var progressPercentage: String {
        guard let manifest = model.installationSnapshot.manifest, manifest.imageBytes > 0 else { return "0%" }
        let progress = Double(model.installationSnapshot.progress.imageAcknowledged) / Double(manifest.imageBytes)
        return progress.formatted(.percent.precision(.fractionLength(0)))
    }
    private func askSelection() { confirmSelection = true }
}
