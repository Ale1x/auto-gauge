import SwiftUI
import Observation
import GaugeCore

@MainActor
@Observable
final class AppModel {
    let cloud = CloudAccountModel()
    let firmwareTrust: OTAPreparationTrust?
    let firmwareInstallation: OTAInstallationController
    let nativeBluetooth = NativeBluetoothController()
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
    var gaugeCommitState: GaugeCommitState = .idle
    var gaugeNotice = "Nessun collegamento simulato attivo."
    private var started = false
    private var logArchive: LocalLogArchive
    private let nativeLogArchive: LocalLogArchive
    private var storage: AtomicFileConfigurationStorage
    private var binding: GaugeSessionBinding
    private let simulatorBinding: GaugeSessionBinding
    private let directory: URL
    private let nativeQualificationProvider: any GaugeNativeQualificationProvider
    private var sessionEpoch = 0
    private var nativePipeGeneration: UInt64?
    private var changingSession = false
    private var activeDraftKey = "simulation"
    private var retainedDrafts: [String: ConfigurationDraft] = [:]
    private var historySourceCursor = 0
    var gaugeDeviceID: String?
    var isNativeSession: Bool { binding.isNative }
    var sourceTitle: String { isNativeSession ? "Gauge selezionato" : "Gauge simulato" }
    private var transport: any GaugeSessionTransport { binding.transport }
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
    init(nativeQualificationProvider: any GaugeNativeQualificationProvider = UnqualifiedNativeGaugeProvider(),
         firmwareTrust: OTAPreparationTrust? = nil,
         otaInstallationProvider: (any OTAInstallationSessionProvider)? = nil) {
        self.firmwareTrust = firmwareTrust
        firmwareInstallation = OTAInstallationController(provider: otaInstallationProvider)
        self.nativeQualificationProvider = nativeQualificationProvider
        directory = URL.applicationSupportDirectory.appending(path: "AutoGauge")
        storage = .init(fileURL: directory.appending(path: "simulated-configuration-v1.json"))
        nativeLogArchive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: directory.appending(path: "native-histories-v1.json")))
        logArchive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: directory.appending(path: "simulated-histories-v1.json")))
        do {
            editor = try ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
            let fixture = try DemoGaugeFixture.load()
            let transport = DevelopmentGaugeTransport(pipe: InMemoryGaugePacketPipe(channel: try InMemoryGaugeEndpoint(configuration: fixture.configuration, historySource: fixture.historySource), clientID: "IPHONE-SIM-A"))
            simulatorBinding = .simulator(transport, sessions: [fixture.historySource.sessionID])
            binding = simulatorBinding
        } catch { fatalError("Fixture dimostrative incorporate non valide: \(error)") }
        do {
            if let data = try storage.read() {
                editor = try ConfigurationDraft(configuration: JSONDecoder().decode(GaugeConfiguration.self, from: data), catalog: InMemoryGaugeEndpoint.catalog)
            } else if let legacy = UserDefaults.standard.data(forKey: "auto-gauge.simulated-configuration.v1") {
                let migrated = try ConfigurationDraft(configuration: JSONDecoder().decode(GaugeConfiguration.self, from: legacy), catalog: InMemoryGaugeEndpoint.catalog)
                try storage.write(legacy); editor = migrated
            }
        } catch { message = "Bozza locale non leggibile: è stato caricato l’esempio senza sovrascrivere il salvataggio. \(error.localizedDescription)" }
        nativeBluetooth.onSessionInvalidated = { [weak self] generation in Task { await self?.nativeSessionEnded(generation: generation) } }
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
        // Recover durable native intents even while the simulator is selected
        // and without requiring the gauge or an iCloud account to be present.
        do {
            try await nativeLogArchive.reload()
            try await cloud.recoverQualifiedImports(from: nativeLogArchive)
        } catch { message = "Storici reali conservati; recupero della coda iCloud da ripetere: \(error.localizedDescription)" }
        await reloadLogs()
        await connectGauge()
        if logReady && logSessions.isEmpty { await syncSimulatedHistory() }
    }
    func connectGauge() async {
        guard !gaugeBusy else { return }
        if isNativeSession { gaugeNotice = "Seleziona e qualifica una nuova connessione nella sezione Bluetooth."; return }
        let token = sessionEpoch; let transport = self.transport
        gaugeBusy = true; defer { if token == sessionEpoch { gaugeBusy = false } }
        do {
            try await transport.connect()
            let nextCatalog = try await transport.supportedPIDs()
            let snapshot = try await transport.configuration()
            let id = try await transport.deviceIdentifier()
            guard token == sessionEpoch else { return }
            catalog = nextCatalog; gaugeSnapshot = snapshot; gaugeDeviceID = id
            gaugeConnected = true; gaugeNeedsRead = false
            gaugeCommitState = await transport.commitState()
            guard token == sessionEpoch else { return }
            gaugeNotice = "Collegato al simulatore locale. La bozza del telefono è conservata."
        } catch { await refreshLinkAfterError(error, transport: transport, token: token) }
    }
    func activateNativeGauge() async {
        guard !gaugeBusy, !logBusy, !changingSession else { return }
        let token = sessionEpoch; gaugeBusy = true
        defer { if token == sessionEpoch { gaugeBusy = false } }
        var candidate: QualifiedGaugeTransport?
        do {
            let pipe = try await nativeBluetooth.qualifiedPacketPipe()
            let pipeGeneration = try await pipe.readyConnection().generation
            let native = QualifiedGaugeTransport(pipe: pipe, qualificationProvider: nativeQualificationProvider)
            candidate = native
            try await native.connect()
            let nextCatalog = try await native.supportedPIDs()
            let snapshot = try await native.configuration()
            let id = try await native.deviceIdentifier()
            guard token == sessionEpoch else { await native.disconnect(); return }
            let key = "native-" + CloudHistoryItem.digest(Data(id.utf8))
            let nextStorage = AtomicFileConfigurationStorage(fileURL: directory.appending(path: key + "/configuration-v1.json"))
            let nextEditor: ConfigurationDraft
            if let retained = retainedDrafts[key] { nextEditor = retained }
            else if let bytes = try nextStorage.read() {
                nextEditor = try ConfigurationDraft(configuration: JSONDecoder().decode(GaugeConfiguration.self, from: bytes), catalog: nextCatalog)
            } else { nextEditor = try ConfigurationDraft(configuration: snapshot, catalog: nextCatalog) }
            changingSession = true
            retainedDrafts[activeDraftKey] = editor
            await transport.disconnect()
            guard token == sessionEpoch, nativeBluetooth.isCurrentConnection(pipeGeneration) else { changingSession = false; await native.disconnect(); return }
            sessionEpoch += 1; let installed = sessionEpoch
            binding = .qualifiedNative(native); nativePipeGeneration = pipeGeneration; storage = nextStorage; activeDraftKey = key; editor = nextEditor
            catalog = nextCatalog; gaugeSnapshot = snapshot; gaugeDeviceID = id
            gaugeConnected = true; gaugeNeedsRead = false; gaugeCommitState = .idle
            logArchive = nativeLogArchive
            logSessions = []; logReady = false; lastLogAcknowledgement = nil; historySourceCursor = 0
            changingSession = false; gaugeBusy = false
            gaugeNotice = "Sessione locale qualificata per il gauge selezionato. Bozze e storici dimostrativi restano separati."
            await reloadLogs()
            guard installed == sessionEpoch else { return }
        } catch {
            changingSession = false
            await candidate?.disconnect()
            guard token == sessionEpoch else { return }
            gaugeNotice = error.localizedDescription; message = error.localizedDescription
        }
    }
    func useSimulator() async {
        guard !gaugeBusy, !logBusy, !changingSession else { return }
        changingSession = true; sessionEpoch += 1
        retainedDrafts[activeDraftKey] = editor
        await transport.disconnect()
        binding = simulatorBinding; nativePipeGeneration = nil; activeDraftKey = "simulation"
        storage = .init(fileURL: directory.appending(path: "simulated-configuration-v1.json"))
        if let retained = retainedDrafts[activeDraftKey] { editor = retained }
        catalog = DemoData.catalog; gaugeSnapshot = nil; gaugeDeviceID = nil
        gaugeConnected = false; gaugeNeedsRead = true; gaugeCommitState = .idle
        logArchive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: directory.appending(path: "simulated-histories-v1.json")))
        logSessions = []; logReady = false; lastLogAcknowledgement = nil; historySourceCursor = 0
        changingSession = false
        await reloadLogs(); await connectGauge()
    }
    private func nativeSessionEnded(generation: UInt64) async {
        guard isNativeSession, nativePipeGeneration == generation, !changingSession else { return }
        nativePipeGeneration = nil; sessionEpoch += 1; let token = sessionEpoch
        gaugeConnected = false; gaugeNeedsRead = true
        gaugeBusy = false; logBusy = false
        let transport = self.transport
        await transport.disconnect()
        let state = await transport.commitState()
        guard token == sessionEpoch else { return }
        gaugeCommitState = state
        gaugeNotice = "Gauge disconnesso. Bozza e archivio locale conservati; riconnetti e rileggi prima di applicare."
    }
    func disconnectGauge() async {
        sessionEpoch += 1; let token = sessionEpoch; let transport = self.transport
        gaugeConnected = false; gaugeNeedsRead = true; gaugeBusy = false
        await transport.disconnect()
        guard token == sessionEpoch else { return }
        gaugeNotice = "Sessione scollegata. Un altro telefono può essere usato senza reset."
    }
    func readGauge() async {
        guard gaugeConnected, !gaugeBusy else { return }
        let token = sessionEpoch; let transport = self.transport
        gaugeBusy = true; defer { if token == sessionEpoch { gaugeBusy = false } }
        do {
            let nextCatalog = try await transport.supportedPIDs()
            let snapshot = try await transport.configuration()
            guard token == sessionEpoch else { return }
            catalog = nextCatalog; gaugeSnapshot = snapshot; gaugeNeedsRead = false
            gaugeCommitState = await transport.commitState()
            guard token == sessionEpoch else { return }
            gaugeNotice = "Snapshot aggiornato dal dispositivo selezionato; bozza locale conservata."
        } catch { await refreshLinkAfterError(error, transport: transport, token: token) }
    }
    func useGaugeSnapshot() {
        guard let gaugeSnapshot, !gaugeNeedsRead else { return }
        edit { try $0.restore(gaugeSnapshot, catalog: catalog) }
        gaugeNotice = "Snapshot copiato nella bozza. Il salvataggio sul telefono resta separato."
    }
    func validateOnGauge() async {
        guard gaugeConnected, !gaugeBusy else { return }
        let token = sessionEpoch; let transport = self.transport
        gaugeBusy = true; defer { if token == sessionEpoch { gaugeBusy = false } }
        do {
            try await transport.validateDraft(editor.working)
            guard token == sessionEpoch else { return }
            gaugeNotice = "Bozza validata; nessuna applicazione e nessun salvataggio eseguiti."
        } catch { await refreshLinkAfterError(error, transport: transport, token: token) }
    }
    func applyToGauge() async {
        guard canApplyToGauge else { return }
        let token = sessionEpoch; let transport = self.transport
        gaugeBusy = true; defer { if token == sessionEpoch { gaugeBusy = false } }
        do {
            let snapshot = try await transport.applyDraft(editor.working, onPhase: { [weak self] phase in
                await self?.receiveGaugePhase(phase, token: token)
            })
            guard token == sessionEpoch else { return }
            gaugeSnapshot = snapshot; gaugeNeedsRead = false
            gaugeNotice = isNativeSession ? "Applicazione confermata dal gauge. La bozza del telefono non è stata salvata automaticamente." : "Applicazione confermata in RAM simulata. La bozza del telefono non è stata salvata automaticamente."
        } catch { await refreshLinkAfterError(error, transport: transport, token: token) }
    }
    private func receiveGaugePhase(_ phase: GaugeCommitState, token: Int) { if token == sessionEpoch { gaugeCommitState = phase } }
    private func refreshLinkAfterError(_ error: Error, transport: any GaugeSessionTransport, token: Int) async {
        let connected = await transport.isConnected(); let needsRead = await transport.requiresRead(); let state = await transport.commitState()
        guard token == sessionEpoch else { return }
        gaugeConnected = connected; gaugeNeedsRead = needsRead; gaugeCommitState = state
        gaugeNotice = error.localizedDescription; message = error.localizedDescription
    }
    func reloadLogs() async {
        guard !logBusy else { return }
        let token = sessionEpoch; let archive = logArchive
        logBusy = true; defer { if token == sessionEpoch { logBusy = false } }
        do {
            try await archive.reload()
            do { try await cloud.recoverQualifiedImports(from: archive) }
            catch { if token == sessionEpoch { message = "Storico locale conservato; recupero della coda iCloud da ripetere: \(error.localizedDescription)" } }
            let sessions = await archive.sessions(); let ready = await archive.isReady()
            guard token == sessionEpoch else { return }
            logSessions = sessions; logReady = ready
        } catch {
            guard token == sessionEpoch else { return }
            logReady = false; message = "Ricarica storico non riuscita. Nessun dato sovrascritto e nessun nuovo ACK. \(error.localizedDescription)"
        }
    }
    func syncSimulatedHistory() async { await syncGaugeHistory() }
    func syncGaugeHistory() async {
        guard logReady, !logBusy, gaugeConnected, !gaugeBusy else { return }
        let token = sessionEpoch; let binding = self.binding; let transport = binding.transport; let archive = logArchive
        logBusy = true; gaugeBusy = true
        defer { if token == sessionEpoch { logBusy = false; gaugeBusy = false } }
        do {
            let sources = try await binding.historySessionIDs()
            guard token == sessionEpoch else { return }
            guard !sources.isEmpty else { lastLogAcknowledgement = "Nessuna sessione dichiarata dal gauge."; return }
            for _ in 0..<4 {
                let source = sources[historySourceCursor % sources.count]; historySourceCursor = (historySourceCursor + 1) % sources.count
                let imported = try await transport.importHistoryPage(sessionID: source, archive: archive, limit: 16)
                // Transfer the archive's durable intents, including earlier
                // interrupted imports, independently of the current UI lease.
                if imported.qualifiedImport != nil {
                    do { try await cloud.recoverQualifiedImports(from: archive) }
                    catch { if token == sessionEpoch { message = "Storico locale conservato; coda iCloud da recuperare: \(error.localizedDescription)" } }
                }
                guard token == sessionEpoch else { return }
                logSessions = await archive.sessions()
                guard token == sessionEpoch else { return }
                lastLogAcknowledgement = "Cursore locale \(imported.acknowledgement.acknowledgedThrough.decimal) · \(imported.peerAcknowledged ? "conferma ricevuta" : "conferma non recapitata")"
                if let warning = imported.warning { message = warning; break }
            }
        } catch {
            let sessions = await archive.sessions()
            guard token == sessionEpoch else { return }
            logSessions = sessions
            await refreshLinkAfterError(error, transport: transport, token: token)
        }
    }
    func importDemoLogs() async {
        guard !isNativeSession, logReady, !logBusy else { return }
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
    func addDashboard() {
        guard let pid = catalog.first else { message = "Nessuna misura disponibile per una nuova dashboard."; return }
        edit { _ = try $0.addDashboard(pid: pid) }
    }
}
