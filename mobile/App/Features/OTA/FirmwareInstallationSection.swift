import SwiftUI
import GaugeCore

struct FirmwareInstallationSection: View {
    let model: FirmwarePreparationModel
    @State private var confirmSelection = false
    var body: some View {
        Section("Installazione sul gauge") {
            if !model.installationAvailable {
                Label("Gauge non pronto per l’aggiornamento", systemImage: "lock")
                Text("Il trasferimento sarà disponibile dopo la configurazione di una connessione autorizzata e del profilo fidato del gauge.")
                    .font(.callout).foregroundStyle(.secondary)
            } else {
                Button("Trasferisci pacchetto verificato", systemImage: "arrow.up.circle", action: model.upload)
                    .disabled(!model.canUpload)
                if let manifest = model.installationSnapshot.manifest {
                    LabeledContent("Pacchetto dell’operazione", value: manifest.firmwareVersion)
                    ProgressView(value: Double(model.installationSnapshot.progress.imageAcknowledged), total: Double(max(1, manifest.imageBytes))) {
                        Text("Immagine confermata dal gauge")
                    } currentValueLabel: {
                        Text("\(model.installationSnapshot.progress.imageAcknowledged) di \(manifest.imageBytes) byte")
                    }
                }
                switch model.installationSnapshot.state {
                case .idle: EmptyView()
                case .opening: ProgressView("Preparazione della connessione autorizzata")
                case .uploading: Text("Trasferimento in corso")
                case .ready:
                    Label("Pronto per il prossimo avvio", systemImage: "checkmark.circle")
                    Button("Seleziona firmware per il prossimo avvio", systemImage: "power", action: askSelection)
                        .disabled(!model.canSelect)
                case .selecting: ProgressView("Conferma selezione in corso")
                case .selected:
                    Label("Selezione confermata", systemImage: "checkmark.circle")
                    Text("L’avvio e la salute del nuovo firmware richiedono ancora una verifica sul gauge.")
                case .unknown:
                    Label("Esito da verificare sul gauge", systemImage: "exclamationmark.triangle")
                    Text("Non ripetere l’operazione: lo stato deve essere riconciliato prima di un altro aggiornamento.")
                }
                if model.installationSnapshot.state == .opening || model.installationSnapshot.state == .uploading || model.installationSnapshot.state == .ready || model.installationSnapshot.state == .selecting {
                    Button("Interrompi e chiudi collegamento", role: .cancel, action: model.cancelInstallation)
                }
            }
        }
        .confirmationDialog("Selezionare questo firmware per il prossimo avvio?", isPresented: $confirmSelection, titleVisibility: .visible) {
            Button("Seleziona firmware", action: model.selectInstalledFirmware)
            Button("Mantieni il firmware attuale", role: .cancel) { }
        } message: {
            Text("Il trasferimento è terminato. Questa azione cambia separatamente la scelta del firmware per il prossimo avvio.")
        }
    }
    private func askSelection() { confirmSelection = true }
}
