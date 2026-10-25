import SwiftUI
import GaugeCore

struct FirmwarePackageFilesSection: View {
    let snapshot: OTAPreparationSnapshot
    let busy: Bool
    let chooseManifest: () -> Void
    let chooseImage: () -> Void

    var body: some View {
        Section {
            LabeledContent("Informazioni aggiornamento", value: snapshot.manifestFile?.name ?? "Da scegliere")
            Button("Scegli informazioni aggiornamento", systemImage: "doc.text", action: chooseManifest)
                .frame(minHeight: AppTheme.minimumTouch)
                .disabled(busy)
            LabeledContent("File aggiornamento", value: snapshot.imageFile?.name ?? "Da scegliere")
            Button("Scegli file aggiornamento", systemImage: "doc", action: chooseImage)
                .frame(minHeight: AppTheme.minimumTouch)
                .disabled(busy)
        } header: {
            Text("File necessari")
        } footer: {
            Text("Scegli entrambi i file forniti per lo stesso aggiornamento. Verranno controllati prima di poterli usare.")
        }
    }
}
