import SwiftUI

struct DashboardNavigationView: View {
    @Binding var path: [DashboardRoute]
    var body: some View {
        NavigationStack(path: $path) {
            DashboardListView().modifier(SimulationBannerModifier()).navigationDestination(for: DashboardRoute.self) { route in
                switch route { case .editor(let id): DashboardEditorView(dashboardID: id).modifier(SimulationBannerModifier()) }
            }
        }
    }
}
