import SwiftUI

extension Color {
    init(gaugeHex: UInt32) {
        self.init(red: Double((gaugeHex >> 16) & 255) / 255,
                  green: Double((gaugeHex >> 8) & 255) / 255,
                  blue: Double(gaugeHex & 255) / 255)
    }
}
