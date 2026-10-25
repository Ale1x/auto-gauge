import SwiftUI

struct HistoryTimelineSessionPickerView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    @Bindable var timeline: HistoryTimelineModel

    var body: some View {
        List(timeline.sessions) { session in
            Button { select(session.id) } label: {
                HStack {
                    VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                        Text(title(session.id)).font(.headline)
                        Text(HistoryPresentation.duration(session.records)).foregroundStyle(.secondary)
                    }
                    Spacer()
                    if session.id == timeline.selectedSessionID {
                        Image(systemName: "checkmark.circle.fill").foregroundStyle(AppTheme.accent)
                    }
                }.frame(minHeight: AppTheme.minimumTouch)
            }
        }.navigationTitle("Scegli viaggio")
    }

    private func title(_ id: String) -> String {
        model.logSessions.first(where: { $0.id == id }).map { model.historyTitle(for: $0) } ?? "Viaggio salvato"
    }
    private func select(_ id: String) { timeline.selectedSessionID = id; dismiss() }
}
