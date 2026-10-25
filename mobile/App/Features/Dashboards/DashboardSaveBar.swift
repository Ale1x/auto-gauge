import SwiftUI

struct DashboardSaveBar: View {
    @Environment(AppModel.self) private var model
    @State private var confirmingDiscard = false
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.spacing) {
            if model.isSaving { ProgressView("Salvataggio…") }
            else {
                Label(model.editor.hasChanges ? "Modifiche da salvare" : model.saveStatusText, systemImage: "pencil.circle").font(.headline)
                Button { Task { await model.saveChanges() } } label: {
                    Text("Salva modifiche").frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch)
                }
                .buttonStyle(.borderedProminent).foregroundStyle(AppTheme.canvas)
                .accessibilityIdentifier("configuration.save")
                if model.editor.hasChanges {
                    Button("Annulla modifiche") { confirmingDiscard = true }
                        .frame(minHeight: AppTheme.minimumTouch)
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(AppTheme.spacing)
        .background(AppTheme.panel, in: .rect(cornerRadius: AppTheme.cardRadius))
        .alert("Annullare le modifiche?", isPresented: $confirmingDiscard) {
            Button("Annulla modifiche", role: .destructive, action: model.discardChanges)
            Button("Continua a modificare", role: .cancel) { }
        } message: { Text("Tornerai all’ultimo salvataggio di schermate e luminosità.") }
    }
}
