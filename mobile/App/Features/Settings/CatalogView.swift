import SwiftUI
import GaugeCore

struct CatalogView: View {
    @Environment(AppModel.self) private var model
    @State private var searchText = ""
    private var availableMeasures: [PIDDescriptor] { model.catalog.filter(\.supported) }
    private var filteredMeasures: [PIDDescriptor] {
        let query = searchText.trimmingCharacters(in: .whitespacesAndNewlines)
        return availableMeasures.filter { query.isEmpty || $0.nameIT.localizedStandardContains(query) }
    }
    private func symbol(for pid: Int) -> String {
        switch pid { case 12: "gauge.with.needle"; case 13: "speedometer"; case 5, 15: "thermometer.medium"; default: "chart.bar" }
    }
    var body: some View {
        Group {
            if !model.isDemoMode && !model.isNativeSession {
                ContentUnavailableView("Collega il dispositivo", systemImage: "link", description: Text("Collega Auto Gauge per vedere le misure disponibili per la tua auto."))
            } else {
                List {
                    if availableMeasures.isEmpty {
                        ContentUnavailableView("Nessuna misura disponibile", systemImage: "gauge.with.needle", description: Text("Collega Auto Gauge per conoscere le informazioni disponibili."))
                    } else if filteredMeasures.isEmpty {
                        ContentUnavailableView.search
                    } else {
                        Section {
                            ForEach(filteredMeasures) { measure in
                                LabeledContent {
                                    Text(measure.unit).foregroundStyle(.secondary)
                                } label: {
                                    Label(measure.nameIT, systemImage: symbol(for: measure.pid))
                                }
                                .frame(minHeight: AppTheme.minimumTouch).accessibilityElement(children: .combine)
                            }
                        } header: { Text("\(availableMeasures.count) misure disponibili") }
                        Section {
                            NavigationLink(value: DeviceRoute.screens) { Label("Personalizza schermate", systemImage: "slider.horizontal.3") }
                        } footer: { Text("Scegli in Schermate quali informazioni vedere e come disporle sul display.") }
                    }
                }
                .searchable(text: $searchText, placement: .navigationBarDrawer(displayMode: .automatic), prompt: "Cerca una misura")
            }
        }
        .navigationTitle("Misure").navigationBarTitleDisplayMode(.inline)
    }
}
