import SwiftUI
import GaugeCore

struct GeometryControlView: View {
    @Environment(AppModel.self) private var model
    let field: GeometryField
    let rectangle: EditingRectangle
    let widgetID: Int
    let dashboardID: Int

    var body: some View {
        @Bindable var model = model
        let bounds = rectangle.range(for: field)
        VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
            LabeledContent(title, value: percentage).monospacedDigit()
            if bounds.lowerBound < bounds.upperBound {
                Slider(value: $model[geometryValueFor: dashboardID, widget: widgetID, field: field],
                       in: Double(bounds.lowerBound)...Double(bounds.upperBound), step: 100)
                    .frame(minHeight: AppTheme.minimumTouch)
                    .accessibilityLabel(title)
                    .accessibilityValue(percentage)
                HStack {
                    Text(lowerLabel)
                    Spacer()
                    Text(upperLabel)
                }
                .font(.footnote).foregroundStyle(.secondary)
                .accessibilityHidden(true)
                Stepper("Regolazione precisa",
                        value: $model[geometryUnitFor: dashboardID, widget: widgetID, field: field],
                        in: bounds, step: 100)
                    .frame(minHeight: AppTheme.minimumTouch)
                    .accessibilityLabel("Regola \(title.lowercased())")
                    .accessibilityValue(percentage)
            }
        }
    }

    private var title: String {
        switch field {
        case .x: "Posizione orizzontale"
        case .y: "Posizione verticale"
        case .width: "Larghezza"
        case .height: "Altezza"
        }
    }
    private var percentage: String {
        let value = Double(rectangle[field]) / 100
        if value > 0, value < 0.1 { return "Meno dello 0,1%" }
        return value.formatted(.number.precision(.fractionLength(0...1))) + "%"
    }
    private var lowerLabel: String {
        switch field {
        case .x: "Sinistra"
        case .y: "Alto"
        case .width: "Stretta"
        case .height: "Bassa"
        }
    }
    private var upperLabel: String {
        switch field {
        case .x: "Destra"
        case .y: "Basso"
        case .width: "Larga"
        case .height: "Alta"
        }
    }
}
