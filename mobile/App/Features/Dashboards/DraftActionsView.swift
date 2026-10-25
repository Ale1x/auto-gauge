import SwiftUI

struct DraftActionsView: View {
    @Environment(AppModel.self) private var model
    @State private var showingRestoreConfirmation = false

    var body: some View {
        Section {
            if model.isSaving {
                ProgressView("Salvataggio…")
                    .frame(minHeight: AppTheme.minimumTouch)
            } else {
                Label(model.saveStatusText, systemImage: model.hasPendingChanges ? "pencil.circle" : "checkmark.circle")
                    .foregroundStyle(model.hasPendingChanges ? AppTheme.warning : AppTheme.accent)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Button(action: save) {
                Label("Salva modifiche", systemImage: "checkmark")
                    .frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch)
            }
            .buttonStyle(.borderedProminent)
            .accessibilityIdentifier("configuration.save")
            .disabled(!model.hasPendingChanges || model.isSaving)
            AppActionButton(title: "Annulla", symbol: "arrow.uturn.backward",
                            disabled: !model.editor.hasChanges || model.isSaving,
                            action: model.discardChanges)
            Menu("Altre opzioni", systemImage: "ellipsis.circle") {
                Button("Ripristina impostazioni iniziali", systemImage: "arrow.counterclockwise", role: .destructive,
                       action: requestRestore)
            }
            .frame(minHeight: AppTheme.minimumTouch)
            .disabled(model.isSaving)
            .confirmationDialog("Ripristinare le impostazioni iniziali?", isPresented: $showingRestoreConfirmation,
                                titleVisibility: .visible) {
                Button("Ripristina", role: .destructive, action: model.restoreDefaults)
                Button("Annulla", role: .cancel) { }
            } message: {
                Text("Le modifiche correnti a schermate e luminosità saranno sostituite. Premi Salva modifiche per confermare il nuovo risultato.")
            }
        } header: {
            Text("Le tue modifiche")
        } footer: {
            Text("Schermate e luminosità si salvano insieme. Annulla torna all’ultimo salvataggio.")
        }
    }

    private func save() { Task { await model.saveChanges() } }
    private func requestRestore() { showingRestoreConfirmation = true }
}
