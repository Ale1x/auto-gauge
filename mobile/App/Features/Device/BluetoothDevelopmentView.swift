import SwiftUI

struct BluetoothDevelopmentView: View {
    @Environment(AppModel.self) private var model
    @State private var searched = false
    var body: some View {
        let bluetooth = model.nativeBluetooth
        List {
            Section {
                Label("Colleghiamo il tuo Auto Gauge", systemImage: "link.circle.fill").font(.headline)
                Text("Accendi il dispositivo e tienilo vicino al telefono. Avvia la ricerca quando sei pronto.")
                    .font(.subheadline).foregroundStyle(.secondary)
                if bluetooth.isScanning { ProgressView("Cerco il tuo dispositivo…") }
                else if bluetooth.isConnecting { ProgressView("Collegamento in corso…") }
                else if searched || bluetooth.isReady { Text(bluetooth.connectionStatusText).font(.subheadline).foregroundStyle(.secondary) }
                Button("Cerca dispositivo", systemImage: "magnifyingglass") {
                    searched = true
                    bluetooth.developmentEnabled = true; bluetooth.beginScan()
                }
                .frame(minHeight: AppTheme.minimumTouch)
                .disabled(bluetooth.isScanning || bluetooth.isConnecting || bluetooth.isReady)
                if bluetooth.isScanning || bluetooth.isConnecting { Button("Annulla", role: .cancel, action: bluetooth.stop) }
            }
            Section("Dispositivi vicini") {
                if bluetooth.candidates.isEmpty && !bluetooth.isScanning {
                    Text(searched ? "Nessun dispositivo trovato. Controlla che sia acceso e riprova." : "Il tuo Auto Gauge comparirà qui.").foregroundStyle(.secondary)
                }
                ForEach(bluetooth.candidates) { candidate in
                    Button { bluetooth.connect(candidate.id) } label: {
                        Label(candidate.name, systemImage: "circle.hexagongrid")
                            .frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch, alignment: .leading)
                    }.disabled(bluetooth.isConnecting || bluetooth.isReady)
                }
            }
            if bluetooth.isReady {
                Section {
                    Button("Usa questo dispositivo", systemImage: "checkmark.circle") { Task { await model.activateNativeGauge() } }
                        .frame(minHeight: AppTheme.minimumTouch).disabled(model.gaugeBusy || bluetooth.rpcBusy)
                    if model.isNativeSession { Label("Dispositivo collegato", systemImage: "checkmark.circle.fill") }
                }
            }
        }
        .navigationTitle("Collega dispositivo").navigationBarTitleDisplayMode(.inline)
        .onDisappear { if !model.isNativeSession { bluetooth.stop() } }
    }
}
