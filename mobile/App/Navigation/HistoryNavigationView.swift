import SwiftUI

struct HistoryNavigationView: View {
    @Binding var path: [HistoryRoute]
    var body: some View {
        NavigationStack(path: $path) {
            SessionsView().navigationDestination(for: HistoryRoute.self) { route in
                switch route { case .session(let id): HistoricalSessionView(sessionID: id) }
            }
        }
    }
}
