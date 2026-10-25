import SwiftUI
import GaugeCore

struct GaugePreviewWidget: View {
    let widget: GaugeWidget
    let theme: GaugeTheme
    let primary: Bool
    let interactive: Bool
    let alternate: Bool
    let selected: Bool
    let canvasSize: Double
    let onSelect: (Int) -> Void
    let onMove: (Int, Int, Int) -> Void
    @State private var dragOrigin: EditingRectangle?
    private var scale: Double { canvasSize / 466 }
    private var tint: Color { Color(gaugeHex: alternate ? theme.secondary : theme.accent) }
    private var reading: some View {
        VStack(spacing: 3 * scale) {
            Text(primary ? widget.pid.nameIT.uppercased() : "\(widget.pid.nameIT.uppercased())  \(widget.pid.unit)")
                .font(theme.previewFont(size: (primary ? 18 : 14) * scale))
                .foregroundStyle(tint).lineLimit(1).minimumScaleFactor(0.6)
            if theme.visualStyle == .pixel || theme.visualStyle == .arcade {
                PixelGaugeValue(value: exampleValue, color: tint)
                    .frame(height: (primary ? 65 : 27) * scale)
                    .padding(.vertical, 5 * scale)
            } else {
                Text(exampleValue)
                    .font(theme.previewFont(size: (primary ? 80 : 32) * scale, number: true))
                    .foregroundStyle(theme.visualStyle == .terminal ? tint : theme.previewForeground)
                    .monospacedDigit().lineLimit(1).minimumScaleFactor(0.5)
            }
            if primary {
                Text(widget.pid.unit)
                    .font(theme.previewFont(size: 18 * scale))
                    .foregroundStyle(theme.previewForeground.opacity(0.6))
            }
        }
        .frame(width: canvasSize * widget.frame.width, height: canvasSize * widget.frame.height)
        .background(selected ? tint.opacity(0.1) : .clear)
        .overlay { RoundedRectangle(cornerRadius: 4).stroke(selected ? tint : .clear, lineWidth: 1) }
    }
    var body: some View {
        Group {
            if interactive {
                Button(action: select) { reading.frame(minWidth: AppTheme.minimumTouch, minHeight: AppTheme.minimumTouch).contentShape(.rect) }
                    .buttonStyle(.plain)
                    .simultaneousGesture(DragGesture(minimumDistance: 8).onChanged(move).onEnded(endMove))
            } else { reading }
        }
        .accessibilityLabel("\(widget.pid.nameIT), \(exampleValue) \(widget.pid.unit)")
        .accessibilityValue(selected ? "Misura selezionata" : "Non selezionata")
        .accessibilityHint(interactive ? "Puoi regolare posizione e dimensioni anche nei controlli sotto l’anteprima." : "Anteprima del display")
    }
    private var exampleValue: String {
        GaugePreviewValue.value(for: widget.pid.pid)
    }
    private func select() { onSelect(widget.id) }
    private func move(_ value: DragGesture.Value) {
        if dragOrigin == nil { dragOrigin = try? EditingRectangle(widget.frame) }
        guard let origin = dragOrigin, canvasSize > 0 else { return }
        onMove(widget.id, origin.x + Int(value.translation.width / canvasSize * 10000), origin.y + Int(value.translation.height / canvasSize * 10000))
    }
    private func endMove(_ value: DragGesture.Value) { dragOrigin = nil }
}
