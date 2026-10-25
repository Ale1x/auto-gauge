import SwiftUI
import GaugeCore

struct DashboardListView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        Group {
            if !model.isDemoMode && !model.isNativeSession {
                ContentUnavailableView("Collega il dispositivo", systemImage: "link",
                                       description: Text("Collega Auto Gauge dalla scheda Dispositivo per personalizzare le schermate del display."))
            } else {
                List {
                    Section {
                        Text("Scegli quali misure vedere e come disporle sul display.")
                            .font(.subheadline).foregroundStyle(.secondary)
                    }
                    Section {
                        ForEach(model.configuration.dashboards) { dashboard in
                            NavigationLink(value: DashboardRoute.editor(dashboard.id)) {
                                Label {
                                    VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                                        Text(screenName(dashboard)).font(.headline)
                                        Text(dashboard.widgets.count == 1 ? "1 misura" : "\(dashboard.widgets.count) misure")
                                            .font(.footnote).foregroundStyle(.secondary)
                                    }
                                } icon: {
                                    Image(systemName: "rectangle.on.rectangle")
                                        .foregroundStyle(AppTheme.accent)
                                }
                                .frame(minHeight: AppTheme.minimumTouch)
                            }
                            .deleteDisabled(model.configuration.dashboards.count == 1 || model.isSaving)
                        }
                        .onDelete(perform: remove)
                        AppActionButton(title: "Nuova schermata", symbol: "plus.circle",
                                        disabled: model.configuration.dashboards.count >= 3 || model.isSaving,
                                        action: model.addDashboard)
                    } header: {
                        Text("Le tue schermate")
                    } footer: {
                        Text("Puoi creare fino a 3 schermate, con 5 misure ciascuna. Tocca il display di Auto Gauge per passare alla successiva.")
                    }
                    DraftActionsView()
                }
                .disabled(model.isSaving)
            }
        }
        .navigationTitle("Schermate del display")
    }

    private func screenName(_ dashboard: Dashboard) -> String {
        let position = model.configuration.dashboards.firstIndex(where: { $0.id == dashboard.id }) ?? 0
        return "Schermata \(position + 1)"
    }
    private func remove(_ offsets: IndexSet) {
        let ids = offsets.map { model.configuration.dashboards[$0].id }
        model.edit { draft in for id in ids { try draft.removeDashboard(id: id) } }
    }
}
