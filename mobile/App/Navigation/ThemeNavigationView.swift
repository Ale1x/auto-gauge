import SwiftUI

struct ThemeNavigationView: View {
    @State private var path: [ThemeRoute] = []
    private var showingDetail: Bool {
        if case .detail = path.last { return true }
        return false
    }
    var body: some View {
        NavigationStack(path: $path) {
            ThemeMarketplaceView()
                .navigationDestination(for: ThemeRoute.self) { route in
                    switch route {
                    case .detail(let id): ThemeDetailView(themeID: id)
                    case .customize: ThemeColorEditorView()
                    case .search: ThemeSearchView()
                    }
                }
        }
        .toolbarVisibility(path.isEmpty ? .hidden : .visible, for: .navigationBar)
        .toolbarVisibility(showingDetail ? .hidden : .visible, for: .tabBar)
    }
}
