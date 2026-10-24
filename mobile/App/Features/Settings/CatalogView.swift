import SwiftUI

struct CatalogView: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        List {
            Section {
                Text("Catalogo Service 01 dichiarato dal simulatore. Non rappresenta tutti i PID OBD né prova compatibilità con la tua auto; la discovery reale è ancora da integrare.").foregroundStyle(.secondary)
            }
            ForEach(model.catalog) { pid in
                VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                    Text(pid.nameIT).font(.headline)
                    Text("Service 01 · PID \(String(pid.pid, radix: 16).uppercased()) · \(pid.ecuSource)").font(.footnote).monospaced().foregroundStyle(.secondary)
                    if !pid.unit.isEmpty { Text(pid.unit).foregroundStyle(AppTheme.accent) }
                }.padding(.vertical, AppTheme.smallSpacing).accessibilityElement(children: .combine)
            }
        }.navigationTitle("Catalogo misure")
    }
}
