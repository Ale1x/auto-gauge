import SwiftUI

struct HistoryTimelineFiltersView: View {
    @Bindable var timeline: HistoryTimelineModel

    var body: some View {
        Section {
            if timeline.sessions.count > 1 {
                NavigationLink {
                    HistoryTimelineSessionPickerView(timeline: timeline)
                } label: {
                    Label("Scegli un altro viaggio", systemImage: "car.side")
                }
            }
            Picker("Misura", selection: $timeline.selectedPID) {
                if timeline.pidChoices.isEmpty { Text("Nessuna misura").tag(Int?.none) }
                ForEach(timeline.pidChoices, id: \.self) { pid in
                    Text(HistoryPresentation.measure(pid)).tag(Optional(pid))
                }
            }.pickerStyle(.navigationLink)
            if timeline.ecuChoices.count > 1 {
                Picker("Lettura", selection: $timeline.selectedECU) {
                    ForEach(timeline.ecuChoices, id: \.self) { source in
                        Text(timeline.readingTitle(source)).tag(Optional(source))
                    }
                }.pickerStyle(.navigationLink)
            }
        }
    }
}
