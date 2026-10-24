import SwiftUI

struct HistoryTimelineFiltersView: View {
    @Bindable var timeline: HistoryTimelineModel
    var body: some View {
        Section("Filtri") {
            NavigationLink {
                HistoryTimelineSessionPickerView(timeline: timeline).modifier(SimulationBannerModifier())
            } label: {
                LabeledContent("Sessione", value: timeline.session.map { "\($0.deviceID) · \($0.sessionID.value.prefix(8))" } ?? "Nessuna")
            }.disabled(timeline.sessions.isEmpty)
                .accessibilityValue(timeline.session.map { "Dispositivo \($0.deviceID), sessione \($0.sessionID.value)" } ?? "Nessuna sessione")
            Picker("Misura", selection: $timeline.selectedPID) {
                if timeline.pidChoices.isEmpty { Text("Nessuna misura").tag(Int?.none) }
                ForEach(timeline.pidChoices, id: \.self) { pid in Text(HistoryTimelineModel.pidTitle(pid)).tag(Optional(pid)) }
            }.pickerStyle(.navigationLink)
            Picker("Sorgente ECU", selection: $timeline.selectedECU) {
                if timeline.ecuChoices.isEmpty { Text("Nessuna sorgente").tag(UInt32?.none) }
                ForEach(timeline.ecuChoices, id: \.self) { ecu in Text(HistoryTimelineModel.ecuTitle(ecu)).tag(Optional(ecu)) }
            }.pickerStyle(.navigationLink)
        }
    }
}
