import SwiftUI
import UniformTypeIdentifiers
import GaugeCore

struct FirmwarePreparationView: View {
    @State private var model: FirmwarePreparationModel
    @State private var showImporter = false
    @State private var importKind: OTAPreparationFileKind = .manifest
    init(trust: OTAPreparationTrust? = nil, installation: OTAInstallationController = .init()) {
        _model = State(initialValue: FirmwarePreparationModel(trust: trust, installation: installation))
    }
    var body: some View {
        Form {
            Section("Prepara l’aggiornamento") {
                Text("Scegli i file dell’aggiornamento e controllali prima di inviarli al dispositivo.")
                if !model.trustAvailable {
                    Label("Verifica non ancora disponibile", systemImage: "lock.shield")
                    Text("Puoi scegliere i file, ma per ora non è possibile verificarli né installarli su questo dispositivo.")
                        .font(.callout).foregroundStyle(.secondary)
                }
            }
            FirmwarePackageFilesSection(snapshot: model.snapshot, busy: model.preparationLocked, chooseManifest: chooseManifest, chooseImage: chooseImage)
            Section {
                Button("Verifica aggiornamento", systemImage: "checkmark.shield", action: model.verify).frame(minHeight: AppTheme.minimumTouch).disabled(!model.canVerify)
                Button("Conserva file verificati", systemImage: "square.and.arrow.down", action: model.save).frame(minHeight: AppTheme.minimumTouch).disabled(!model.canSave)
                if model.busy && model.installationSnapshot.state == .idle {
                    ProgressView("Controllo dei file in corso…")
                    Button("Interrompi controllo", role: .cancel, action: model.cancel)
                }
                if let notice = model.notice { Text(notice).font(.callout).accessibilityAddTraits(.updatesFrequently) }
            }
            if let manifest = model.snapshot.verifiedManifest { FirmwareVerifiedPackageSection(manifest: manifest) }
            FirmwareInstallationSection(model: model)
        }
        .navigationTitle("Aggiornamento")
        .fileImporter(isPresented: $showImporter, allowedContentTypes: [.data], allowsMultipleSelection: false, onCompletion: importCompleted)
        .task { await model.restore() }
        .onDisappear(perform: model.leave)
    }
    private func chooseManifest() { importKind = .manifest; showImporter = true }
    private func chooseImage() { importKind = .image; showImporter = true }
    private func importCompleted(_ result: Result<[URL], Error>) {
        switch result {
        case .success(let urls):
            guard let url = urls.first else { return }
            model.importFile(url, kind: importKind)
        case .failure(let error): model.importFailed(error)
        }
    }
}
