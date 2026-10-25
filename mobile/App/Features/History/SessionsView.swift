import SwiftUI

struct SessionsView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        List {
            Section {
                VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                    Image(systemName: "road.lanes.curved.left")
                        .font(.largeTitle).foregroundStyle(AppTheme.accent).accessibilityHidden(true)
                    Text("Ogni viaggio, a colpo d’occhio").font(.title2).bold()
                    Text("Ritrova le tue misure e segui l’andamento della tua auto.")
                        .foregroundStyle(.secondary)
                }.padding(.vertical, AppTheme.smallSpacing)
            }
            if model.logSessions.isEmpty {
                Section {
                    ContentUnavailableView("I tuoi viaggi saranno qui", systemImage: "car.side",
                        description: Text("Aggiorna i viaggi dopo aver collegato Auto Gauge."))
                }
            } else {
                Section("Viaggi salvati") {
                    ForEach(model.logSessions) { session in
                        NavigationLink(value: HistoryRoute.session(session.id)) {
                            HistoricalSessionRow(session: session)
                        }
                    }
                }
            }
            if !model.isDemoMode && !model.cloud.restoredItems.isEmpty {
                Section {
                    NavigationLink(value: HistoryRoute.recovered) {
                        Label("Viaggi recuperati", systemImage: "icloud.and.arrow.down")
                    }
                }
            }
            Section {
                Button { refresh() } label: {
                    Label("Aggiorna viaggi", systemImage: "arrow.clockwise")
                        .frame(minHeight: AppTheme.minimumTouch)
                }.disabled(model.logBusy || (!model.isDemoMode && !model.gaugeConnected))
                if model.logBusy { ProgressView("Aggiornamento in corso") }
            }
        }
        .navigationTitle("Viaggi")
        .refreshable { await model.refreshHistory() }
    }

    private func refresh() { Task { await model.refreshHistory() } }
}
