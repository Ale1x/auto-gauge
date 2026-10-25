import SwiftUI
import GaugeCore

struct DashboardGalleryCard: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dynamicTypeSize) private var typeSize
    let dashboard: Dashboard
    let number: Int
    let remove: () -> Void
    private var measureNames: String { dashboard.widgets.map(\.pid.nameIT).joined(separator: " · ") }

    var body: some View {
        HStack(alignment: .top, spacing: AppTheme.smallSpacing) {
            NavigationLink(value: DashboardRoute.editor(dashboard.id)) {
                Group {
                    if typeSize.isAccessibilitySize {
                        VStack(alignment: .leading, spacing: AppTheme.spacing) { preview; details }
                    } else {
                        HStack(spacing: AppTheme.spacing) { preview; details }
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .contentShape(.rect)
            }
            .buttonStyle(.plain)
            .accessibilityElement(children: .ignore)
            .accessibilityAddTraits(.isButton)
            .accessibilityLabel("Schermata \(number), \(measureNames)")
            .accessibilityHint("Apre le misure e la disposizione da personalizzare")
            .accessibilityIdentifier("screen.edit.\(dashboard.id)")
            Menu {
                Button("Disponi automaticamente", systemImage: "circle.grid.2x2") { model.useClusterLayout(dashboardID: dashboard.id) }
                if model.configuration.dashboards.count > 1 {
                    Button("Elimina schermata", systemImage: "trash", role: .destructive) { remove() }
                }
            } label: {
                Label("Opzioni schermata \(number)", systemImage: "ellipsis")
                    .labelStyle(.iconOnly)
                    .frame(minWidth: AppTheme.minimumTouch, minHeight: AppTheme.minimumTouch)
                    .contentShape(.rect)
            }
            .accessibilityIdentifier("screen.options.\(dashboard.id)")
        }
        .padding(AppTheme.spacing)
        .background(AppTheme.panel, in: .rect(cornerRadius: AppTheme.cardRadius))
        .disabled(model.isSaving)
    }
    private var preview: some View {
        GaugePreview(dashboard: dashboard, theme: model.displayTheme)
            .frame(width: typeSize.isAccessibilitySize ? 160 : 108)
            .accessibilityHidden(true)
    }
    private var details: some View {
        VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
            Text("Schermata \(number)").font(.headline).foregroundStyle(.primary)
            Text(measureNames).font(.subheadline).foregroundStyle(.secondary)
                .lineLimit(typeSize.isAccessibilitySize ? nil : 3)
            Label("Personalizza", systemImage: "chevron.right")
                .font(.subheadline).foregroundStyle(AppTheme.accent)
        }.frame(maxWidth: .infinity, alignment: .leading)
    }
}
