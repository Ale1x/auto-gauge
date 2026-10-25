import SwiftUI

struct DashboardNavigationView: View {
    @Binding var path: [DashboardRoute]
    var body: some View {
        NavigationStack(path: $path) {
            DashboardListView().navigationDestination(for: DashboardRoute.self) { route in
                switch route { case .editor(let id): DashboardEditorView(dashboardID: id).toolbarVisibility(.visible, for: .navigationBar) }
            }
        }
        .toolbarVisibility(path.isEmpty ? .hidden : .visible, for: .navigationBar)
    }
}
