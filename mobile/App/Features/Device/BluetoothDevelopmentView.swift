import SwiftUI

struct BluetoothDevelopmentView: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        @Bindable var bluetooth = model.nativeBluetooth
        List {
            Section {
                Label("Bluetooth reale · sviluppo", systemImage: "antenna.radiowaves.left.and.right").font(.headline)
                Text("Questa sezione è separata dal gauge simulato. La radio si attiva soltanto quando premi Avvia ricerca. Nessuna scansione o connessione automatica.")
                Toggle("Abilita strumenti Bluetooth di sviluppo", isOn: $bluetooth.developmentEnabled)
                    .onChange(of: bluetooth.developmentEnabled) { _, enabled in if !enabled { bluetooth.stop() } }
                Text(bluetooth.notice).foregroundStyle(.secondary)
            }
            Section {
                AppActionButton(title: "Avvia ricerca · 10 secondi", symbol: "magnifyingglass", disabled: !bluetooth.developmentEnabled || bluetooth.isScanning || bluetooth.isConnecting || bluetooth.isReady, action: bluetooth.beginScan)
                AppActionButton(title: "Arresta ricerca e disconnetti", symbol: "stop.circle", action: bluetooth.stop)
                if bluetooth.isScanning { ProgressView("Ricerca limitata in corso") }
                if bluetooth.isConnecting { ProgressView("Connessione e indicazioni in corso") }
            }
            Section("Dispositivi trovati · massimo 20") {
                if bluetooth.candidates.isEmpty {
                    ContentUnavailableView("Nessun dispositivo selezionato", systemImage: "antenna.radiowaves.left.and.right.slash", description: Text("Avvia la ricerca solo per un gauge di sviluppo con il servizio previsto. L’app non si collega da sola."))
                }
                ForEach(bluetooth.candidates) { candidate in
                    Button { bluetooth.connect(candidate.id) } label: {
                        VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                            Label(candidate.name, systemImage: "circle.hexagongrid")
                            Text(candidate.id.uuidString).font(.caption).monospaced().foregroundStyle(.secondary)
                        }.frame(maxWidth: .infinity, minHeight: AppTheme.minimumTouch, alignment: .leading)
                    }.disabled(bluetooth.isConnecting || bluetooth.isReady)
                        .accessibilityHint("Collega soltanto questo dispositivo di sviluppo")
                }
            }
            Section("Verifica del solo protocollo") {
                AppActionButton(title: "Invia hello di sviluppo", symbol: "waveform.path", disabled: !bluetooth.isReady || bluetooth.rpcBusy, action: { Task { await bluetooth.probeHello() } })
                Text("Hello e conferme ATT non provano autenticazione, cifratura LESC/MITM, chiave a 128 bit o ammissione locale. iOS può chiedere un codice mostrato dal gauge; l’app non fornisce codici fissi.")
                Label("Configurazione, log e mutazioni reali bloccati", systemImage: "lock.shield").foregroundStyle(AppTheme.warning)
                Text("Il server radio e i suoi controlli di sicurezza devono essere qualificati prima di attivare quelle funzioni. Un solo collegamento attivo; altri telefoni potranno essere usati in sequenza senza reset.").foregroundStyle(.secondary)
            }
        }.navigationTitle("Bluetooth sviluppo")
            .onDisappear { bluetooth.stop() }
    }
}
