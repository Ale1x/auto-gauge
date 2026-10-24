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
            LabeledContent(field.titleIT, value: "\(rectangle[field]) / 10000").monospacedDigit()
            if bounds.lowerBound < bounds.upperBound {
                Slider(value: $model[geometryValueFor: dashboardID, widget: widgetID, field: field], in: Double(bounds.lowerBound)...Double(bounds.upperBound), step: 1)
                    .frame(minHeight: AppTheme.minimumTouch)
                    .accessibilityLabel(field.titleIT).accessibilityValue("\(rectangle[field]) su diecimila")
            }
            Stepper("Regola \(field.titleIT.lowercased())", value: $model[geometryUnitFor: dashboardID, widget: widgetID, field: field], in: bounds, step: 1)
                .frame(minHeight: AppTheme.minimumTouch)
        }
    }
}
