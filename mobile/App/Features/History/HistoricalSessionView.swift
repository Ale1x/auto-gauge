import SwiftUI

struct HistoricalSessionView: View {
    @Environment(AppModel.self) private var model
    let sessionID: String

    var body: some View {
        if let session = model.logSessions.first(where: { $0.id == sessionID }) {
            HistoryTimelineView(source: .simulatedGauge, initialSessionID: session.id)
        } else {
            ContentUnavailableView("Viaggio non disponibile", systemImage: "car.side",
                description: Text("Torna all’elenco dei viaggi e aggiorna."))
        }
    }
}
