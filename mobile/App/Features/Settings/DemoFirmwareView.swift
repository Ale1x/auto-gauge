import SwiftUI

struct DemoFirmwareView: View {
    @Environment(AppModel.self) private var model
    @State private var confirmInstall = false
    private var headline: String {
        switch model.demoFirmwareState {
        case .completed: "Il display è aggiornato"
        case .available, .downloaded: "C’è una nuova versione"
        case .checking: "Controllo in corso"
        case .downloading: "Download in corso"
        case .installing: "Aggiornamento in corso"
        case .idle: "Il software del display"
        }
    }
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: AppTheme.pageSpacing) {
                VStack(alignment: .leading, spacing: AppTheme.spacing) {
                    Image(systemName: model.demoFirmwareState == .completed ? "checkmark.circle.fill" : "arrow.down.circle")
                        .font(.largeTitle).foregroundStyle(AppTheme.accent).accessibilityHidden(true)
                    Text(headline).font(.title2.bold())
                    Text(model.firmwareStatusText).font(.subheadline).foregroundStyle(.secondary)
                    LabeledContent("Versione installata", value: model.firmwareCurrentVersion).font(.subheadline)
                    if let version = model.firmwareAvailableVersion, model.demoFirmwareState == .available || model.demoFirmwareState == .downloaded {
                        LabeledContent("Nuova versione", value: version).font(.subheadline)
                    }
                    actions
                }.modifier(AppCardStyle())
                if model.demoFirmwareState == .available || model.demoFirmwareState == .downloaded {
                    VStack(alignment: .leading, spacing: AppTheme.spacing) {
                        Text("Cosa cambia").font(.headline)
                        Label("Maggiore stabilità", systemImage: "checkmark")
                        Label("Tocchi più rapidi", systemImage: "checkmark")
                    }.font(.subheadline).modifier(AppCardStyle())
                }
            }
            .padding(AppTheme.pageSpacing).frame(maxWidth: AppTheme.readableWidth).frame(maxWidth: .infinity)
        }
        .background(AppTheme.canvas.ignoresSafeArea())
        .alert("Installare l’aggiornamento?", isPresented: $confirmInstall) {
            Button("Installa aggiornamento") { Task { await model.installFirmwareUpdate() } }
            Button("Annulla", role: .cancel) { }
        } message: { Text("Tieni il dispositivo acceso e il telefono vicino fino al termine.") }
    }
    @ViewBuilder private var actions: some View {
        switch model.demoFirmwareState {
        case .idle, .completed:
            action("Controlla aggiornamenti", id: "firmware.check") { Task { await model.checkFirmwareUpdate() } }
        case .checking: ProgressView("Cerco aggiornamenti…")
        case .available:
            action("Scarica aggiornamento", id: "firmware.download") { Task { await model.downloadFirmwareUpdate() } }
        case .downloading:
            ProgressView(value: model.demoFirmwareProgress) { Text("Download in corso…") }
            Button("Annulla", role: .cancel, action: model.cancelFirmwareUpdate)
                .frame(minHeight: AppTheme.minimumTouch).accessibilityIdentifier("firmware.cancel")
        case .downloaded:
            action("Installa ora", id: "firmware.install") { confirmInstall = true }
        case .installing:
            ProgressView(value: model.demoFirmwareProgress) { Text("Installazione in corso…") }
        }
    }
    private func action(_ title: LocalizedStringResource, id: String, perform: @escaping () -> Void) -> some View {
        Button(action: perform) { Text(title).frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch) }
            .buttonStyle(.borderedProminent).foregroundStyle(AppTheme.canvas).accessibilityIdentifier(id)
    }
}
