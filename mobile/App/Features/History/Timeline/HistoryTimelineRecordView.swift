import SwiftUI
import GaugeCore

struct HistoryTimelineRecordView: View {
    let row: HistoryTimelineRow
    let unit: String
    let origin: LogUInt64?

    var body: some View {
        LabeledContent {
            if let value = row.value {
                Text("\(value, format: .number.precision(.fractionLength(0...1))) \(unit)").monospacedDigit()
            } else {
                Text("Dato non disponibile").foregroundStyle(.secondary)
            }
        } label: {
            Text(HistoryPresentation.elapsed(row.record.monotonicMS, from: origin)).monospacedDigit()
        }
        .padding(.vertical, AppTheme.smallSpacing)
        .accessibilityElement(children: .combine)
    }
}
