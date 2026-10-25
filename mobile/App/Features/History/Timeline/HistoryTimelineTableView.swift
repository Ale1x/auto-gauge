import SwiftUI

struct HistoryTimelineTableView: View {
    @Bindable var timeline: HistoryTimelineModel

    var body: some View {
        Section {
            DisclosureGroup("Tutte le letture") {
                ForEach(timeline.tableRows) { row in
                    HistoryTimelineRecordView(row: row, unit: timeline.definition?.unit ?? "", origin: timeline.projection?.originMS)
                }
                if timeline.pageCount > 1 {
                    HStack {
                        Button("Precedenti", systemImage: "chevron.backward", action: timeline.previousPage)
                            .disabled(timeline.page == 0)
                        Spacer()
                        Button("Successive", systemImage: "chevron.forward", action: timeline.nextPage)
                            .disabled(timeline.page + 1 >= timeline.pageCount)
                    }.frame(minHeight: AppTheme.minimumTouch)
                }
            }.id("timeline-table")
        }
    }
}
