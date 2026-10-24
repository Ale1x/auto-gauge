import SwiftUI
import Observation
import GaugeCore

@MainActor
@Observable
final class AppModel {
    let cloud = CloudAccountModel()
    var editor: ConfigurationDraft
    var message: String?
    var logSessions: [ImportedLogSession] = []
    var logReady = false
    var logBusy = false
    var lastLogAcknowledgement: String?
    var catalog = DemoData.catalog
    var gaugeConnected = false
    var gaugeBusy = false
    var gaugeNeedsRead = true
    var gaugeSnapshot: GaugeConfiguration?
    var gaugeCommitState: SimulatedCommitState = .idle
    var gaugeNotice = "Nessun collegamento simulato attivo."
    private var started = false
    private let logArchive: LocalLogArchive
    private let storage: AtomicFileConfigurationStorage
    private let transport: SimulatedGaugeTransport
    private let historySession: LogSessionID
    var configuration: GaugeConfiguration {
        get { editor.working }
        set { editor.working = newValue }
    }
    var savedRevision: Int { editor.saved.revision }
    var canApplyToGauge: Bool { gaugeConnected && !gaugeNeedsRead && gaugeSnapshot != nil && !gaugeBusy }
    var manualBrightness: Double {
        get { Double(editor.working.brightness.manualPercent) }
        set { editor.working.brightness.manualPercent = Int(newValue.rounded()) }
    }
    var alertPresented: Bool {
        get { message != nil }
        set { if !newValue { message = nil } }
    }
    init() {
        let directory = URL.applicationSupportDirectory.appending(path: "AutoGauge")
        storage = .init(fileURL: directory.appending(path: "simulated-configuration-v1.json"))
        logArchive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: directory.appending(path: "simulated-histories-v1.json")))
        do {
            editor = try ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
            let fixture = try DemoGaugeFixture.load()
            transport = SimulatedGaugeTransport(channel: try InMemoryGaugeEndpoint(configuration: fixture.configuration, historySource: fixture.historySource), clientID: "IPHONE-SIM-A")
            historySession = fixture.historySource.sessionID
        } catch { fatalError("Fixture dimostrative incorporate non valide: \(error)") }
        do {
            if let data = try storage.read() {
                editor = try ConfigurationDraft(configuration: JSONDecoder().decode(GaugeConfiguration.self, from: data), catalog: InMemoryGaugeEndpoint.catalog)
            } else if let legacy = UserDefaults.standard.data(forKey: "auto-gauge.simulated-configuration.v1") {
                let migrated = try ConfigurationDraft(configuration: JSONDecoder().decode(GaugeConfiguration.self, from: legacy), catalog: InMemoryGaugeEndpoint.catalog)
                try storage.write(legacy); editor = migrated
            }
        } catch { message = "Bozza locale non leggibile: è stato caricato l’esempio senza sovrascrivere il salvataggio. \(error.localizedDescription)" }
    }
    subscript(geometryUnitFor dashboardID: Int, widget widgetID: Int, field field: GeometryField) -> Int {
        get {
            guard let frame = editor.working.dashboards.first(where: { $0.id == dashboardID })?.widgets.first(where: { $0.id == widgetID })?.frame,
                  let rectangle = try? EditingRectangle(frame) else { return 0 }
            return rectangle[field]
        }
        set { edit { try $0.setGeometry(dashboardID: dashboardID, widgetID: widgetID, field: field, units: newValue) } }
    }
    subscript(geometryValueFor dashboardID: Int, widget widgetID: Int, field field: GeometryField) -> Double {
        get { Double(self[geometryUnitFor: dashboardID, widget: widgetID, field: field]) }
        set { self[geometryUnitFor: dashboardID, widget: widgetID, field: field] = Int(newValue.rounded()) }
    }
    func bootstrap() async {
        guard !started else { return }
        started = true
        await reloadLogs()
        await connectGauge()
        if logReady && logSessions.isEmpty { await syncSimulatedHistory() }
    }
    func connectGauge() async {
        guard !gaugeBusy else { return }
        gaugeBusy = true
        defer { gaugeBusy = false }
        do {
            try await transport.connect()
            catalog = try await transport.supportedPIDs()
            gaugeSnapshot = try await transport.configuration()
            gaugeConnected = true; gaugeNeedsRead = false
            gaugeCommitState = await transport.commitState()
            gaugeNotice = "Collegato al simulatore locale. Lo stato letto non modifica la bozza del telefono."
        } catch { await refreshLinkAfterError(error) }
    }
    func disconnectGauge() async {
        await transport.disconnect()
        gaugeConnected = false; gaugeNeedsRead = true
        gaugeNotice = "Simulatore scollegato. Un altro client può collegarsi senza reset."
    }
    func readGauge() async {
        guard gaugeConnected, !gaugeBusy else { return }
        gaugeBusy = true
        defer { gaugeBusy = false }
        do {
            catalog = try await transport.supportedPIDs()
            gaugeSnapshot = try await transport.configuration()
            gaugeNeedsRead = false
            gaugeCommitState = await transport.commitState()
            gaugeNotice = "Snapshot aggiornato dal simulatore; bozza locale conservata."
        } catch { await refreshLinkAfterError(error) }
    }
    func useGaugeSnapshot() {
        guard let gaugeSnapshot, !gaugeNeedsRead else { return }
        edit { try $0.restore(gaugeSnapshot, catalog: catalog) }
        gaugeNotice = "Contenuto dello snapshot copiato nella bozza. Il salvataggio sul telefono resta un’azione separata."
    }
    func validateOnGauge() async {
        guard gaugeConnected, !gaugeBusy else { return }
        gaugeBusy = true
        defer { gaugeBusy = false }
        do {
            try await transport.validateDraft(editor.working)
            gaugeNotice = "Bozza validata dal simulatore; nessuna applicazione e nessun salvataggio eseguiti."
        } catch { await refreshLinkAfterError(error) }
    }
    func applyToGauge() async {
        guard canApplyToGauge else { return }
        gaugeBusy = true
        defer { gaugeBusy = false }
        do {
            gaugeSnapshot = try await transport.applyDraft(editor.working, onPhase: { [weak self] phase in
                await self?.receiveGaugePhase(phase)
            })
            gaugeNeedsRead = false
            gaugeNotice = "Ricezione e applicazione confermate in RAM simulata. La bozza sul telefono non è stata salvata automaticamente."
        } catch { await refreshLinkAfterError(error) }
    }
    private func receiveGaugePhase(_ phase: SimulatedCommitState) { gaugeCommitState = phase }
    private func refreshLinkAfterError(_ error: Error) async {
        gaugeConnected = await transport.isConnected()
        gaugeNeedsRead = await transport.requiresRead()
        gaugeCommitState = await transport.commitState()
        gaugeNotice = error.localizedDescription
        message = error.localizedDescription
    }
    func reloadLogs() async {
        guard !logBusy else { return }
        logBusy = true
        defer { logBusy = false }
        do {
            try await logArchive.reload()
            logSessions = await logArchive.sessions(); logReady = true
        } catch {
            logReady = false
            message = "Ricarica storico non riuscita. Nessun dato sovrascritto e nessun nuovo ACK. \(error.localizedDescription)"
        }
    }
    func syncSimulatedHistory() async {
        guard logReady, !logBusy, gaugeConnected, !gaugeBusy else { return }
        logBusy = true; gaugeBusy = true
        defer { logBusy = false; gaugeBusy = false }
        do {
            // Bounded work per interaction; a subsequent tap can resume after the local cursor.
            for _ in 0..<4 {
                let before = await logArchive.cursor(deviceID: "SIMULATO-001", sessionID: historySession.value)
                let imported = try await transport.importHistoryPage(sessionID: historySession, archive: logArchive, limit: 2)
                logSessions = await logArchive.sessions()
                lastLogAcknowledgement = "Cursore locale \(imported.acknowledgement.acknowledgedThrough.decimal) · \(imported.peerAcknowledged ? "conferma ricevuta dal simulatore" : "conferma non recapitata")"
                if let warning = imported.warning { message = warning; break }
                if imported.acknowledgement.acknowledgedThrough.value == before { break }
            }
        } catch {
            logSessions = await logArchive.sessions()
            await refreshLinkAfterError(error)
        }
    }
    func importDemoLogs() async {
        guard logReady, !logBusy else { return }
        logBusy = true
        defer { logBusy = false }
        do {
            let demo = try DemoLogImport.load()
            for page in demo.pages {
                let ack = try await logArchive.importPage(deviceID: demo.deviceID, data: page)
                lastLogAcknowledgement = "Fixture locale · cursore \(ack.acknowledgedThrough.decimal) · nessun ACK al simulatore"
                logSessions = await logArchive.sessions()
            }
        } catch { message = "Pagina fixture rifiutata senza nuovo ACK. \(error.localizedDescription)" }
    }
    func edit(_ operation: (inout ConfigurationDraft) throws -> Void) {
        var candidate = editor
        do { try operation(&candidate); editor = candidate }
        catch { message = error.localizedDescription }
    }
    func save() async {
        do {
            try editor.save(to: storage, catalog: catalog)
            message = "Bozza salvata solo su questo iPhone, revisione locale \(editor.saved.revision). Nessuna applicazione al gauge."
        } catch { message = error.localizedDescription }
    }
    func cancelEdits() { editor.cancel() }
    func reloadSaved() {
        do { try editor.reload(from: storage, catalog: catalog) }
        catch { message = "Ricarica non riuscita: bozza corrente conservata. \(error.localizedDescription)" }
    }
    func restoreExample() { edit { try $0.restore(DemoData.configuration, catalog: catalog) } }
    func addDashboard() { edit { _ = try $0.addDashboard(pid: catalog[0]) } }
}
