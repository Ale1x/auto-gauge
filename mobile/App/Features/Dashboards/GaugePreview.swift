import SwiftUI
import GaugeCore

struct GaugePreview: View {
    let dashboard: Dashboard
    var brand = "BMW"
    var interactive = false
    var selectedID: Int? = nil
    var onSelect: (Int) -> Void = { _ in }
    var onMove: (Int, Int, Int) -> Void = { _, _, _ in }
    var body: some View {
        GeometryReader { proxy in
            let size = min(proxy.size.width, proxy.size.height)
            ZStack {
                Circle().fill(.black)
                GaugeDialChrome(brand: brand, count: dashboard.widgets.count)
                ForEach(dashboard.widgets) { widget in
                    GaugePreviewWidget(widget: widget, primary: dashboard.widgets.count <= 3 && dashboard.widgets.first?.id == widget.id, interactive: interactive, alternate: dashboard.widgets.firstIndex(where: { $0.id == widget.id }) == (dashboard.widgets.count <= 3 ? 2 : 1), selected: selectedID == widget.id, canvasSize: size, onSelect: onSelect, onMove: onMove)
                        .position(x: size * (widget.frame.x + widget.frame.width / 2), y: size * (widget.frame.y + widget.frame.height / 2))
                }
            }.frame(width: size, height: size).clipShape(.circle)
        }
        .aspectRatio(1, contentMode: .fit).frame(maxWidth: AppTheme.previewMaximumWidth)
        .environment(\.colorScheme, .dark)
        // A scale drawing of the physical panel; surrounding controls support Dynamic Type.
        .dynamicTypeSize(.medium)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Anteprima della schermata")
    }
}
