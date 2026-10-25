import SwiftUI
import GaugeCore

struct HistoryTimelineOriginView: View {
    let source: HistoryTimelineSource
    let session: HistoryTimelineSession?
    let projection: HistoryTimelineProjection

    var body: some View {
        if let session {
            LabeledContent("Durata", value: HistoryPresentation.duration(session.records))
        }
    }
}
