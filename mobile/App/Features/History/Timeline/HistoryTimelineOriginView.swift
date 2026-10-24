import SwiftUI
import GaugeCore

struct HistoryTimelineOriginView: View {
    let source: HistoryTimelineSource
    let session: HistoryTimelineSession?
    let projection: HistoryTimelineProjection
    var body: some View {
        Section {
            DisclosureGroup("Origine e tempi") {
                Text(source == .simulatedGauge ? "Record letti dal file locale della simulazione. Non sono misure live né dati di un veicolo reale." : "Record già recuperati sul telefono dall’account iCloud verificato. Questa vista non contatta il cloud.")
                Text("I tempi sono monotoni, relativi all’avvio del gauge. Nessuna data assoluta viene ricostruita.")
                if let session { Text("Sessione: \(session.sessionID.value)").font(.footnote).monospaced().textSelection(.enabled) }
                if let origin = projection.originMS { Text("Origine del grafico: \(origin.decimal) ms dall’avvio.").monospacedDigit() }
                Text("Le linee sono una guida fra campioni disponibili, non una misura continua. La tabella conserva tutti i raw e gli stati.")
            }
        }
    }
}
