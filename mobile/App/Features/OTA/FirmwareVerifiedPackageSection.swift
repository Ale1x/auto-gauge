import SwiftUI
import GaugeCore

struct FirmwareVerifiedPackageSection: View {
    let manifest: OTAManifest

    var body: some View {
        Section {
            Label("File verificati", systemImage: "checkmark.shield")
            LabeledContent("Versione", value: manifest.firmwareVersion)
        } header: {
            Text("Controllo completato")
        } footer: {
            Text("I file sono adatti al dispositivo. L’aggiornamento non è ancora stato installato.")
        }
    }
}
