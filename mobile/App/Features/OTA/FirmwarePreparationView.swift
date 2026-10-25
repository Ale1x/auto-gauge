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
            Section("Preparazione locale") {
                Text("Prepara e controlla un pacchetto firmware prima del trasferimento al gauge.")
                if !model.trustAvailable {
                    Label("Profilo fidato non configurato", systemImage: "lock.shield")
                    Text("Puoi selezionare i file. La verifica sarà disponibile quando l’app avrà le informazioni fidate del gauge. La selezione dei file non autorizza l’installazione.")
                        .font(.callout).foregroundStyle(.secondary)
                }
            }
            FirmwarePackageFilesSection(snapshot: model.snapshot, busy: model.preparationLocked, chooseManifest: chooseManifest, chooseImage: chooseImage)
            Section {
                Button("Verifica pacchetto", systemImage: "checkmark.shield", action: model.verify).disabled(!model.canVerify)
                Button("Salva preparazione verificata", systemImage: "square.and.arrow.down", action: model.save).disabled(!model.canSave)
                if model.busy && model.installationSnapshot.state == .idle {
                    ProgressView("Preparazione in corso")
                    Button("Annulla operazione", role: .cancel, action: model.cancel)
                }
                if let notice = model.notice { Text(notice).font(.callout).accessibilityAddTraits(.updatesFrequently) }
            }
            if let manifest = model.snapshot.verifiedManifest { FirmwareVerifiedPackageSection(manifest: manifest) }
            FirmwareInstallationSection(model: model)
        }
        .navigationTitle("Firmware")
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
