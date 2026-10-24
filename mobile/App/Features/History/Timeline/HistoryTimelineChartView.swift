import SwiftUI
import Charts
import GaugeCore

struct HistoryTimelineChartView: View {
    let projection: HistoryTimelineProjection
    let definition: HistoryPIDDefinition?
    @ScaledMetric(relativeTo: .body) private var chartHeight = 240.0
    var body: some View {
        Section("Andamento dei valori disponibili") {
            if projection.chart.isEmpty {
                ContentUnavailableView("Nessun valore da tracciare", systemImage: "chart.xyaxis.line", description: Text("La tabella conserva i raw e spiega stati, formule assenti o dati incoerenti. Nessun valore viene sostituito con zero."))
            } else {
                Chart(projection.chart) { point in
                    if projection.drawLines {
                        LineMark(x: .value("Secondi relativi", point.seconds), y: .value("Valore", point.value), series: .value("Tratto continuo", point.segment))
                            .foregroundStyle(AppTheme.accent)
                            .accessibilityHidden(true)
                    }
                    PointMark(x: .value("Secondi relativi", point.seconds), y: .value("Valore", point.value))
                        .foregroundStyle(AppTheme.accent)
                        .accessibilityLabel("Sequenza \(point.sequence.decimal), \(point.monotonicMS.decimal) millisecondi dall’avvio")
                        .accessibilityValue("\(point.value.formatted(.number.precision(.fractionLength(0...2)))) \(definition?.unit ?? "")")
                }
                .chartXScale(domain: 0...max(1, projection.chart.last?.seconds ?? 0))
                .chartYAxisLabel(definition?.unit ?? "Valore")
                .chartXAxisLabel("Secondi dal primo record della sessione")
                .frame(height: chartHeight)
                .id("timeline-chart")
                .accessibilityLabel("Grafico storico di \(definition?.nameIT ?? "misura")")
                .accessibilityHint("La tabella seguente consente di leggere tutti i record, anche quelli esclusi dal grafico.")
                Text("\(projection.chart.count) punti mostrati su \(projection.numericCount) valori decodificabili.")
                    .font(.subheadline).foregroundStyle(.secondary)
                if projection.decimated { Text("Grafico ridotto per mantenere la schermata fluida. I raw e la tabella completa restano intatti.").font(.footnote) }
                if !projection.drawLines { Text("Solo punti: le interruzioni sono troppe per conservare tutte le linee in un grafico ridotto.").foregroundStyle(.secondary) }
                if projection.timePrecisionLimited { Label("Scala temporale approssimata per contatori molto grandi. I contatori esatti restano nella tabella.", systemImage: "exclamationmark.triangle").foregroundStyle(AppTheme.warning) }
            }
        }
    }
}
