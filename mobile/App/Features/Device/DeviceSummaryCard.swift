import SwiftUI

struct DeviceSummaryCard: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        VStack(alignment: .leading, spacing: AppTheme.spacing) {
            Label(model.sourceTitle, systemImage: "circle.dotted.circle").font(.headline).foregroundStyle(AppTheme.accent)
            Text("Il salvataggio sul telefono e l’applicazione al dispositivo selezionato sono azioni separate.").foregroundStyle(.secondary)
            ViewThatFits(in: .horizontal) {
                HStack(alignment: .top, spacing: AppTheme.pageSpacing) {
                    DeviceStatistic(title: "Dashboard", value: "\(model.configuration.dashboards.count) / 3")
                    DeviceStatistic(title: "Revisione sul telefono", value: "\(model.savedRevision)")
                }
                VStack(alignment: .leading, spacing: AppTheme.spacing) {
                    DeviceStatistic(title: "Dashboard", value: "\(model.configuration.dashboards.count) / 3")
                    DeviceStatistic(title: "Revisione sul telefono", value: "\(model.savedRevision)")
                }
            }
        }.modifier(AppCardStyle())
    }
}
