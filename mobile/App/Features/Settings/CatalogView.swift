import SwiftUI
import GaugeCore

struct CatalogView: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        List {
            Section {
                Text("Catalogo Service 01 dichiarato dal simulatore. Non rappresenta tutti i PID OBD né prova compatibilità con la tua auto; la discovery reale è ancora da integrare.").foregroundStyle(.secondary)
            }
            Section("Dichiarati dal gauge simulato") {
            ForEach(model.catalog) { pid in
                VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                    Text(pid.nameIT).font(.headline)
                    Text("Service 01 · PID \(String(pid.pid, radix: 16).uppercased()) · \(pid.ecuSource)").font(.footnote).monospaced().foregroundStyle(.secondary)
                    if !pid.unit.isEmpty { Text(pid.unit).foregroundStyle(AppTheme.accent) }
                }.padding(.vertical, AppTheme.smallSpacing).accessibilityElement(children: .combine)
            }
            }
            Section {
                Text("53 formule scalari disponibili nell’app per analizzare gli storici. Questo elenco non dichiara che il gauge o la tua ECU supportino tutte queste misure. Le formule non aggiungono misure all’editor.").foregroundStyle(.secondary)
            } header: { Text("Formule per gli storici") }
            ForEach(HistoryPIDDefinition.qualified) { definition in
                VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                    Text(definition.nameIT).font(.headline)
                    Text("Service 01 · PID \(String(definition.id, radix: 16).uppercased()) · \(definition.unit)").font(.footnote).foregroundStyle(.secondary)
                    Text("Dati raw: \(definition.byteCount) byte, escluso header").font(.footnote).foregroundStyle(.secondary)
                }.padding(.vertical, AppTheme.smallSpacing).accessibilityElement(children: .combine)
            }
        }.navigationTitle("Catalogo misure")
    }
}
