import SwiftUI
import GaugeCore

struct ThemeCatalogCard: View {
    @Environment(AppModel.self) private var model
    let theme: GaugeTheme
    private var price: String {
        if theme.isFree { return "Incluso" }
        if model.themes.owns(theme, demo: model.isDemoMode) { return "Acquistato" }
        return model.themes.store.products.first { $0.id == theme.productID }?.displayPrice ?? "1,99 €"
    }
    var body: some View {
        NavigationLink(value: ThemeRoute.detail(theme.id)) {
            VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                GaugePreview(dashboard: model.configuration.dashboards[0], theme: theme)
                    .accessibilityHidden(true)
                Text(theme.title).font(.headline).foregroundStyle(.primary).lineLimit(2)
                    .frame(minHeight: 42, alignment: .topLeading)
                Text(theme.visualStyle.category).font(.caption).foregroundStyle(.secondary)
                HStack {
                    Text(price).font(.subheadline.weight(.semibold)).foregroundStyle(.primary)
                    Spacer()
                    if model.displayTheme.id == theme.id { Image(systemName: "checkmark.circle.fill").foregroundStyle(AppTheme.accent) }
                }
            }
            .padding(16).frame(maxWidth: .infinity, alignment: .topLeading)
            .background(AppTheme.panel, in: .rect(cornerRadius: 22))
        }
        .buttonStyle(.plain)
        .accessibilityElement(children: .ignore)
        .accessibilityAddTraits(.isButton)
        .accessibilityLabel("\(theme.title), \(price)")
        .accessibilityHint("Apre l’anteprima del tema")
        .accessibilityIdentifier("theme.card.\(theme.id)")
    }
}
