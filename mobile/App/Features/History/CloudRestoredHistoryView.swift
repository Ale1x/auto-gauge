import SwiftUI
import GaugeCore

struct CloudRestoredHistoryView: View {
    @Environment(AppModel.self) private var model
    private var cloud: CloudAccountModel { model.cloud }
    var body: some View {
        List {
            Section {
                Label("Recuperati dall’account iCloud verificato", systemImage: "icloud.and.arrow.down").font(.headline)
                Text("Archivio separato dai dati dimostrativi del gauge. I tempi restano relativi all’avvio; nessuna telemetria live o data di guida ricostruita.").foregroundStyle(.secondary)
                Text("Una perdita è un’osservazione della sorgente: un altro telefono può avere recuperato quei record. I record disponibili vengono conservati anche dentro intervalli segnalati persi.")
            }
            if cloud.restoredItems.isEmpty {
                ContentUnavailableView("Nessuno storico iCloud recuperato", systemImage: "icloud.slash", description: Text("Verifica l’account e avvia il recupero dalla schermata Account. La configurazione predefinita mantiene il cloud disabilitato."))
            }
            ForEach(cloud.restoredItems, id: \.recordID) { item in
                Section {
                    Text("Dispositivo: \(item.deviceID)").font(.headline)
                    Text("Sessione: \(item.sessionID.value)").font(.footnote).monospaced().textSelection(.enabled)
                    if let record = item.record { HistoricalRecordRow(record: record) }
                    if let loss = item.loss {
                        Label("Perdita osservata · sequenze \(loss.first.decimal)–\(loss.last.decimal)", systemImage: "exclamationmark.triangle")
                            .foregroundStyle(AppTheme.warning).monospacedDigit()
                        Text("Non dimostra assenza di queste sequenze nel cloud.").font(.footnote).foregroundStyle(.secondary)
                    }
                }
            }
            Section {
                Text("Il recupero usa un indice aggiornato in modo asincrono. Anche a scansione terminata possono mancare modifiche recenti: una nuova scansione può trovarle. Nessuna cancellazione cloud viene propagata automaticamente a questo archivio locale.").foregroundStyle(.secondary)
            }
        }.navigationTitle("Storici iCloud")
    }
}
