import SwiftUI

struct HistoryTimelineTableView: View {
    @Bindable var timeline: HistoryTimelineModel
    var body: some View {
        Section("Tutti i record del filtro") {
            Text("Pagina \(timeline.page + 1) di \(timeline.pageCount) · massimo \(HistoryTimelineModel.rowsPerPage) record per pagina.")
                .font(.subheadline).foregroundStyle(.secondary).id("timeline-table")
            ForEach(timeline.tableRows) { row in HistoryTimelineRecordView(row: row, unit: timeline.definition?.unit ?? "") }
            AppActionButton(title: "Pagina precedente", symbol: "chevron.backward", disabled: timeline.page == 0, action: timeline.previousPage)
            AppActionButton(title: "Pagina successiva", symbol: "chevron.forward", disabled: timeline.page + 1 >= timeline.pageCount, action: timeline.nextPage)
        }
    }
}
