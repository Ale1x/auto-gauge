import SwiftUI
import GaugeCore

struct DashboardListView: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        List {
            Section {
                Text("Personalizza il display del gauge. L’anteprima usa valori fissi di esempio e non riceve dati live.")
                    .font(.subheadline).foregroundStyle(.secondary)
            }
            Section {
                ForEach(model.configuration.dashboards) { dashboard in
                    NavigationLink(value: DashboardRoute.editor(dashboard.id)) {
                        HStack {
                            Text(String(dashboard.id)).font(.headline.monospaced()).foregroundStyle(AppTheme.accent).frame(width: 26)
                            VStack(alignment: .leading) {
                                Text(dashboard.name)
                                Text("\(dashboard.widgets.count) misure").font(.footnote).foregroundStyle(.secondary)
                            }
                        }
                    }
                }.onDelete { offsets in
                    let ids = offsets.map { model.configuration.dashboards[$0].id }
                    model.edit { draft in for id in ids { try draft.removeDashboard(id: id) } }
                }
                AppActionButton(title: "Aggiungi dashboard", symbol: "plus.circle", disabled: model.configuration.dashboards.count >= 3, action: model.addDashboard)
            } header: { Text("Le tue dashboard") } footer: { Text("Da 1 a 3 dashboard, fino a 5 misure per ciascuna. Gli ID delle dashboard esistenti non cambiano dopo una rimozione.") }
            DraftActionsView()
        }.navigationTitle("Dashboard")
    }
}
