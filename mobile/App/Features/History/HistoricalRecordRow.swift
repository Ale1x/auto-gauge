import SwiftUI
import GaugeCore

struct HistoricalRecordRow: View {
    let record: HistoricalLogRecord
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
            Text("Sequenza \(record.sequence.decimal)").font(.headline).monospacedDigit()
            Label(record.status.titleIT, systemImage: record.status == .valid ? "checkmark.circle" : "exclamationmark.circle")
                .foregroundStyle(record.status == .valid ? AppTheme.accent : AppTheme.warning)
            Text(record.relativeTimeIT).monospacedDigit().foregroundStyle(.secondary)
            Text("Service 01 · PID \(String(record.pid, radix: 16).uppercased()) · ECU 0x\(String(record.ecuSource, radix: 16).uppercased())")
                .font(.footnote).monospaced().foregroundStyle(.secondary)
            if record.status == .valid {
                Text("Bytes OBD: \(record.rawHex)").font(.footnote).monospaced().textSelection(.enabled)
            } else {
                Text("—").font(.title2).accessibilityLabel("Dato non disponibile")
            }
        }.padding(.vertical, AppTheme.smallSpacing).accessibilityElement(children: .combine)
    }
}
