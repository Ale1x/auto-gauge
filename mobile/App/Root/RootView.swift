import SwiftUI
import CloudKit

struct RootView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.scenePhase) private var scenePhase
    @State private var selection = AppTab.device
    @State private var devicePath: [DeviceRoute] = []
    @State private var dashboardPath: [DashboardRoute] = []
    @State private var historyPath: [HistoryRoute] = []
    @State private var pendingCloudAccountChange = false
    @State private var debugHistoryRouteApplied = false
    @State private var debugFirmwareRouteApplied = false
    var body: some View {
        @Bindable var model = model
        VStack(spacing: 0) {
            if model.isDemoMode { SimulationNotice().fixedSize(horizontal: false, vertical: true).layoutPriority(1) }
            TabView(selection: $selection) {
                Tab("Dispositivo", systemImage: "circle.hexagongrid", value: .device) { DeviceNavigationView(path: $devicePath) }
                Tab("Schermate", systemImage: "rectangle.3.group", value: .dashboards) { DashboardNavigationView(path: $dashboardPath) }
                Tab("Viaggi", systemImage: "clock.arrow.circlepath", value: .histories) { HistoryNavigationView(path: $historyPath) }
                Tab("Impostazioni", systemImage: "gearshape", value: .account) { NavigationStack { AccountView() } }
            }
            .frame(minHeight: 0, maxHeight: .infinity)
            .clipped()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .preferredColorScheme(model.appearance.colorScheme)
        .onChange(of: model.isDemoMode) { _, enabled in
            selection = .device; devicePath = []; dashboardPath = []; historyPath = []
            if !enabled && pendingCloudAccountChange {
                pendingCloudAccountChange = false
                Task {
                    if model.isDemoMode { pendingCloudAccountChange = true }
                    else { await model.cloud.accountChanged() }
                }
            }
        }
        .onChange(of: scenePhase) { _, phase in if phase == .background { model.nativeBluetooth.stop() } }
        .tint(AppTheme.accent)
        .alert("Auto Gauge", isPresented: $model.alertPresented) {
            Button("Va bene", role: .cancel) { model.message = nil }
        } message: { Text(model.message ?? "") }
        .task {
            for await _ in NotificationCenter.default.notifications(named: .CKAccountChanged) {
                if model.isDemoMode { pendingCloudAccountChange = true }
                else { await model.cloud.accountChanged() }
            }
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
