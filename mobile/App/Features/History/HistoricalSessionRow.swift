import SwiftUI
import GaugeCore

struct HistoricalSessionRow: View {
    @Environment(AppModel.self) private var model
    let session: ImportedLogSession

    var body: some View {
        HStack(alignment: .top, spacing: AppTheme.spacing) {
            Image(systemName: "car.side.fill").foregroundStyle(AppTheme.accent)
                .font(.title2).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                Text(model.historyTitle(for: session)).font(.headline)
                if let date = model.historyDate(for: session) {
                    Text(date, format: .dateTime.day().month(.abbreviated).hour().minute())
                        .foregroundStyle(.secondary)
                }
                Label(HistoryPresentation.duration(session.records), systemImage: "clock")
                    .font(.subheadline).foregroundStyle(.secondary)
            }
        }
        .padding(.vertical, AppTheme.smallSpacing)
        .frame(minHeight: AppTheme.minimumTouch, alignment: .leading)
        .accessibilityElement(children: .combine)
    }
}
