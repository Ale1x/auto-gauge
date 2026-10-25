import SwiftUI
import GaugeCore

struct CatalogView: View {
    @Environment(AppModel.self) private var model
    @State private var searchText = ""

    private var availableMeasures: [PIDDescriptor] {
        model.catalog.filter { $0.supported }
    }
    private var filteredMeasures: [PIDDescriptor] {
        let query = searchText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !query.isEmpty else { return availableMeasures }
        return availableMeasures.filter { $0.nameIT.localizedStandardContains(query) }
    }

    var body: some View {
        Group {
            if !model.isDemoMode && !model.isNativeSession {
                ContentUnavailableView("Collega il dispositivo", systemImage: "link",
                                       description: Text("Collega Auto Gauge dalla scheda Dispositivo per vedere le misure disponibili per la tua auto."))
            } else {
                List {
                    if availableMeasures.isEmpty {
                        ContentUnavailableView("Nessuna misura disponibile", systemImage: "gauge.with.needle",
                                               description: Text("Collega Auto Gauge per vedere le misure disponibili per la tua auto."))
                    } else if filteredMeasures.isEmpty {
                        ContentUnavailableView.search
                    } else {
                        Section {
                            ForEach(filteredMeasures) { measure in
                                LabeledContent {
                                    if !measure.unit.isEmpty {
                                        Text(measure.unit).foregroundStyle(AppTheme.accent)
                                    }
                                } label: {
                                    Text(measure.nameIT)
                                }
                                .frame(minHeight: AppTheme.minimumTouch)
                                .accessibilityElement(children: .combine)
                            }
                        } header: {
                            Text("Misure disponibili")
                        } footer: {
                            Text("Puoi aggiungere queste misure alle schermate del display.")
                        }
                    }
                }
                .searchable(text: $searchText, prompt: "Cerca una misura")
            }
        }
        .navigationTitle("Misure")
    }
}
