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
                ScrollView {
                    VStack(alignment: .leading, spacing: AppTheme.pageSpacing) {
                        Text("Il tuo display, a modo tuo.").font(.title2.bold())
                        Text("Scegli una schermata per cambiare le misure e la loro disposizione.")
                            .foregroundStyle(.secondary)
                        ForEach(Array(model.configuration.dashboards.enumerated()), id: \.element.id) { index, dashboard in
                            DashboardGalleryCard(dashboard: dashboard, number: index + 1) {
                                model.edit { try $0.removeDashboard(id: dashboard.id) }
                            }
                        }
                        Button(action: model.addDashboard) {
                            Label("Nuova schermata", systemImage: "plus")
                                .frame(maxWidth: .infinity, minHeight: 56)
                        }
                        .buttonStyle(.bordered)
                        .disabled(model.configuration.dashboards.count >= 3 || model.isSaving)
                        Text("Fino a 3 schermate. Sul display passi alla successiva con un gesto.")
                            .font(.footnote).foregroundStyle(.secondary)
                        DashboardSaveBar()
                    }
                    .padding(AppTheme.pageSpacing)
                    .frame(maxWidth: AppTheme.readableWidth).frame(maxWidth: .infinity)
                }
            }
        }
        .background(AppTheme.canvas.ignoresSafeArea())
        .navigationTitle("Schermate")
        .navigationBarTitleDisplayMode(.inline)
        .toolbarVisibility(.visible, for: .navigationBar)
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                Button("Salva") { Task { await model.saveChanges() } }
                    .accessibilityLabel("Salva modifiche")
                    .accessibilityIdentifier("configuration.save")
                    .disabled(!model.hasPendingChanges || model.isSaving)
            }
        }
    }
}
