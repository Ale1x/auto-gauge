import SwiftUI
import GaugeCore

struct FirmwarePackageFilesSection: View {
    let snapshot: OTAPreparationSnapshot
    let busy: Bool
    let chooseManifest: () -> Void
    let chooseImage: () -> Void
    var body: some View {
        Section {
            Button(action: chooseManifest) {
                Label {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Informazioni aggiornamento").foregroundStyle(Color.primary)
                        Text(snapshot.manifestFile?.name ?? "Scegli il file").font(.subheadline).foregroundStyle(Color.secondary)
                    }
                } icon: { Image(systemName: "doc.text") }
                .frame(minHeight: AppTheme.minimumTouch)
            }.disabled(busy)
            Button(action: chooseImage) {
                Label {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("File aggiornamento").foregroundStyle(Color.primary)
                        Text(snapshot.imageFile?.name ?? "Scegli il file").font(.subheadline).foregroundStyle(Color.secondary)
                    }
                } icon: { Image(systemName: "doc") }
                .frame(minHeight: AppTheme.minimumTouch)
            }.disabled(busy)
        } header: { Text("1. Scegli i file") }
          footer: { Text("Servono entrambi i file dello stesso aggiornamento.") }
    }
}
