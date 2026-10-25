import SwiftUI
import GaugeCore

struct GaugePreviewWidget: View {
    let widget: GaugeWidget
    let primary: Bool
    let interactive: Bool
    let alternate: Bool
    let selected: Bool
    let canvasSize: Double
    let onSelect: (Int) -> Void
    let onMove: (Int, Int, Int) -> Void
    @State private var dragOrigin: EditingRectangle?
    private var scale: Double { canvasSize / 466 }
    private var tint: Color { alternate ? Color(red: 154/255, green: 174/255, blue: 1) : Color(red: 82/255, green: 214/255, blue: 1) }
    private var reading: some View {
        VStack(spacing: 3 * scale) {
            Text(primary ? widget.pid.nameIT.uppercased() : "\(widget.pid.nameIT.uppercased())  \(widget.pid.unit)")
                .font(.custom("Montserrat-Medium", fixedSize: (primary ? 18 : 14) * scale))
                .foregroundStyle(tint).lineLimit(1).minimumScaleFactor(0.6)
            Text(exampleValue)
                .font(.custom("Montserrat-Medium", fixedSize: (primary ? 80 : 32) * scale))
                .foregroundStyle(Color(red: 242/255, green: 245/255, blue: 250/255))
                .monospacedDigit().lineLimit(1).minimumScaleFactor(0.5)
            if primary {
                Text(widget.pid.unit)
                    .font(.custom("Montserrat-Medium", fixedSize: 18 * scale))
                    .foregroundStyle(Color(red: 134/255, green: 148/255, blue: 168/255))
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
