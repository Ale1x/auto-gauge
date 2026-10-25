import SwiftUI
import GaugeCore

struct DashboardEditorView: View {
    @Environment(AppModel.self) private var model
    let dashboardID: Int
    private var selectedID: Int? {
        model.editor.selection?.dashboardID == dashboardID ? model.editor.selection?.widgetID : nil
    }
    var body: some View {
        if !model.isDemoMode && !model.isNativeSession {
            ContentUnavailableView("Collega il dispositivo", systemImage: "link",
                                   description: Text("Collega Auto Gauge dalla scheda Dispositivo per personalizzare le schermate del display."))
                .navigationTitle("Modifica schermata")
        } else if let dashboard = model.configuration.dashboards.first(where: { $0.id == dashboardID }) {
            Form {
                Section {
                    GaugePreview(dashboard: dashboard, theme: model.displayTheme, interactive: true, selectedID: selectedID, onSelect: select, onMove: move)
                        .frame(maxWidth: .infinity).padding(.vertical, 12)
                    Text("Anteprima").font(.footnote.bold()).foregroundStyle(AppTheme.muted)
                        .frame(maxWidth: .infinity)
                    Text("Tocca una misura per selezionarla e trascinala nella posizione che preferisci. Puoi regolare posizione e dimensioni anche con i controlli sotto.")
                        .font(.footnote).foregroundStyle(.secondary)
                }.listRowBackground(Color.clear)
                let clipped = model.editor.clippedWidgetIDs(dashboardID: dashboardID)
                if !clipped.isEmpty {
                    Section {
                        Label("Alcune misure escono dal display", systemImage: "exclamationmark.triangle")
                            .foregroundStyle(AppTheme.warning).font(.subheadline)
                        Text("Sposta o riduci le misure segnalate per renderle completamente visibili.")
                            .font(.footnote).foregroundStyle(.secondary)
                    }
                }
                Section("Misure della schermata") {
                    ForEach(dashboard.widgets) { widget in
                        Button { select(widget.id) } label: {
                            HStack {
                                VStack(alignment: .leading) {
                                    Text(widget.pid.nameIT).foregroundStyle(.primary)
                                    Text("\(GaugePreviewValue.value(for: widget.pid.pid)) \(widget.pid.unit)").font(.footnote).foregroundStyle(.secondary)
                                }
                                Spacer()
                                if clipped.contains(widget.id) { Image(systemName: "exclamationmark.triangle").foregroundStyle(AppTheme.warning).accessibilityLabel("Tagliata sul bordo circolare") }
                                Image(systemName: selectedID == widget.id ? "checkmark.circle.fill" : "circle")
                                    .foregroundStyle(selectedID == widget.id ? AppTheme.accent : AppTheme.muted)
                                    .accessibilityHidden(true)
                            }
                        }.frame(minHeight: AppTheme.minimumTouch)
                         .deleteDisabled(dashboard.widgets.count == 1 || model.isSaving)
                         .accessibilityLabel(widget.pid.nameIT)
                         .accessibilityValue(selectedID == widget.id ? "Selezionata" : "Non selezionata")
                    }.onDelete { offsets in
                        let ids = offsets.map { dashboard.widgets[$0].id }
                        model.edit { draft in for id in ids { try draft.removeWidget(dashboardID: dashboardID, widgetID: id) } }
                    }
                    Menu {
                        ForEach(model.catalog) { pid in
                            Button(pid.nameIT) { model.edit { _ = try $0.addWidget(dashboardID: dashboardID, pid: pid) } }
                        }
                    } label: { Label("Aggiungi misura", systemImage: "plus") }
                    .frame(minHeight: AppTheme.minimumTouch).disabled(dashboard.widgets.count >= 5 || model.catalog.isEmpty)
                }
                if let selectedID, let widget = dashboard.widgets.first(where: { $0.id == selectedID }),
                   let rectangle = try? EditingRectangle(widget.frame) {
                    Section {
                        DisclosureGroup("Regola posizione e dimensioni") {
                            ForEach(GeometryField.allCases, id: \.self) { field in
                                GeometryControlView(field: field, rectangle: rectangle, widgetID: selectedID, dashboardID: dashboardID)
                            }
                        }
                    }
                }
                DraftActionsView()
            }.navigationTitle("Modifica schermata").navigationBarTitleDisplayMode(.inline)
             .disabled(model.isSaving)
             .onAppear { if selectedID == nil, let id = dashboard.widgets.first?.id { select(id) } }
        } else {
            ContentUnavailableView("Schermata rimossa", systemImage: "rectangle.slash", description: Text("Torna all’elenco per scegliere un’altra schermata."))
        }
    }
    private func select(_ id: Int) { model.edit { try $0.select(dashboardID: dashboardID, widgetID: id) } }
    private func move(_ id: Int, _ x: Int, _ y: Int) {
        model.edit {
            try $0.select(dashboardID: dashboardID, widgetID: id)
            try $0.setGeometry(dashboardID: dashboardID, widgetID: id, field: .x, units: x)
            try $0.setGeometry(dashboardID: dashboardID, widgetID: id, field: .y, units: y)
        }
    }
}
