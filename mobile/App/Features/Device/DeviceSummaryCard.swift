import SwiftUI

struct DeviceSummaryCard: View {
    @Environment(AppModel.self) private var model
    private var hasDisplay: Bool { model.isDemoMode || model.isNativeSession }
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.spacing) {
            HStack(alignment: .top) {
                Label(model.gaugeConnected ? "Collegato" : "Da collegare", systemImage: model.gaugeConnected ? "checkmark.circle.fill" : "link")
                    .font(.headline).foregroundStyle(model.gaugeConnected ? AppTheme.accent : .primary)
                Spacer()
                if model.gaugeConnected {
                    Menu {
                        Button("Scollega dispositivo", systemImage: "link.badge.plus") { Task { await model.disconnectGauge() } }
                    } label: {
                        Label("Connessione", systemImage: "ellipsis").labelStyle(.iconOnly)
                            .frame(minWidth: AppTheme.minimumTouch, minHeight: AppTheme.minimumTouch).contentShape(.rect)
                    }.disabled(model.gaugeBusy)
                }
            }
            if hasDisplay, let dashboard = model.configuration.dashboards.first {
                GaugePreview(dashboard: dashboard, theme: model.displayTheme)
                    .frame(maxWidth: 220).frame(maxWidth: .infinity).accessibilityHidden(true)
                HStack {
                    Text("Anteprima · \(model.displayTheme.title)").font(.subheadline)
                    Spacer()
                    Text(model.configuration.dashboards.count == 1 ? "1 schermata" : "\(model.configuration.dashboards.count) schermate")
                        .font(.subheadline).foregroundStyle(.secondary)
                }
                if model.hasPendingChanges {
                    Label("Hai modifiche da salvare", systemImage: "pencil.circle").font(.subheadline).foregroundStyle(.secondary)
                }
            } else {
                Text("Accendi Auto Gauge e tienilo vicino al telefono. Ti aiuteremo a collegarlo.")
                    .font(.subheadline).foregroundStyle(.secondary)
            }
            if model.gaugeBusy { ProgressView("Collegamento in corso…") }
            if !model.gaugeConnected && model.isDemoMode {
                Button("Ricollega", systemImage: "link") { Task { await model.connectGauge() } }
                    .buttonStyle(.borderedProminent).foregroundStyle(AppTheme.canvas)
                    .frame(minHeight: AppTheme.minimumTouch).disabled(model.gaugeBusy)
            } else if !hasDisplay {
                NavigationLink(value: DeviceRoute.bluetoothDevelopment) {
                    Label("Cerca dispositivo", systemImage: "magnifyingglass").frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch)
                }.buttonStyle(.borderedProminent).foregroundStyle(AppTheme.canvas)
            } else {
                NavigationLink(value: DeviceRoute.screens) {
                    Text("Personalizza schermate").frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch)
                }.buttonStyle(.borderedProminent).foregroundStyle(AppTheme.canvas)
            }
        }.modifier(AppCardStyle())
    }
}
