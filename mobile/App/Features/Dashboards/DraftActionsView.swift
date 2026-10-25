import SwiftUI

struct DraftActionsView: View {
    @Environment(AppModel.self) private var model
    @State private var showingConfirmation = false
    @State private var restoring = false
    var body: some View {
        Section {
            if model.isSaving {
                ProgressView("Salvataggio…").frame(minHeight: AppTheme.minimumTouch)
            } else {
                Label(model.editor.hasChanges ? "Modifiche da salvare" : model.saveStatusText,
                      systemImage: model.hasPendingChanges ? "pencil.circle" : "checkmark.circle")
                    .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }
            if model.hasPendingChanges {
                Button { Task { await model.saveChanges() } } label: {
                    Text("Salva modifiche").frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch)
                }
                .buttonStyle(.borderedProminent).foregroundStyle(AppTheme.canvas)
                .accessibilityIdentifier("configuration.save").disabled(model.isSaving)
            }
            if model.editor.hasChanges {
                Button("Annulla modifiche", systemImage: "arrow.uturn.backward") { restoring = false; showingConfirmation = true }
                    .frame(minHeight: AppTheme.minimumTouch).disabled(model.isSaving)
            }
            Menu("Altre opzioni", systemImage: "ellipsis.circle") {
                Button("Ripristina impostazioni iniziali", systemImage: "arrow.counterclockwise", role: .destructive) { restoring = true; showingConfirmation = true }
            }.frame(minHeight: AppTheme.minimumTouch).disabled(model.isSaving)
        } header: { Text("Salvataggio") }
          footer: { Text("Schermate e luminosità si salvano insieme.") }
        .alert(restoring ? "Ripristinare le impostazioni iniziali?" : "Annullare le modifiche?", isPresented: $showingConfirmation) {
            if restoring {
                Button("Ripristina", role: .destructive, action: model.restoreDefaults)
            } else {
                Button("Annulla modifiche", role: .destructive, action: model.discardChanges).accessibilityIdentifier("configuration.discard.confirm")
            }
            Button("Continua a modificare", role: .cancel) { }
        } message: {
            Text(restoring ? "Schermate e luminosità saranno sostituite. Salva per confermare il risultato." : "Tornerai all’ultimo salvataggio di schermate e luminosità.")
        }
    }
}
