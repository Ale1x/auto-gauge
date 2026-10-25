import Foundation

public enum GaugeThemeStyle: String, Codable, CaseIterable, Sendable {
    case sport, pixel, arcade, terminal, synthwave, analog, minimal, blueprint, endurance, retro
    public var category: String {
        switch self {
        case .sport: "Sport"
        case .pixel, .arcade: "Pixel"
        case .terminal, .retro: "Rétro"
        case .synthwave: "Neon"
        case .analog: "Analogici"
        case .minimal: "Minimal"
        case .blueprint, .endurance: "Strumenti"
        }
    }
}
