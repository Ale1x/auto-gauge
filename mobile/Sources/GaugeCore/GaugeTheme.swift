import Foundation

public struct GaugeTheme: Codable, Hashable, Identifiable, Sendable {
    public let id: String
    public let title: String
    public let brand: String
    public let dialTitle: String
    public let summary: String
    public let accent: UInt32
    public let secondary: UInt32
    public let stripes: [UInt32]
    public let style: GaugeThemeStyle?
    public var visualStyle: GaugeThemeStyle { style ?? .sport }
    public init(id: String, title: String, brand: String, dialTitle: String, summary: String, accent: UInt32, secondary: UInt32, stripes: [UInt32], style: GaugeThemeStyle? = nil) {
        self.id = id; self.title = title; self.brand = brand; self.dialTitle = dialTitle; self.summary = summary
        self.accent = accent; self.secondary = secondary; self.stripes = stripes; self.style = style
    }
    public var isFree: Bool { id == "original" || id == "custom" }
    public var productID: String? { isFree ? nil : "it.ale1x.autogauge.theme.\(id)" }
}

public enum GaugeThemeCatalog {
    public static let original = GaugeTheme(id: "original", title: "Originale", brand: "Auto Gauge", dialTitle: "AUTO GAUGE", summary: "Essenziale, leggibile, tuo.", accent: 0x52d6ff, secondary: 0x9aaeff, stripes: [0x52d6ff, 0x8694a8, 0x25303e])
    public static let all: [GaugeTheme] = (try? load()) ?? [original]
    public static func find(_ id: String) -> GaugeTheme? { all.first { $0.id == id } }
    public static func load() throws -> [GaugeTheme] {
        guard let url = Bundle.module.url(forResource: "themes", withExtension: "json") else { throw CocoaError(.fileNoSuchFile) }
        let themes = try JSONDecoder().decode([GaugeTheme].self, from: Data(contentsOf: url))
        guard themes.first == original, Set(themes.map(\.id)).count == themes.count,
              themes.allSatisfy({ !$0.title.isEmpty && !$0.dialTitle.isEmpty && (1...3).contains($0.stripes.count) && $0.accent <= 0xffffff && $0.secondary <= 0xffffff && $0.stripes.allSatisfy { $0 <= 0xffffff } }),
              themes.allSatisfy({ $0.id.range(of: "^[a-z][a-z0-9_]{0,39}$", options: .regularExpression) != nil }) else { throw CocoaError(.coderReadCorrupt) }
        return themes
    }
}
