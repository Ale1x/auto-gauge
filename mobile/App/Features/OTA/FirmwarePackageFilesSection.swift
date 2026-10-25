import SwiftUI
import GaugeCore

struct FirmwarePackageFilesSection: View {
    let snapshot: OTAPreparationSnapshot
    let busy: Bool
    let chooseManifest: () -> Void
    let chooseImage: () -> Void
    var body: some View {
        Section {
            LabeledContent("Manifesto", value: snapshot.manifestFile?.name ?? "Non selezionato")
            if let file = snapshot.manifestFile { Text("\(file.byteCount) byte · limite 4 KiB").font(.caption).foregroundStyle(.secondary) }
            Button("Scegli manifesto", systemImage: "doc.text", action: chooseManifest).disabled(busy)
            LabeledContent("Immagine", value: snapshot.imageFile?.name ?? "Non selezionata")
            if let file = snapshot.imageFile { Text("\(file.byteCount) byte · limite 16 MiB").font(.caption).foregroundStyle(.secondary) }
            Button("Scegli immagine firmware", systemImage: "doc", action: chooseImage).disabled(busy)
        } header: { Text("File del pacchetto") }
        footer: { Text("Seleziona il manifesto firmato JSON e l’immagine firmware separata. Ogni file viene copiato sul telefono entro il limite indicato.") }
    }
}
