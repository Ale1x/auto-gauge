import SwiftUI
import GaugeCore

struct GaugePreviewWidget: View {
    let widget: GaugeWidget
    let selected: Bool
    let canvasSize: Double
    let onSelect: (Int) -> Void
    let onMove: (Int, Int, Int) -> Void
    @Environment(\.dynamicTypeSize) private var dynamicTypeSize
    @ScaledMetric(relativeTo: .title2) private var valueSize: Double = 24
    @State private var dragOrigin: EditingRectangle?
    var body: some View {
        Button(action: select) {
            VStack(spacing: 2) {
                if dynamicTypeSize.isAccessibilitySize {
                    Text("\(widget.id)").font(.title3).monospacedDigit()
                } else {
                    Text(exampleValue).font(.system(size: valueSize, design: .rounded)).monospacedDigit().minimumScaleFactor(0.5)
                    Text(widget.pid.unit).font(.footnote).foregroundStyle(AppTheme.accent)
                    Text(widget.pid.nameIT).font(.footnote).foregroundStyle(.secondary).lineLimit(1).minimumScaleFactor(0.5)
                }
            }
            .frame(width: canvasSize * widget.frame.width, height: canvasSize * widget.frame.height)
            .background(selected ? AppTheme.accent.opacity(0.12) : Color.clear)
            .overlay { Rectangle().stroke(selected ? AppTheme.accent : Color.clear, lineWidth: 2) }
            .overlay(alignment: .topTrailing) { if selected { Image(systemName: "checkmark.circle.fill").foregroundStyle(AppTheme.accent).accessibilityHidden(true) } }
            .frame(minWidth: AppTheme.minimumTouch, minHeight: AppTheme.minimumTouch)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .simultaneousGesture(DragGesture(minimumDistance: 8).onChanged(move).onEnded(endMove))
        .accessibilityLabel("\(widget.pid.nameIT), valore dimostrativo \(exampleValue) \(widget.pid.unit)")
        .accessibilityValue(selected ? "Misura selezionata" : "Non selezionata")
        .accessibilityHint("Puoi regolare posizione e dimensioni anche nei controlli sotto l’anteprima.")
        .accessibilityInputLabels([widget.pid.nameIT])
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
