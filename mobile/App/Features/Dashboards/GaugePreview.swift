import SwiftUI
import GaugeCore

struct GaugePreview: View {
    let dashboard: Dashboard
    var selectedID: Int? = nil
    var onSelect: (Int) -> Void = { _ in }
    var onMove: (Int, Int, Int) -> Void = { _, _, _ in }
    var body: some View {
        GeometryReader { proxy in
            let size = min(proxy.size.width, proxy.size.height)
            ZStack {
                Circle().fill(.black).stroke(AppTheme.accent.opacity(0.4), lineWidth: 2)
                ForEach(dashboard.widgets) { widget in
                    GaugePreviewWidget(widget: widget, selected: selectedID == widget.id, canvasSize: size, onSelect: onSelect, onMove: onMove)
                        .position(x: size * (widget.frame.x + widget.frame.width / 2), y: size * (widget.frame.y + widget.frame.height / 2))
                }
            }.frame(width: size, height: size).clipShape(.circle)
        }
        .aspectRatio(1, contentMode: .fit).frame(maxWidth: AppTheme.previewMaximumWidth)
        .environment(\.colorScheme, .dark)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Anteprima della schermata")
    }
}
