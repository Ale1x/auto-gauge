import SwiftUI
import GaugeCore

struct HistoricalSessionView: View {
    @Environment(AppModel.self) private var model
    let sessionID: String
    var body: some View {
        if let session = model.logSessions.first(where: { $0.id == sessionID }) {
            List {
                Section("Sessione dimostrativa") {
                    LabeledContent("Dispositivo", value: session.deviceID)
                    Text("ID sessione: \(session.sessionID.value)").font(.footnote).monospaced().textSelection(.enabled)
                    LabeledContent("Cursore sul telefono", value: session.acknowledgedThrough.decimal).monospacedDigit()
                    LabeledContent("Orario assoluto", value: "Non disponibile")
                    Text("I tempi sono relativi all’avvio del gauge. Nessuna data di guida viene ricostruita senza un’ancora temporale affidabile.").foregroundStyle(.secondary)
                }
                Section { NavigationLink(value: HistoryRoute.timeline(session.id)) { Label("Analizza misure nel tempo", systemImage: "chart.xyaxis.line") } }
                if !session.lost.isEmpty {
                    Section("Perdite dichiarate dalla sorgente") {
                        ForEach(session.lost) { interval in
                            Label("Sequenze \(interval.first.decimal)–\(interval.last.decimal)", systemImage: "exclamationmark.triangle")
                                .monospacedDigit().foregroundStyle(AppTheme.warning)
                        }
                    }
                }
                Section("Record importati") {
                    ForEach(session.records) { record in HistoricalRecordRow(record: record) }
                }
            }.navigationTitle("Dettaglio storico").navigationBarTitleDisplayMode(.inline)
        } else {
            ContentUnavailableView("Sessione non disponibile", systemImage: "clock", description: Text("Torna all’elenco degli storici."))
        }
    }
}
