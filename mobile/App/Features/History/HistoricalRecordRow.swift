import SwiftUI
import GaugeCore

struct HistoricalRecordRow: View {
    let record: HistoricalLogRecord

    var body: some View {
        LabeledContent {
            if let value = HistoryPresentation.value(record) {
                Text("\(value, format: .number.precision(.fractionLength(0...1))) \(HistoryPIDDefinition.find(record.pid)?.unit ?? "")")
                    .monospacedDigit()
            } else {
                Text("Dato non disponibile").foregroundStyle(.secondary)
            }
        } label: { Text(HistoryPresentation.measure(record.pid)) }
        .padding(.vertical, AppTheme.smallSpacing)
        .accessibilityElement(children: .combine)
    }
}
