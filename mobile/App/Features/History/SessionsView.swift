import SwiftUI

struct SessionsView: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        List {
            Section {
                Label("Storici dal gauge simulato", systemImage: "tray.and.arrow.down").font(.headline)
                Text("Le pagine passano attraverso il trasporto in memoria e vengono salvate su questo telefono prima della conferma al simulatore. Nessuna radio, lettura dal veicolo o telemetria live.")
                    .foregroundStyle(.secondary)
            }
            if model.logBusy { Section { ProgressView("Importazione locale in corso…") } }
            if model.logSessions.isEmpty && !model.logBusy {
                Section {
                    ContentUnavailableView("Nessuno storico importato", systemImage: "clock.arrow.circlepath", description: Text(model.logReady ? "Collega il gauge simulato e importa le sue pagine di esempio." : "Ricarica correttamente l’archivio locale per continuare."))
                }
            }
            Section("Sessioni su questo telefono") {
                ForEach(model.logSessions) { session in
                    NavigationLink(value: HistoryRoute.session(session.id)) { HistoricalSessionRow(session: session) }
                }
            }
            Section {
                AppActionButton(title: "Importa dal gauge simulato", symbol: "arrow.down.doc", disabled: !model.logReady || model.logBusy || !model.gaugeConnected || model.gaugeBusy, action: sync)
                AppActionButton(title: "Ricarica archivio sul telefono", symbol: "arrow.clockwise", disabled: model.logBusy, action: reload)
                AppActionButton(title: "Importa fixture locali aggiuntive", symbol: "testtube.2", disabled: !model.logReady || model.logBusy, action: importFixtures)
                if let acknowledgement = model.lastLogAcknowledgement {
                    Label("Salvataggio locale confermato", systemImage: "checkmark.circle").foregroundStyle(AppTheme.accent)
                    Text(acknowledgement).font(.footnote).monospacedDigit()
                }
            } footer: {
                Text("Ogni telefono conserva il proprio cursore. Le conferme non cancellano il buffer condiviso del gauge. Ripetere l’import non duplica i record; ogni azione elabora un numero limitato di pagine e può essere ripetuta per continuare.")
            }
            Section {
                Text("Archivio dimostrativo limitato: a capacità piena le nuove pagine vengono rifiutate senza eliminare record già importati.").foregroundStyle(.secondary)
            }
        }.navigationTitle("Storici")
    }
    private func sync() { Task { await model.syncSimulatedHistory() } }
    private func reload() { Task { await model.reloadLogs() } }
    private func importFixtures() { Task { await model.importDemoLogs() } }
}
