import SwiftUI
import GaugeCore

extension GaugeTheme {
    var previewBackground: Color {
        let hex: UInt32
        switch visualStyle {
        case .sport: hex = 0x000000
        case .pixel: hex = 0x101a28
        case .arcade: hex = 0x190f2d
        case .terminal: hex = 0x06120e
        case .synthwave: hex = 0x1d1030
        case .analog: hex = ["analog_heritage", "analog_racing"].contains(id) ? 0xf3ebd8 : id == "analog_british" ? 0x102a24 : 0x142336
        case .minimal: hex = id == "minimal_sand" ? 0xe9dfcf : ["minimal_chalk", "minimal_cloud"].contains(id) ? 0xf3f2ed : 0x14181e
        case .blueprint: hex = 0x102b45
        case .endurance: hex = 0x12191f
        case .retro: hex = id == "retro_burgundy" ? 0x351c28 : 0x172b30
        }
        return Color(gaugeHex: hex)
    }
    var previewForeground: Color {
        let light = ["analog_heritage", "analog_racing", "minimal_chalk", "minimal_sand", "minimal_cloud"].contains(id)
        return Color(gaugeHex: light ? 0x252d32 : 0xf2f5fa)
    }
    func previewFont(size: Double, number: Bool = false) -> Font {
        switch visualStyle {
        case .sport: .custom("Montserrat-Medium", fixedSize: size)
        case .pixel, .arcade, .terminal, .blueprint: .system(size: size, weight: number ? .medium : .semibold, design: .monospaced)
        case .analog: .system(size: size, weight: .medium, design: .serif)
        case .minimal: .system(size: size, weight: number ? .light : .regular)
        case .synthwave, .endurance: .system(size: size, weight: .bold, design: .rounded)
        case .retro: .system(size: size, weight: .medium, design: .monospaced)
        }
    }
}
