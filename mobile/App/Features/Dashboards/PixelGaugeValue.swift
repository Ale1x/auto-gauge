import SwiftUI

/// Bitmap digits drawn on a fixed 5×7 grid; no font smoothing or raster assets.
struct PixelGaugeValue: View {
    let value: String
    let color: Color
    private static let glyphs: [Character: [String]] = [
        "0": ["01110","11011","11011","11011","11011","11011","01110"],
        "1": ["00100","01100","00100","00100","00100","00100","01110"],
        "2": ["01110","11011","00011","00110","01100","11000","11111"],
        "3": ["11110","00011","00011","01110","00011","00011","11110"],
        "4": ["00011","00111","01111","11011","11111","00011","00011"],
        "5": ["11111","11000","11000","11110","00011","00011","11110"],
        "6": ["01110","11000","11000","11110","11011","11011","01110"],
        "7": ["11111","00011","00110","00110","01100","01100","01100"],
        "8": ["01110","11011","11011","01110","11011","11011","01110"],
        "9": ["01110","11011","11011","01111","00011","00011","01110"],
        ".": ["00000","00000","00000","00000","00000","00110","00110"],
        "—": ["00000","00000","00000","11111","00000","00000","00000"]
    ]
    var body: some View {
        Canvas { context, size in
            let columns = Double(max(1, value.count * 6 - 1))
            let cell = min(size.width / columns, size.height / 7)
            let offsetX = (size.width - columns * cell) / 2
            let offsetY = (size.height - 7 * cell) / 2
            var pixels = Path()
            for (index, character) in value.enumerated() {
                for (row, bits) in (Self.glyphs[character] ?? Self.glyphs["—"]!).enumerated() {
                    for (column, bit) in bits.enumerated() where bit == "1" {
                        pixels.addRect(CGRect(x: offsetX + Double(index * 6 + column) * cell, y: offsetY + Double(row) * cell, width: cell * 0.88, height: cell * 0.88))
                    }
                }
            }
            context.fill(pixels, with: .color(color))
        }.accessibilityHidden(true)
    }
}
