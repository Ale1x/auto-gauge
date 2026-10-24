import SwiftUI

struct SimulatedGaugeCard: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
            Label(model.gaugeConnected ? "Simulatore collegato" : "Simulatore scollegato", systemImage: model.gaugeConnected ? "link.circle.fill" : "link.badge.plus")
                .font(.headline).foregroundStyle(AppTheme.accent)
            Text(model.gaugeNotice).font(.subheadline).fixedSize(horizontal: false, vertical: true)
            if let snapshot = model.gaugeSnapshot {
                LabeledContent("Ultima revisione letta", value: "\(snapshot.revision)").monospacedDigit()
                Label(model.gaugeNeedsRead ? "Stato da rileggere" : "Snapshot disponibile · RAM volatile", systemImage: model.gaugeNeedsRead ? "exclamationmark.arrow.triangle.2.circlepath" : "checkmark.circle")
                    .font(.subheadline).foregroundStyle(model.gaugeNeedsRead ? AppTheme.warning : AppTheme.accent)
            }
            Text(model.gaugeCommitState.titleIT).font(.subheadline).fixedSize(horizontal: false, vertical: true)
            if model.gaugeBusy { ProgressView("Operazione simulata in corso…") }
            AppActionButton(title: "Collega al gauge simulato", symbol: "link", disabled: model.gaugeConnected || model.gaugeBusy, action: connect)
            AppActionButton(title: "Leggi dal gauge simulato", symbol: "arrow.down.doc", disabled: !model.gaugeConnected || model.gaugeBusy, action: read)
            AppActionButton(title: "Usa lo snapshot nella bozza", symbol: "doc.on.doc", disabled: model.gaugeSnapshot == nil || model.gaugeNeedsRead || model.gaugeBusy, action: model.useGaugeSnapshot)
            AppActionButton(title: "Valida sul gauge simulato", symbol: "checkmark.shield", disabled: !model.gaugeConnected || model.gaugeBusy, action: validate)
            AppActionButton(title: "Applica al gauge simulato", symbol: "arrow.up.doc", disabled: !model.canApplyToGauge, action: apply)
            AppActionButton(title: "Scollega il simulatore", symbol: "link.badge.plus", disabled: !model.gaugeConnected || model.gaugeBusy, action: disconnect)
            Text("Solo trasporto in memoria. Ricezione, applicazione volatile e salvataggio sul telefono restano distinti. Nessuna radio o autenticazione reale.")
                .font(.footnote).foregroundStyle(.secondary)
        }.modifier(AppCardStyle())
    }
    private func connect() { Task { await model.connectGauge() } }
    private func read() { Task { await model.readGauge() } }
    private func validate() { Task { await model.validateOnGauge() } }
    private func apply() { Task { await model.applyToGauge() } }
    private func disconnect() { Task { await model.disconnectGauge() } }
}
