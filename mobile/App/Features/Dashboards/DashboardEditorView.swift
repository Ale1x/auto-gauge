import SwiftUI
import GaugeCore

struct DashboardEditorView: View {
    @Environment(AppModel.self) private var model
    let dashboardID: Int
    private var selectedID: Int? {
        model.editor.selection?.dashboardID == dashboardID ? model.editor.selection?.widgetID : nil
    }
    var body: some View {
        if let dashboard = model.configuration.dashboards.first(where: { $0.id == dashboardID }) {
            Form {
                Section {
                    GaugePreview(dashboard: dashboard, selectedID: selectedID, onSelect: select, onMove: move)
                        .frame(maxWidth: .infinity).padding(.vertical, 12)
                    Text("ANTEPRIMA · VALORI DIMOSTRATIVI").font(.footnote.bold()).foregroundStyle(AppTheme.muted)
                        .frame(maxWidth: .infinity)
                    Text("Tocca una misura per selezionarla. Con testo molto grande l’anteprima mostra gli ID: nomi e valori di esempio restano nella lista. Puoi trascinare o usare i controlli sotto.")
                        .font(.footnote).foregroundStyle(.secondary)
                }.listRowBackground(Color.clear)
                let clipped = model.editor.clippedWidgetIDs(dashboardID: dashboardID)
                if !clipped.isEmpty {
                    Section {
                        Label("\(clipped.count) misure tagliate dal bordo circolare", systemImage: "exclamationmark.triangle")
                            .foregroundStyle(AppTheme.warning).font(.subheadline)
                        Text("Il riquadro supera il bordo del display e l’anteprima viene tagliata. Controlla posizione e dimensioni prima di salvare.")
                            .font(.footnote).foregroundStyle(.secondary)
                    }
                }
                Section(dashboard.name) {
                    ForEach(dashboard.widgets) { widget in
                        Button { select(widget.id) } label: {
                            HStack {
                                VStack(alignment: .leading) {
                                    Text(widget.pid.nameIT).foregroundStyle(.primary)
                                    Text("Misura \(widget.id) · esempio \(GaugePreviewValue.value(for: widget.pid.pid)) \(widget.pid.unit)").font(.footnote).foregroundStyle(.secondary)
                                }
                                Spacer()
                                if clipped.contains(widget.id) { Image(systemName: "exclamationmark.triangle").foregroundStyle(AppTheme.warning).accessibilityLabel("Tagliata sul bordo circolare") }
                                Image(systemName: selectedID == widget.id ? "checkmark.circle.fill" : "circle")
                            }
                        }.frame(minHeight: AppTheme.minimumTouch).accessibilityLabel("\(widget.pid.nameIT), misura \(widget.id)")
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
                    .frame(minHeight: AppTheme.minimumTouch).disabled(dashboard.widgets.count >= 5)
                }
                if let selectedID, let widget = dashboard.widgets.first(where: { $0.id == selectedID }),
                   let rectangle = try? EditingRectangle(widget.frame) {
                    Section("Posizione e dimensioni · misura \(selectedID)") {
                        ForEach(GeometryField.allCases, id: \.self) { field in
                            GeometryControlView(field: field, rectangle: rectangle, widgetID: selectedID, dashboardID: dashboardID)
                        }
                    }
                }
                DraftActionsView()
            }.navigationTitle("Modifica dashboard").navigationBarTitleDisplayMode(.inline)
             .onAppear { if selectedID == nil, let id = dashboard.widgets.first?.id { select(id) } }
        } else {
            ContentUnavailableView("Dashboard rimossa", systemImage: "rectangle.slash", description: Text("Torna all’elenco per scegliere un’altra dashboard."))
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
