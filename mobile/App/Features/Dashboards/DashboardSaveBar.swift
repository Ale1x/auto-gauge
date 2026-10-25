import SwiftUI

struct DashboardSaveBar: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        VStack(spacing: AppTheme.smallSpacing) {
            Label(model.isSaving ? "Salvataggio…" : model.hasPendingChanges ? "Hai modifiche da salvare" : model.saveStatusText,
                  systemImage: model.hasPendingChanges ? "pencil.circle" : "checkmark.circle")
                .font(.subheadline).foregroundStyle(.secondary)
            if model.editor.hasChanges {
                Button("Annulla modifiche", action: model.discardChanges)
                    .frame(minHeight: AppTheme.minimumTouch).disabled(model.isSaving)
            }
        }.frame(maxWidth: .infinity)
    }
}
