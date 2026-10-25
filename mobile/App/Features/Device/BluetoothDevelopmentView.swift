import SwiftUI

struct BluetoothDevelopmentView: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        let bluetooth = model.nativeBluetooth
        List {
            Section {
                Text("Accendi Auto Gauge e tienilo vicino al telefono.")
                if bluetooth.isScanning { ProgressView("Cerco il tuo dispositivo…") }
                else if bluetooth.isConnecting { ProgressView("Collegamento in corso…") }
                else { Text(bluetooth.connectionStatusText).foregroundStyle(.secondary) }
                Button("Cerca dispositivo", systemImage: "magnifyingglass") {
                    bluetooth.developmentEnabled = true
                    bluetooth.beginScan()
                }.disabled(bluetooth.isScanning || bluetooth.isConnecting || bluetooth.isReady)
                if bluetooth.isScanning || bluetooth.isConnecting {
                    Button("Annulla", role: .cancel, action: bluetooth.stop)
                }
            }
            Section("Dispositivi vicini") {
                if bluetooth.candidates.isEmpty && !bluetooth.isScanning {
                    Text("Nessun dispositivo trovato. Controlla che sia acceso e riprova.").foregroundStyle(.secondary)
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
                    Button("Usa questo dispositivo", systemImage: "checkmark.circle") {
                        Task { await model.activateNativeGauge() }
                    }.disabled(model.gaugeBusy || bluetooth.rpcBusy)
                    if model.isNativeSession { Label("Dispositivo collegato", systemImage: "checkmark.circle.fill") }
                }
            }
        }
        .navigationTitle("Collega dispositivo")
        .onDisappear { if !model.isNativeSession { bluetooth.stop() } }
    }
}
