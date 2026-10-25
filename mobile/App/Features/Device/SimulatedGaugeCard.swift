import SwiftUI

/// User-facing connection actions; transport details stay in the model.
struct SimulatedGaugeCard: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
            Label(model.gaugeConnected ? "Pronto per il viaggio" : "Collega il tuo dispositivo", systemImage: model.gaugeConnected ? "checkmark.circle" : "antenna.radiowaves.left.and.right")
                .font(.headline).foregroundStyle(AppTheme.accent)
            Text(model.gaugeConnected ? (model.hasPendingChanges ? "Salva le modifiche per aggiornare il display." : "Il display usa le tue schermate e le tue preferenze.") : "Accendi Auto Gauge e tieni il telefono vicino al dispositivo.")
                .font(.subheadline).foregroundStyle(.secondary)
            if model.gaugeBusy { ProgressView("Collegamento in corso…") }
            if model.gaugeConnected {
                Button("Scollega", systemImage: "link.badge.plus") { Task { await model.disconnectGauge() } }
                    .frame(minHeight: AppTheme.minimumTouch).disabled(model.gaugeBusy)
            } else if model.isDemoMode {
                Button("Collega", systemImage: "link") { Task { await model.connectGauge() } }
                    .frame(minHeight: AppTheme.minimumTouch).disabled(model.gaugeBusy)
            } else {
                NavigationLink(value: DeviceRoute.bluetoothDevelopment) { Label("Cerca dispositivo", systemImage: "magnifyingglass") }
                    .frame(minHeight: AppTheme.minimumTouch)
            }
        }.modifier(AppCardStyle())
    }
}
