import SwiftUI
import CloudKit

struct RootView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.scenePhase) private var scenePhase
    @State private var selection = AppTab.device
    @State private var devicePath: [DeviceRoute] = []
    @State private var dashboardPath: [DashboardRoute] = []
    @State private var historyPath: [HistoryRoute] = []
    @State private var debugHistoryRouteApplied = false
    @State private var debugFirmwareRouteApplied = false
    var body: some View {
        @Bindable var model = model
        TabView(selection: $selection) {
            Tab("Dispositivo", systemImage: "circle.hexagongrid", value: .device) { DeviceNavigationView(path: $devicePath) }
            Tab("Dashboard", systemImage: "rectangle.3.group", value: .dashboards) { DashboardNavigationView(path: $dashboardPath) }
            Tab("Storici", systemImage: "clock.arrow.circlepath", value: .histories) { HistoryNavigationView(path: $historyPath) }
            Tab("Account", systemImage: "person.crop.circle", value: .account) { NavigationStack { AccountView().modifier(SimulationBannerModifier()) } }
        }
        .onChange(of: scenePhase) { _, phase in if phase == .background { model.nativeBluetooth.stop() } }
        .tint(AppTheme.accent)
        .alert("Auto Gauge", isPresented: $model.alertPresented) {
            Button("Va bene", role: .cancel) { model.message = nil }
        } message: { Text(model.message ?? "") }
        .task {
            for await _ in NotificationCenter.default.notifications(named: .CKAccountChanged) { await model.cloud.accountChanged() }
        }
        .task {
            await model.bootstrap()
            #if DEBUG
            if ProcessInfo.processInfo.arguments.contains(where: { ["--history-timeline", "--history-timeline-chart", "--history-timeline-table"].contains($0) }), !debugHistoryRouteApplied {
                debugHistoryRouteApplied = true
                selection = .histories; historyPath = [.timeline(nil)]
            }
            if ProcessInfo.processInfo.arguments.contains("--firmware-preparation"), !debugFirmwareRouteApplied {
                debugFirmwareRouteApplied = true
                selection = .device; devicePath = [.firmware]
            }
            if ProcessInfo.processInfo.arguments.contains("--design-snapshots") {
                do { try DesignSnapshotRenderer.render(model: model) }
                catch { model.message = "Rendering diagnostico non riuscito: \(error.localizedDescription)" }
            }
            #endif
        }
    }
}
