import SwiftUI

struct CloudRestoredHistoryView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        if model.isDemoMode {
            SessionsView()
        } else if model.cloud.restoredItems.isEmpty {
            ContentUnavailableView("Nessun viaggio recuperato", systemImage: "icloud.and.arrow.down",
                description: Text("Puoi recuperare i tuoi viaggi dalle Impostazioni."))
                .navigationTitle("Viaggi recuperati")
        } else {
            HistoryTimelineView(source: .restoredCloud)
        }
    }
}
