import SwiftUI

struct DeviceSummaryCard: View {
    @Environment(AppModel.self) private var model
    private var deviceName: some View {
        Label("Auto Gauge", systemImage: "circle.hexagongrid.fill")
            .font(.headline).foregroundStyle(AppTheme.accent)
            .fixedSize(horizontal: false, vertical: true)
    }
    private var connectionStatus: some View {
        Label(model.gaugeConnected ? "Collegato" : "Non collegato", systemImage: model.gaugeConnected ? "checkmark.circle.fill" : "circle")
            .font(.caption).foregroundStyle(.secondary)
            .fixedSize(horizontal: false, vertical: true)
    }
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.spacing) {
            ViewThatFits(in: .horizontal) {
                HStack { deviceName.fixedSize(); Spacer(); connectionStatus.fixedSize() }
                VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                    deviceName
                    connectionStatus
                }
            }
            if (model.isDemoMode || model.isNativeSession), let dashboard = model.configuration.dashboards.first {
                HStack { Spacer(); GaugePreview(dashboard: dashboard, brand: model.isDemoMode ? "BMW" : "AUTO GAUGE").allowsHitTesting(false).accessibilityHidden(true); Spacer() }
                Text("Anteprima del display").font(.subheadline).foregroundStyle(.secondary)
            }
            if model.isDemoMode || model.isNativeSession {
            NavigationLink(value: DeviceRoute.screens) {
                Label("Personalizza il display", systemImage: "slider.horizontal.3")
                    .frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch)
            }.buttonStyle(.borderedProminent)
            }
        }.modifier(AppCardStyle())
    }
}
