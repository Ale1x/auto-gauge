import SwiftUI
import GaugeCore

struct ThemeMarketplaceView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dynamicTypeSize) private var typeSize
    @State private var collectionOnly = false
    @State private var category = "Tutti"
    private let categories = ["Tutti", "Sport", "Pixel", "Rétro", "Neon", "Analogici", "Minimal", "Strumenti"]
    private var filtered: [GaugeTheme] {
        let featured = ["original", "bmw_m", "pixel_turbo", "synth_sunset", "analog_heritage", "terminal_phosphor"]
        let catalog = featured.compactMap(GaugeThemeCatalog.find) + GaugeThemeCatalog.all.filter { !featured.contains($0.id) }
        let available = [model.themes.custom(demo: model.isDemoMode)].compactMap { $0 } + catalog
        return available.filter {
            (!collectionOnly || model.themes.owns($0, demo: model.isDemoMode)) &&
            (category == "Tutti" || $0.visualStyle.category == category)
        }
    }
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                VStack(alignment: .leading, spacing: 6) {
                    Text("Temi").font(.largeTitle.bold())
                    Text("Trova il tuo stile. Rendilo tuo.").font(.subheadline).foregroundStyle(.secondary)
                }
                HStack(spacing: 12) {
                    Picker("Catalogo", selection: $collectionOnly) {
                        Text("Scopri").tag(false)
                        Text("La tua raccolta").tag(true)
                    }.pickerStyle(.segmented).accessibilityIdentifier("themes.collection")
                    NavigationLink(value: ThemeRoute.search) {
                        Image(systemName: "magnifyingglass").font(.title3)
                            .frame(width: 48, height: 48)
                            .background(AppTheme.panel, in: .circle)
                    }
                    .accessibilityLabel("Cerca un tema").accessibilityIdentifier("themes.search")
                }
                if collectionOnly {
                    NavigationLink(value: ThemeRoute.customize) {
                        HStack(spacing: 14) {
                            Image(systemName: "paintbrush.pointed").font(.title2)
                            VStack(alignment: .leading, spacing: 4) {
                                Text("Crea il tuo tema").font(.headline)
                                Text("I tuoi colori. Sempre gratis.").font(.subheadline).foregroundStyle(.secondary)
                            }
                            Spacer(); Image(systemName: "chevron.right").font(.subheadline)
                        }
                        .padding(18).background(AppTheme.panel, in: .rect(cornerRadius: 22))
                    }.buttonStyle(.plain).accessibilityIdentifier("themes.create")
                } else {
                    ScrollView(.horizontal) {
                        HStack(spacing: 8) {
                            ForEach(categories, id: \.self) { item in
                                Button { category = item } label: {
                                    Text(item).font(.subheadline.weight(.semibold))
                                        .padding(.horizontal, 16).frame(minHeight: 44)
                                        .background(category == item ? AppTheme.accent : AppTheme.panel, in: .capsule)
                                        .foregroundStyle(category == item ? AppTheme.canvas : Color.primary)
                                }
                                .buttonStyle(.plain).accessibilityAddTraits(category == item ? [.isSelected] : [])
                                .accessibilityIdentifier("themes.category.\(item)")
                            }
                        }
                    }.scrollIndicators(.hidden)
                }
                HStack {
                    Text(collectionOnly ? "I tuoi quadranti" : category == "Tutti" ? "Da scoprire" : category).font(.headline)
                    Spacer()
                    Text(collectionOnly ? "\(filtered.count) temi" : "1,99 € a tema").font(.subheadline).foregroundStyle(.secondary)
                }
                if filtered.isEmpty {
                    ContentUnavailableView("Nessun tema in questa categoria", systemImage: "paintpalette", description: Text("Scegli un altro stile per continuare a esplorare."))
                } else {
                    LazyVGrid(columns: Array(repeating: GridItem(.flexible(), alignment: .top), count: typeSize.isAccessibilitySize ? 1 : 2), spacing: 16) {
                        ForEach(filtered) { ThemeCatalogCard(theme: $0) }
                    }
                }
                if collectionOnly {
                    Button("Ripristina acquisti", systemImage: "arrow.clockwise") {
                        Task { await model.themes.restore(demo: model.isDemoMode) }
                    }.frame(minHeight: 44).accessibilityIdentifier("themes.restore")
                }
                if let notice = model.themes.notice { Text(notice).font(.subheadline).foregroundStyle(.secondary) }
            }
            .padding(AppTheme.pageSpacing)
            .frame(maxWidth: AppTheme.readableWidth).frame(maxWidth: .infinity)
        }
        .background(AppTheme.canvas.ignoresSafeArea())
        .onChange(of: collectionOnly) { category = "Tutti" }
    }
}
