import SwiftUI
import GaugeCore

struct DashboardGalleryCard: View {
    @Environment(AppModel.self) private var model
    let dashboard: Dashboard
    let number: Int
    let remove: () -> Void
    @State private var confirmingRemoval = false

    var body: some View {
        VStack(spacing: AppTheme.spacing) {
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Schermata \(number)").font(.title3.bold())
                    Text(dashboard.widgets.count == 1 ? "1 misura" : "\(dashboard.widgets.count) misure")
                        .font(.subheadline).foregroundStyle(.secondary)
                }
                Spacer()
                Menu("Opzioni schermata", systemImage: "ellipsis") {
                    Button("Usa layout originale", systemImage: "circle.grid.2x2") { model.useClusterLayout(dashboardID: dashboard.id) }
                    if model.configuration.dashboards.count > 1 {
                        Button("Elimina schermata", systemImage: "trash", role: .destructive) { confirmingRemoval = true }
                    }
                }.labelStyle(.iconOnly).frame(minWidth: AppTheme.minimumTouch, minHeight: AppTheme.minimumTouch)
                .confirmationDialog("Eliminare questa schermata?", isPresented: $confirmingRemoval, titleVisibility: .visible) {
                    Button("Elimina schermata", role: .destructive, action: remove)
                    Button("Annulla", role: .cancel) { }
                }
            }
            GaugePreview(dashboard: dashboard, theme: model.displayTheme)
                .frame(maxWidth: .infinity).accessibilityHidden(true)
            NavigationLink(value: DashboardRoute.editor(dashboard.id)) {
                Label("Modifica schermata", systemImage: "slider.horizontal.3")
                    .frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch)
            }
            .buttonStyle(.borderedProminent)
            .accessibilityIdentifier("screen.edit.\(dashboard.id)")
        }
        .padding(AppTheme.pageSpacing)
        .background(AppTheme.panel, in: .rect(cornerRadius: 28))
        .disabled(model.isSaving)
    }
}
