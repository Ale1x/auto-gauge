import SwiftUI
import GaugeCore

struct HistoricalSessionRow: View {
    let session: ImportedLogSession
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
            Text("Sessione \(session.sessionID.value.prefix(8))").font(.headline)
            Text("Record: \(session.records.count) · intervalli di perdita: \(session.lost.count)").foregroundStyle(.secondary)
            Text(session.deviceID).font(.footnote).monospaced().foregroundStyle(.secondary)
        }.frame(minHeight: AppTheme.minimumTouch, alignment: .leading)
         .accessibilityElement(children: .combine)
    }
}
