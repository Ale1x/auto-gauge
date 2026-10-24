import SwiftUI
import GaugeCore

struct HistoryTimelineRecordView: View {
    let row: HistoryTimelineRow
    let unit: String
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
            Text("Sequenza \(row.record.sequence.decimal)").font(.headline).monospacedDigit()
            if let value = row.value {
                Text("\(value, format: .number.precision(.fractionLength(0...2))) \(unit)").font(.title3).monospacedDigit()
            } else { Text("Nessun valore numerico").foregroundStyle(.secondary) }
            Label(row.quality.titleIT, systemImage: row.value == nil ? "exclamationmark.circle" : "checkmark.circle")
                .foregroundStyle(row.value == nil ? AppTheme.warning : AppTheme.accent)
            Text("Stato sorgente: \(row.record.status.titleIT)").font(.subheadline)
            Text(row.record.relativeTimeIT).monospacedDigit()
            Text("Contatore esatto: \(row.record.monotonicMS.decimal) ms").font(.footnote).monospacedDigit()
            Text("Raw: \(row.record.rawHex.isEmpty ? "assente" : row.record.rawHex)").font(.footnote).monospaced().textSelection(.enabled)
        }.padding(.vertical, AppTheme.smallSpacing).accessibilityElement(children: .combine)
    }
}
