import SwiftUI
import Charts
import GaugeCore

struct HistoryTimelineChartView: View {
    let projection: HistoryTimelineProjection
    let definition: HistoryPIDDefinition?
    @ScaledMetric(relativeTo: .body) private var chartHeight = 250.0
    @State private var selectedSeconds: Double?

    private var selectedPoint: HistoryChartPoint? {
        guard let selectedSeconds else { return nil }
        return projection.chart.min { abs($0.seconds - selectedSeconds) < abs($1.seconds - selectedSeconds) }
    }
    private var average: Double? {
        let values = projection.rows.compactMap(\.value)
        guard !values.isEmpty else { return nil }
        return values.reduce(0, +) / Double(values.count)
    }

    var body: some View {
        let selected = selectedPoint
        Section {
            if projection.chart.isEmpty {
                ContentUnavailableView("Dato non disponibile", systemImage: "chart.xyaxis.line",
                    description: Text("Prova a scegliere un’altra misura."))
            } else {
                VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                    Text(selected == nil ? "Valore medio" : "A \(HistoryPresentation.elapsed(selected?.seconds ?? 0))")
                        .font(.subheadline).foregroundStyle(.secondary)
                    if let value = selected?.value ?? average {
                        Text("\(value, format: .number.precision(.fractionLength(0...1))) \(definition?.unit ?? "")")
                            .font(.largeTitle).bold().monospacedDigit()
                    }
                }.accessibilityElement(children: .combine)
                Chart(projection.chart) { point in
                    if projection.drawLines {
                        AreaMark(x: .value("Tempo di guida", point.seconds), y: .value("Valore", point.value),
                            series: .value("Andamento", point.segment))
                            .foregroundStyle(LinearGradient(colors: [AppTheme.accent.opacity(0.2), .clear], startPoint: .top, endPoint: .bottom))
                            .accessibilityHidden(true)
                        LineMark(x: .value("Tempo di guida", point.seconds), y: .value("Valore", point.value),
                            series: .value("Andamento", point.segment))
                            .foregroundStyle(AppTheme.accent).lineStyle(StrokeStyle(lineWidth: 2))
                            .accessibilityHidden(true)
                    }
                    PointMark(x: .value("Tempo di guida", point.seconds), y: .value("Valore", point.value))
                            .foregroundStyle(AppTheme.accent)
                            .symbolSize(projection.drawLines ? 8 : 25)
                            .accessibilityLabel("A \(HistoryPresentation.elapsed(point.seconds))")
                            .accessibilityValue("\(point.value.formatted(.number.precision(.fractionLength(0...1)))) \(definition?.unit ?? "")")
                    if selected?.id == point.id {
                        RuleMark(x: .value("Tempo selezionato", point.seconds))
                            .foregroundStyle(.secondary).accessibilityHidden(true)
                        PointMark(x: .value("Tempo di guida", point.seconds), y: .value("Valore", point.value))
                            .foregroundStyle(AppTheme.accent).symbolSize(70).accessibilityHidden(true)
                    }
                }
                .chartXSelection(value: $selectedSeconds)
                .chartXScale(domain: 0...max(1, projection.chart.last?.seconds ?? 0))
                .chartXAxis {
                    AxisMarks(values: .automatic(desiredCount: 4)) { tick in
                        AxisGridLine()
                        AxisValueLabel { if let seconds = tick.as(Double.self) { Text(HistoryPresentation.elapsed(seconds)) } }
                    }
                }
                .chartYAxisLabel(definition?.unit ?? "Valore")
                .chartXAxisLabel("Tempo di guida")
                .frame(height: chartHeight)
                .id("timeline-chart")
                .accessibilityLabel("Andamento di \(definition?.nameIT ?? "questa misura")")
                .accessibilityHint("Tocca il grafico per esplorare i valori. Tutte le letture sono disponibili più avanti.")
                Text("Tocca il grafico per vedere i valori.").font(.subheadline).foregroundStyle(.secondary)
            }
        } header: { Text(definition?.nameIT ?? "Andamento") }
    }
}
