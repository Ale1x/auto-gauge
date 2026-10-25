import SwiftUI
import StoreKit
import GaugeCore

struct ThemeDetailView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.purchase) private var purchase
    @Environment(\.dynamicTypeSize) private var typeSize
    @State private var confirming = false
    let themeID: String
    private var theme: GaugeTheme {
        (themeID == "custom" ? model.themes.custom(demo: model.isDemoMode) : GaugeThemeCatalog.find(themeID)) ?? GaugeThemeCatalog.original
    }
    private var owned: Bool { model.themes.owns(theme, demo: model.isDemoMode) }
    private var product: Product? { model.themes.store.products.first { $0.id == theme.productID } }
    private var price: String { product?.displayPrice ?? "1,99 €" }
    private var notice: String? {
        if model.themes.lastThemeID == theme.id { return model.themes.notice }
        if model.themes.store.lastProductID == theme.productID { return model.themes.store.notice }
        return nil
    }
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                VStack(alignment: .leading, spacing: 6) {
                    Text(theme.brand.uppercased()).font(.caption.weight(.semibold)).foregroundStyle(.secondary)
                    Text(theme.title).font(.title.bold())
                    Text(theme.summary).font(.subheadline).foregroundStyle(.secondary)
                }
                VStack(spacing: 14) {
                    GaugePreview(dashboard: model.configuration.dashboards[0], theme: theme)
                        .frame(maxWidth: 280).accessibilityIdentifier("theme.preview.\(theme.id)")
                    Text("Anteprima con le tue misure").font(.caption).foregroundStyle(.secondary)
                }
                .padding(20).frame(maxWidth: .infinity)
                .background(AppTheme.panel, in: .rect(cornerRadius: 28))
                VStack(alignment: .leading, spacing: 12) {
                    Label("Acquisto singolo, nessun abbonamento", systemImage: "checkmark.circle")
                    Label("Disposizione sempre personalizzabile", systemImage: "slider.horizontal.3")
                }.font(.subheadline).foregroundStyle(.secondary)
                if let notice {
                    Label(notice, systemImage: "checkmark.circle").font(.subheadline).foregroundStyle(AppTheme.accent)
                }
            }
            .padding(20).frame(maxWidth: AppTheme.readableWidth).frame(maxWidth: .infinity)
        }
        .background(AppTheme.canvas.ignoresSafeArea())
        .navigationTitle("Anteprima").navigationBarTitleDisplayMode(.inline)
        .safeAreaInset(edge: .bottom) {
            Group {
                if typeSize.isAccessibilitySize {
                    VStack(alignment: .leading, spacing: 12) { priceLabel; action }
                } else {
                    HStack(spacing: 20) { priceLabel; Spacer(minLength: 8); action }
                }
            }
            .padding(20).frame(maxWidth: .infinity)
            .background(AppTheme.canvas)
            .overlay(alignment: .top) { Divider() }
        }
    }
    private var priceLabel: some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(owned ? "Nella raccolta" : price).font(owned ? .headline : .title2.bold())
            Text(owned ? "Pronto da usare" : "Acquisto singolo").font(.caption).foregroundStyle(.secondary)
        }
    }
    private var action: some View {
        Group {
            if owned {
                Button(model.displayTheme.id == theme.id ? "Tema in uso" : "Usa tema") {
                    model.themes.choose(theme, demo: model.isDemoMode)
                }.disabled(model.displayTheme.id == theme.id).accessibilityIdentifier("theme.use")
            } else if model.isDemoMode {
                Button("Acquista tema") { confirming = true }
                    .accessibilityIdentifier("theme.buy")
                    .confirmationDialog("Aggiungere \(theme.title) alla raccolta?", isPresented: $confirming, titleVisibility: .visible) {
                        Button("Conferma acquisto") { model.themes.acquireDemo(theme) }
                        Button("Annulla", role: .cancel) { }
                    }
            } else if let product {
                Button("Acquista tema") {
                    Task { await model.themes.store.buy(product) { try await purchase($0) } }
                }.disabled(model.themes.store.busy).accessibilityIdentifier("theme.buy")
            } else {
                Button("In arrivo") { }.disabled(true).accessibilityLabel("Acquisti in arrivo")
            }
        }
        .buttonStyle(.borderedProminent).controlSize(.large)
        .foregroundStyle(AppTheme.canvas)
        .frame(minHeight: 48)
    }
}
