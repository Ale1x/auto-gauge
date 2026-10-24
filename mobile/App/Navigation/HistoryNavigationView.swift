import SwiftUI

struct HistoryNavigationView: View {
    @Binding var path: [HistoryRoute]
    var body: some View {
        NavigationStack(path: $path) {
            SessionsView().modifier(SimulationBannerModifier()).navigationDestination(for: HistoryRoute.self) { route in
                Group {
                switch route {
                case .session(let id): HistoricalSessionView(sessionID: id)
                case .timeline(let id): HistoryTimelineView(source: .simulatedGauge, initialSessionID: id)
                }
                }.modifier(SimulationBannerModifier())
            }
        }
    }
}
