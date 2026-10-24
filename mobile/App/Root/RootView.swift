import SwiftUI
import CloudKit

struct RootView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.scenePhase) private var scenePhase
    @State private var selection = AppTab.device
    @State private var devicePath: [DeviceRoute] = []
    @State private var dashboardPath: [DashboardRoute] = []
    @State private var historyPath: [HistoryRoute] = []
    var body: some View {
        @Bindable var model = model
        Group {
            if #available(iOS 18.0, *) {
                TabView(selection: $selection) {
                    Tab("Dispositivo", systemImage: "circle.hexagongrid", value: .device) { DeviceNavigationView(path: $devicePath) }
                    Tab("Dashboard", systemImage: "rectangle.3.group", value: .dashboards) { DashboardNavigationView(path: $dashboardPath) }
                    Tab("Storici", systemImage: "clock.arrow.circlepath", value: .histories) { HistoryNavigationView(path: $historyPath) }
                    Tab("Account", systemImage: "person.crop.circle", value: .account) { NavigationStack { AccountView() } }
                }
            } else {
                TabView(selection: $selection) {
                    DeviceNavigationView(path: $devicePath).tabItem { Label("Dispositivo", systemImage: "circle.hexagongrid") }.tag(AppTab.device)
                    DashboardNavigationView(path: $dashboardPath).tabItem { Label("Dashboard", systemImage: "rectangle.3.group") }.tag(AppTab.dashboards)
                    HistoryNavigationView(path: $historyPath).tabItem { Label("Storici", systemImage: "clock.arrow.circlepath") }.tag(AppTab.histories)
                    NavigationStack { AccountView() }.tabItem { Label("Account", systemImage: "person.crop.circle") }.tag(AppTab.account)
                }
            }
        }
        .onChange(of: scenePhase) { _, phase in if phase == .background { model.nativeBluetooth.stop() } }
        .tint(AppTheme.accent)
        .safeAreaInset(edge: .top, spacing: 0) { SimulationNotice() }
        .alert("Auto Gauge", isPresented: $model.alertPresented) {
            Button("Va bene", role: .cancel) { model.message = nil }
        } message: { Text(model.message ?? "") }
        .task {
            for await _ in NotificationCenter.default.notifications(named: .CKAccountChanged) { await model.cloud.accountChanged() }
        }
        .task {
            await model.bootstrap()
            #if DEBUG
            if ProcessInfo.processInfo.arguments.contains("--design-snapshots") {
                do { try DesignSnapshotRenderer.render(model: model) }
                catch { model.message = "Rendering diagnostico non riuscito: \(error.localizedDescription)" }
            }
            #endif
        }
    }
}
