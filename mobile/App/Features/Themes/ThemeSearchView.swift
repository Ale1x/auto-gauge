import SwiftUI
import GaugeCore

struct ThemeSearchView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dynamicTypeSize) private var typeSize
    @State private var query = ""
    private var results: [GaugeTheme] {
        let themes = [model.themes.custom(demo: model.isDemoMode)].compactMap { $0 } + GaugeThemeCatalog.all
        return themes.filter { query.isEmpty || $0.title.localizedStandardContains(query) || $0.brand.localizedStandardContains(query) || $0.visualStyle.category.localizedStandardContains(query) }
    }
    var body: some View {
        ScrollView {
            if results.isEmpty {
                ContentUnavailableView.search(text: query)
            } else {
                LazyVGrid(columns: Array(repeating: GridItem(.flexible(), alignment: .top), count: typeSize.isAccessibilitySize ? 1 : 2), spacing: 16) {
                    ForEach(results) { ThemeCatalogCard(theme: $0) }
                }.padding(24).frame(maxWidth: AppTheme.readableWidth).frame(maxWidth: .infinity)
            }
        }
        .background(AppTheme.canvas.ignoresSafeArea())
        .navigationTitle("Cerca un tema").navigationBarTitleDisplayMode(.inline)
        .searchable(text: $query, placement: .navigationBarDrawer(displayMode: .always), prompt: "Nome, marca o stile")
    }
}
