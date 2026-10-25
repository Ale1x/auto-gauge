import SwiftUI
import GaugeCore

struct GaugeStyledChrome: View {
    let theme: GaugeTheme
    var body: some View {
        Canvas { context, size in
            context.scaleBy(x: size.width / 466, y: size.height / 466)
            let accent = Color(gaugeHex: theme.accent)
            let secondary = Color(gaugeHex: theme.secondary)
            func stroke(_ points: [CGPoint], _ color: Color, _ width: Double = 1) {
                guard let start = points.first else { return }
                var path = Path(); path.move(to: start)
                for point in points.dropFirst() { path.addLine(to: point) }
                context.stroke(path, with: .color(color), lineWidth: width)
            }
            switch theme.visualStyle {
            case .sport: break
            case .pixel, .arcade:
                for index in 0..<48 {
                    let a = Double(index) * .pi / 24
                    let x = 233 + cos(a) * 211, y = 233 + sin(a) * 211
                    let side: Double = theme.visualStyle == .pixel ? 9 : 13
                    context.fill(Path(CGRect(x: (x / side).rounded() * side, y: (y / side).rounded() * side, width: side - 2, height: side - 2)), with: .color(index.isMultiple(of: 2) ? accent : secondary.opacity(0.45)))
                }
                stroke([CGPoint(x: 130, y: 105), CGPoint(x: 336, y: 105)], accent, 3)
            case .terminal:
                for y in stride(from: 14, to: 456, by: 5) { stroke([CGPoint(x: 15, y: y), CGPoint(x: 451, y: y)], accent.opacity(0.09)) }
                context.stroke(Path(roundedRect: CGRect(x: 39, y: 40, width: 388, height: 386), cornerRadius: 38), with: .color(accent.opacity(0.5)), lineWidth: 2)
                context.draw(Text(">_").font(.system(size: 21, design: .monospaced)).foregroundStyle(accent), at: CGPoint(x: 97, y: 78))
            case .synthwave:
                for index in 0..<9 {
                    let y = Double(300 + index * index * 2)
                    stroke([CGPoint(x: 14, y: y), CGPoint(x: 452, y: y)], secondary.opacity(0.3))
                }
                for x in stride(from: -180, through: 700, by: 70) { stroke([CGPoint(x: 233, y: 287), CGPoint(x: x, y: 466)], accent.opacity(0.25)) }
                for i in 0..<6 { stroke([CGPoint(x: 188 + i * 3, y: 42 + i * 5), CGPoint(x: 278 - i * 3, y: 42 + i * 5)], accent.opacity(0.7), 3) }
                context.stroke(Path(ellipseIn: CGRect(x: 16, y: 16, width: 434, height: 434)), with: .color(accent), lineWidth: 2)
            case .analog:
                for i in 0..<60 {
                    let a = Double(i) * .pi / 30
                    let outer: Double = 214, inner: Double = i.isMultiple(of: 5) ? 194 : 205
                    stroke([CGPoint(x: 233 + cos(a) * inner, y: 233 + sin(a) * inner), CGPoint(x: 233 + cos(a) * outer, y: 233 + sin(a) * outer)], theme.previewForeground.opacity(i.isMultiple(of: 5) ? 0.7 : 0.25), i.isMultiple(of: 5) ? 3 : 1)
                }
                context.stroke(Path(ellipseIn: CGRect(x: 9, y: 9, width: 448, height: 448)), with: .color(accent.opacity(0.7)), lineWidth: 3)
                stroke([CGPoint(x: 135, y: 402), CGPoint(x: 230, y: 393), CGPoint(x: 324, y: 402)], accent, 2)
            case .minimal:
                context.stroke(Path(ellipseIn: CGRect(x: 18, y: 18, width: 430, height: 430)), with: .color(theme.previewForeground.opacity(0.18)), lineWidth: 1)
                context.fill(Path(ellipseIn: CGRect(x: 228, y: 101, width: 10, height: 10)), with: .color(accent))
            case .blueprint:
                for p in stride(from: 13, to: 466, by: 22) {
                    stroke([CGPoint(x: p, y: 0), CGPoint(x: p, y: 466)], accent.opacity(0.1))
                    stroke([CGPoint(x: 0, y: p), CGPoint(x: 466, y: p)], accent.opacity(0.1))
                }
                for r: Double in [198, 218] { context.stroke(Path(ellipseIn: CGRect(x: 233-r, y: 233-r, width: r*2, height: r*2)), with: .color(accent.opacity(0.55)), style: StrokeStyle(lineWidth: 1, dash: [5, 5])) }
            case .endurance:
                for i in 0..<24 {
                    let x = Double(76 + i * 13)
                    context.fill(Path(CGRect(x: x, y: 103, width: 9, height: 14)), with: .color(i < 17 ? accent : secondary.opacity(0.25)))
                }
                context.stroke(Path(roundedRect: CGRect(x: 38, y: 38, width: 390, height: 390), cornerRadius: 65), with: .color(secondary.opacity(0.5)), lineWidth: 4)
            case .retro:
                for (i, color) in [accent, secondary, accent.opacity(0.5)].enumerated() { context.fill(Path(CGRect(x: 61, y: 98 + i * 5, width: 344, height: 3)), with: .color(color)) }
                context.stroke(Path(ellipseIn: CGRect(x: 15, y: 15, width: 436, height: 436)), with: .color(accent.opacity(0.5)), lineWidth: 6)
                stroke([CGPoint(x: 108, y: 398), CGPoint(x: 358, y: 398)], accent.opacity(0.7), 4)
            }
            context.draw(Text(theme.dialTitle).font(theme.previewFont(size: theme.visualStyle == .minimal ? 23 : 29)).foregroundStyle(theme.previewForeground), at: CGPoint(x: 233, y: 76))
        }.accessibilityHidden(true)
    }
}
