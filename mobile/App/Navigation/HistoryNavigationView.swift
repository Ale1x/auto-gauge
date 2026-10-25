import SwiftUI

struct HistoryNavigationView: View {
    @Binding var path: [HistoryRoute]
    var body: some View {
        NavigationStack(path: $path) {
            SessionsView().navigationDestination(for: HistoryRoute.self) { route in
                Group {
                switch route {
                case .recovered: CloudRestoredHistoryView()
                case .session(let id): HistoricalSessionView(sessionID: id)
                case .timeline(let id): HistoryTimelineView(source: .simulatedGauge, initialSessionID: id)
                }
                }
            }
        }
    }
}
