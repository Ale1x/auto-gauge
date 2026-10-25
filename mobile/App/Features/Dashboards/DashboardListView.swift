import SwiftUI
import GaugeCore

struct DashboardListView: View {
    @Environment(AppModel.self) private var model
    @State private var confirmingRemoval = false
    @State private var removalID: Int?
    var showsHeading = true
    private var count: Int { model.configuration.dashboards.count }
    var body: some View {
        Group {
            if !model.isDemoMode && !model.isNativeSession {
                ContentUnavailableView("Collega il dispositivo", systemImage: "link",
                    description: Text("Collega Auto Gauge dalla scheda Dispositivo per personalizzare le schermate del display."))
            } else {
                ScrollView {
                    VStack(alignment: .leading, spacing: AppTheme.pageSpacing) {
                        VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                            if showsHeading {
                                Text("Schermate").font(.largeTitle.bold()).accessibilityIdentifier("screens.heading")
                            }
                            Text("Le informazioni che vedrai in auto.").font(.subheadline).foregroundStyle(.secondary)
                        }
                        if model.hasPendingChanges || model.isSaving { DashboardSaveBar() }
                        VStack(alignment: .leading, spacing: AppTheme.spacing) {
                            HStack {
                                Text(count == 1 ? "La tua schermata" : "Le tue schermate").font(.headline)
                                Spacer()
                                Text("\(count) di 3").font(.subheadline).foregroundStyle(.secondary)
                                    .accessibilityLabel("\(count) schermate, massimo 3")
                            }
                            ForEach(Array(model.configuration.dashboards.enumerated()), id: \.element.id) { index, dashboard in
                                DashboardGalleryCard(dashboard: dashboard, number: index + 1) {
                                    removalID = dashboard.id; confirmingRemoval = true
                                }
                            }
                            if count < 3 {
                                Button(action: model.addDashboard) {
                                    Label("Nuova schermata", systemImage: "plus")
                                        .frame(maxWidth: .infinity, minHeight: 52)
                                }
                                .buttonStyle(.bordered).disabled(model.isSaving)
                                .accessibilityIdentifier("screens.add")
                            }
                        }
                        Label("Sul display, passa da una schermata all’altra con un gesto.", systemImage: "hand.draw")
                            .font(.subheadline).foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                        if !model.hasPendingChanges && !model.isSaving {
                            Label(model.saveStatusText, systemImage: "checkmark.circle")
                                .font(.subheadline).foregroundStyle(.secondary)
                        }
                    }
                    .padding(AppTheme.pageSpacing)
                    .frame(maxWidth: AppTheme.readableWidth).frame(maxWidth: .infinity)
                }
            }
        }
        .background(AppTheme.canvas.ignoresSafeArea())
        .navigationTitle("Schermate").navigationBarTitleDisplayMode(.inline)
        .toolbarVisibility(showsHeading ? .hidden : .visible, for: .navigationBar)
        .alert("Eliminare questa schermata?", isPresented: $confirmingRemoval) {
            Button("Elimina schermata", role: .destructive) {
                if let removalID { model.edit { try $0.removeDashboard(id: removalID) } }
                removalID = nil
            }
            Button("Annulla", role: .cancel) { removalID = nil }
        } message: { Text("Le altre schermate resteranno disponibili. Salva le modifiche per confermare.") }
    }
}
