import SwiftUI
import GaugeCore

struct HistoryTimelineQualityView: View {
    let projection: HistoryTimelineProjection
    let source: HistoryTimelineSource
    var body: some View {
        Section("Qualità e disponibilità") {
            LabeledContent("Record nel filtro", value: "\(projection.rows.count)")
            LabeledContent("Valori nel grafico prima della riduzione", value: "\(projection.numericCount)")
            LabeledContent("Esclusi dal grafico", value: "\(projection.rows.count - projection.numericCount)")
            ForEach(projection.qualityCounts) { count in
                LabeledContent("Stato sorgente: \(count.status.titleIT)", value: "\(count.count)")
            }
            if projection.clockRegressions > 0 { Label("La sessione contiene \(projection.clockRegressions) regressioni del tempo monotono. Le linee sono interrotte.", systemImage: "exclamationmark.triangle").foregroundStyle(AppTheme.warning) }
            Text("Stato registrato e decodificabilità sono distinti. Errori, timeout e raw non compatibili non diventano zero o valori interpolati.").font(.footnote).foregroundStyle(.secondary)
        }
        Section(source == .restoredCloud ? "Perdite osservate nella sorgente" : "Perdite dichiarate dal gauge simulato") {
            if projection.lossObservations.isEmpty { Text("Nessuna perdita esplicita in questa copia.").foregroundStyle(.secondary) }
            ForEach(projection.lossObservations) { observation in
                VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                    Label("Sequenze \(observation.interval.first.decimal)–\(observation.interval.last.decimal)", systemImage: "exclamationmark.triangle").monospacedDigit()
                    Text("Record ora presenti nella sessione: \(observation.availableRecords). Sequenze non presenti nella copia: \(observation.unavailableCount).")
                        .font(.subheadline).monospacedDigit()
                }.accessibilityElement(children: .combine)
            }
            Text("Gli intervalli sono della sessione, non di un singolo PID o ECU. Non hanno timestamp: non viene inventata una durata sul grafico.").font(.footnote).foregroundStyle(.secondary)
            if source == .restoredCloud { Text("Una perdita osservata non prova assenza globale: i record recuperati da un altro telefono restano disponibili e vengono tracciati.").font(.footnote) }
            if !projection.copyGaps.isEmpty { Text("\(projection.copyGaps.count) intervalli di sequenza non presenti fra i record di questa copia, anche se già dichiarati come perdita. Le linee non li attraversano.").font(.footnote).foregroundStyle(.secondary) }
        }
    }
}
