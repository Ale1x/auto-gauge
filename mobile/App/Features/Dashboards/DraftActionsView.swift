import SwiftUI

struct DraftActionsView: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        Section {
            Label(model.editor.hasChanges ? "Modifiche non salvate sul telefono" : "Bozza salvata sul telefono", systemImage: model.editor.hasChanges ? "pencil.circle" : "checkmark.circle")
                .foregroundStyle(model.editor.hasChanges ? AppTheme.warning : AppTheme.accent)
            LabeledContent("Revisione locale", value: "\(model.savedRevision)").monospacedDigit()
            AppActionButton(title: "Salva bozza sul telefono", symbol: "square.and.arrow.down", disabled: !model.editor.hasChanges, action: save)
            AppActionButton(title: "Annulla modifiche locali", symbol: "arrow.uturn.backward", disabled: !model.editor.hasChanges, action: model.cancelEdits)
            AppActionButton(title: "Ricarica bozza salvata", symbol: "arrow.clockwise", action: model.reloadSaved)
            AppActionButton(title: "Ripristina esempio nella bozza", symbol: "rectangle.stack.badge.plus", action: model.restoreExample)
        } header: { Text("Su questo telefono") } footer: {
            Text("Salva include dashboard e luminosità solo sul telefono. Annulla ripristina l’ultimo salvataggio. Nessuna di queste azioni applica la bozza al gauge.")
        }
        Section("Gauge simulato") { SimulatedGaugeCard() }
    }
    private func save() { Task { await model.save() } }
}
