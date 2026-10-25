import SwiftUI
import GaugeCore

struct HistoryTimelineQualityView: View {
    let projection: HistoryTimelineProjection
    let source: HistoryTimelineSource

    private var hasUnavailableData: Bool {
        projection.rows.contains { $0.value == nil } || !projection.lossObservations.isEmpty
            || !projection.copyGaps.isEmpty || projection.clockRegressions > 0 || projection.timePrecisionLimited
    }

    var body: some View {
        if hasUnavailableData {
            Section {
                Label("Alcuni dati non sono disponibili.", systemImage: "info.circle")
                    .font(.subheadline).foregroundStyle(.secondary)
            }
        }
    }
}
