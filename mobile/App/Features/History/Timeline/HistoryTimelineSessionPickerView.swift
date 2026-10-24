import SwiftUI

struct HistoryTimelineSessionPickerView: View {
    @Environment(\.dismiss) private var dismiss
    @Bindable var timeline: HistoryTimelineModel
    var body: some View {
        List(timeline.sessions) { session in
            Button { select(session.id) } label: {
                VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                    Label(session.deviceID, systemImage: session.id == timeline.selectedSessionID ? "checkmark.circle.fill" : "circle")
                    Text("Sessione \(session.sessionID.value)").font(.footnote).monospaced()
                }.frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch, alignment: .leading)
            }.accessibilityLabel("Dispositivo \(session.deviceID), sessione \(session.sessionID.value)")
        }.navigationTitle("Scegli sessione")
    }
    private func select(_ id: String) { timeline.selectedSessionID = id; dismiss() }
}
