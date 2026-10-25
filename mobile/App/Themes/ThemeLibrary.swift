import Foundation
import Observation
import GaugeCore

@MainActor @Observable
final class ThemeLibrary {
    let store = ThemeStoreClient()
    private let preferences: UserDefaults
    private(set) var demoOwned: Set<String>
    private(set) var customDemo: GaugeTheme?
    private(set) var customDevice: GaugeTheme?
    private(set) var demoSelection: String
    private(set) var deviceSelection: String
    private(set) var lastThemeID: String?
    private(set) var notice: String?
    init(preferences: UserDefaults) {
        self.preferences = preferences
        let known = Set(GaugeThemeCatalog.all.map(\.id))
        demoOwned = Set(preferences.stringArray(forKey: "demo.themes.owned") ?? []).intersection(known).union(["original"])
        customDemo = Self.readCustom(preferences.data(forKey: "demo.themes.custom"))
        customDevice = Self.readCustom(preferences.data(forKey: "device.themes.custom"))
        demoSelection = preferences.string(forKey: "demo.themes.selected") ?? "original"
        deviceSelection = preferences.string(forKey: "device.themes.selected") ?? "original"
    }
    func custom(demo: Bool) -> GaugeTheme? { demo ? customDemo : customDevice }
    func saveCustom(accent: UInt32, secondary: UInt32, demo: Bool) {
        guard accent <= 0xffffff, secondary <= 0xffffff else { return }
        let theme = GaugeTheme(id: "custom", title: "Il mio tema", brand: "Personale", dialTitle: "AUTO GAUGE", summary: "I tuoi colori, il tuo display.", accent: accent, secondary: secondary, stripes: [accent, secondary, 0xf2f5fa])
        guard let data = try? JSONEncoder().encode(theme) else { return }
        preferences.set(data, forKey: demo ? "demo.themes.custom" : "device.themes.custom")
        if demo { customDemo = theme } else { customDevice = theme }
        choose(theme, demo: demo)
    }
    private static func readCustom(_ data: Data?) -> GaugeTheme? {
        guard let data, data.count <= 2048, let theme = try? JSONDecoder().decode(GaugeTheme.self, from: data),
              theme.id == "custom", theme.accent <= 0xffffff, theme.secondary <= 0xffffff,
              theme.title.count <= 80, theme.dialTitle.count <= 24, (1...3).contains(theme.stripes.count), theme.stripes.allSatisfy({ $0 <= 0xffffff }) else { return nil }
        return theme
    }
    func owns(_ theme: GaugeTheme, demo: Bool) -> Bool {
        theme.isFree || (demo ? demoOwned.contains(theme.id) : theme.productID.map { store.owned.contains($0) } ?? false)
    }
    func selected(demo: Bool) -> GaugeTheme {
        let id = demo ? demoSelection : deviceSelection
        let theme = (id == "custom" ? custom(demo: demo) : GaugeThemeCatalog.find(id)) ?? GaugeThemeCatalog.original
        return owns(theme, demo: demo) ? theme : GaugeThemeCatalog.original
    }
    func choose(_ theme: GaugeTheme, demo: Bool) {
        guard owns(theme, demo: demo) else { return }
        if demo { demoSelection = theme.id } else { deviceSelection = theme.id }
        preferences.set(theme.id, forKey: demo ? "demo.themes.selected" : "device.themes.selected")
        lastThemeID = theme.id; notice = "Tema selezionato"
    }
    func acquireDemo(_ theme: GaugeTheme) {
        demoOwned.insert(theme.id); preferences.set(demoOwned.sorted(), forKey: "demo.themes.owned")
        lastThemeID = theme.id; notice = "Tema aggiunto alla raccolta"
    }
    func restore(demo: Bool) async {
        lastThemeID = nil
        if demo { notice = "La tua raccolta è aggiornata" }
        else { await store.restore(); notice = store.notice }
    }
    func start(demo: Bool) async {
        if demo { store.stop() }
        else { await store.start(ids: GaugeThemeCatalog.all.compactMap(\.productID)) }
    }
}
