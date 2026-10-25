import SwiftUI
import GaugeCore

struct FirmwareVerifiedPackageSection: View {
    let manifest: OTAManifest
    var body: some View {
        Section {
            Label("Verificato offline", systemImage: "checkmark.shield")
            LabeledContent("Versione", value: manifest.firmwareVersion)
            LabeledContent("Hardware", value: manifest.hardwareID)
            LabeledContent("Release", value: String(manifest.releaseCounter))
            LabeledContent("Dimensione", value: "\(manifest.imageBytes) byte")
            LabeledContent("Chiave locale", value: manifest.signingKeyID)
            Text(manifest.imageSHA256).font(.caption.monospaced()).textSelection(.enabled)
                .accessibilityLabel("Impronta SHA 256: \(manifest.imageSHA256)")
        } header: { Text("Risultato della verifica") }
        footer: { Text("La verifica riguarda la copia locale. L’installazione e l’avvio sul gauge richiedono controlli separati.") }
    }
}
