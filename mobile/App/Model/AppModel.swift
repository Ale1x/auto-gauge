import SwiftUI
import Observation
import GaugeCore

@MainActor
@Observable
final class AppModel {
    let cloud = CloudAccountModel()
    private(set) var isDemoMode: Bool
    var appearance: AppAppearance { didSet { preferences.set(appearance.rawValue, forKey: "appearance") } }
    var backupEnabled: Bool { didSet { preferences.set(backupEnabled, forKey: isDemoMode ? "demo.backup.enabled" : "device.backup.enabled") } }
    private(set) var backupBusy = false
    private(set) var backupStatusText = "Tutto pronto"
    private(set) var lastBackupDate: Date?
    private(set) var isSaving = false
    private(set) var saveStatusText = "Tutte le modifiche sono salvate"
    private(set) var demoFirmwareState: DemoFirmwareState = .idle
    private(set) var demoFirmwareProgress = 0.0
    private(set) var firmwareCurrentVersion = "0.13.0"
    private(set) var firmwareAvailableVersion: String? = "0.14.0"
    private(set) var firmwareStatusText = "Controlla se ci sono aggiornamenti"
    private(set) var diagnosticDetail: String?
    private let preferences: UserDefaults
    private let demoStore: DemoExperienceStore
    private let demoLogArchive: LocalLogArchive
    private let demoStepDelay: Duration
    private var firmwareGeneration = 0
    private var firmwareWork: Task<Void, Never>?
    private var backupWork: Task<Void, Never>?
    private var backupGeneration = 0
    private var pendingDeviceUpdate = false
    var hasPendingChanges: Bool { editor.hasChanges || pendingDeviceUpdate }
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
    var gaugeNotice = "Collega il tuo Auto Gauge"
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
    var isNativeSession: Bool { !isDemoMode && binding.isNative }
    var sourceTitle: String { "Auto Gauge" }
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
         otaInstallationProvider: (any OTAInstallationSessionProvider)? = nil,
         applicationSupportDirectory: URL? = nil, preferences suppliedPreferences: UserDefaults? = nil,
         demoStepDelay: Duration = .milliseconds(75)) {
        let arguments = ProcessInfo.processInfo.arguments
        let testing = arguments.contains("--uitesting-reset") || arguments.contains("--uitesting")
        let selectedPreferences = suppliedPreferences ?? (testing ? UserDefaults(suiteName: "AutoGauge.UITests") : nil) ?? .standard
        let base = applicationSupportDirectory ?? URL.applicationSupportDirectory.appending(path: "AutoGauge")
        let selectedDirectory = testing && applicationSupportDirectory == nil ? base.appending(path: "uitests") : base
        if testing && arguments.contains("--uitesting-reset") {
            // This path is dedicated to UI tests; never remove normal user data.
            if applicationSupportDirectory == nil { try? FileManager.default.removeItem(at: selectedDirectory) }
            if suppliedPreferences == nil { selectedPreferences.removePersistentDomain(forName: "AutoGauge.UITests") }
        }
        preferences = selectedPreferences
        let initialDemoMode = selectedPreferences.object(forKey: "demo.enabled") == nil ? true : selectedPreferences.bool(forKey: "demo.enabled")
        isDemoMode = initialDemoMode
        appearance = AppAppearance(rawValue: selectedPreferences.string(forKey: "appearance") ?? "system") ?? .system
        backupEnabled = selectedPreferences.object(forKey: initialDemoMode ? "demo.backup.enabled" : "device.backup.enabled") == nil ? initialDemoMode : selectedPreferences.bool(forKey: initialDemoMode ? "demo.backup.enabled" : "device.backup.enabled")
        self.demoStepDelay = demoStepDelay
        self.firmwareTrust = firmwareTrust
        firmwareInstallation = OTAInstallationController(provider: otaInstallationProvider)
        self.nativeQualificationProvider = nativeQualificationProvider
        directory = selectedDirectory
        let demoDirectory = selectedDirectory.appending(path: "demo")
        demoStore = DemoExperienceStore(directory: demoDirectory)
        demoLogArchive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: demoDirectory.appending(path: "histories-v1.json")))
        nativeLogArchive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: selectedDirectory.appending(path: "native-histories-v1.json")))
        logArchive = initialDemoMode ? demoLogArchive : nativeLogArchive
        let demoStorage = AtomicFileConfigurationStorage(fileURL: demoDirectory.appending(path: "configuration-v1.json"))
        storage = demoStorage
        let demoCatalog = GaugeConfigurationDefaults.namedCatalog(InMemoryGaugeEndpoint.catalog)
        var loaded: ConfigurationDraft
        do { loaded = try ConfigurationDraft(configuration: DemoData.configuration, catalog: InMemoryGaugeEndpoint.catalog) }
        catch { fatalError("Configurazione iniziale non valida") }
        do {
            // The persisted configuration is loaded BEFORE the sample endpoint.
            if let data = try demoStorage.read() {
                loaded = try ConfigurationDraft(configuration: GaugeConfigurationDefaults.normalizedLabels(JSONDecoder().decode(GaugeConfiguration.self, from: data), catalog: demoCatalog), catalog: demoCatalog)
            } else if !testing, let data = try AtomicFileConfigurationStorage(fileURL: selectedDirectory.appending(path: "simulated-configuration-v1.json")).read() {
                loaded = try ConfigurationDraft(configuration: GaugeConfigurationDefaults.normalizedLabels(JSONDecoder().decode(GaugeConfiguration.self, from: data), catalog: demoCatalog), catalog: demoCatalog)
                try demoStorage.write(data)
            }
        } catch { diagnosticDetail = String(describing: error); message = "Non è stato possibile aprire le impostazioni salvate. La copia precedente è conservata." }
        if initialDemoMode {
            let updated = GaugeClusterLayout.upgradingDefaultDemo(loaded.saved)
            if updated != loaded.saved {
                do {
                    var candidate = loaded; candidate.working = updated
                    try candidate.save(to: demoStorage, catalog: demoCatalog); loaded = candidate
                } catch { diagnosticDetail = String(describing: error) }
            }
        }
        if initialDemoMode { editor = loaded }
        else {
            do { editor = try ConfigurationDraft(configuration: DemoData.configuration, catalog: InMemoryGaugeEndpoint.catalog) }
            catch { fatalError("Configurazione iniziale non valida") }
            storage = .init(fileURL: selectedDirectory.appending(path: "unselected/configuration-v1.json"))
            activeDraftKey = "unselected"
        }
        do {
            let fixture = try DemoGaugeFixture.load()
            let endpoint = try InMemoryGaugeEndpoint(configuration: loaded.saved, historySource: fixture.historySource)
            let transport = DevelopmentGaugeTransport(pipe: InMemoryGaugePacketPipe(channel: endpoint, clientID: "IPHONE-DEMO"))
            simulatorBinding = .simulator(transport, sessions: [fixture.historySource.sessionID]); binding = simulatorBinding
        } catch { fatalError("Contenuti iniziali non validi") }
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
        guard !started else { return }; started = true
        if isDemoMode {
            await loadDemoPreferences(); await reloadLogs(); await connectGauge(); await refreshHistory()
        } else { await reloadLogs() }
    }
    func connectGauge() async {
        guard !gaugeBusy else { return }
        guard isDemoMode else { gaugeNotice = "Collega il tuo dispositivo per continuare."; return }
        let token = sessionEpoch; let transport = self.transport
        gaugeBusy = true; defer { if token == sessionEpoch { gaugeBusy = false } }
        do {
            try await transport.connect()
            let nextCatalog = try await transport.supportedPIDs()
            let snapshot = try await transport.configuration()
            let id = try await transport.deviceIdentifier()
            guard token == sessionEpoch else { return }
            catalog = GaugeConfigurationDefaults.namedCatalog(nextCatalog); gaugeSnapshot = snapshot; gaugeDeviceID = id
            gaugeConnected = true; gaugeNeedsRead = false
            gaugeCommitState = await transport.commitState()
            guard token == sessionEpoch else { return }
            gaugeNotice = "Auto Gauge collegato"
        } catch { await refreshLinkAfterError(error, transport: transport, token: token) }
    }
    func activateNativeGauge() async {
        guard !isDemoMode else { return }
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
            catalog = GaugeConfigurationDefaults.namedCatalog(nextCatalog); gaugeSnapshot = snapshot; gaugeDeviceID = id
            pendingDeviceUpdate = !sameSettings(nextEditor.saved, snapshot)
            saveStatusText = nextEditor.hasChanges ? "Modifiche da salvare" : pendingDeviceUpdate ? "Modifiche salvate sul telefono" : "Modifiche salvate"
            gaugeConnected = true; gaugeNeedsRead = false; gaugeCommitState = .idle
            logArchive = nativeLogArchive
            logSessions = []; logReady = false; lastLogAcknowledgement = nil; historySourceCursor = 0
            changingSession = false; gaugeBusy = false
            gaugeNotice = "Auto Gauge collegato"
            await reloadLogs()
            guard installed == sessionEpoch else { return }
        } catch {
            changingSession = false
            await candidate?.disconnect()
            guard token == sessionEpoch else { return }
            show(error, message: "Non è stato possibile collegare il dispositivo. Riprova dopo aver controllato il dispositivo."); gaugeNotice = "Collegamento non disponibile"
        }
    }
    func useSimulator() async { await setDemoMode(true) }
    func setDemoMode(_ enabled: Bool) async {
        guard enabled != isDemoMode, !changingSession else { return }
        changingSession = true; sessionEpoch += 1
        if !isDemoMode { cloud.cancel() }
        backupGeneration += 1; backupWork?.cancel(); backupWork = nil
        cancelFirmwareUpdate(); firmwareGeneration += 1
        let old = transport
        await firmwareInstallation.cancel()
        await old.disconnect(); nativeBluetooth.stop()
        retainedDrafts[activeDraftKey] = editor
        isDemoMode = enabled; preferences.set(enabled, forKey: "demo.enabled")
        binding = simulatorBinding; nativePipeGeneration = nil
        gaugeConnected = false; gaugeBusy = false; logBusy = false; isSaving = false; backupBusy = false; pendingDeviceUpdate = false
        gaugeNeedsRead = true; gaugeSnapshot = nil; gaugeDeviceID = nil; gaugeCommitState = .idle
        logSessions = []; logReady = false; lastLogAcknowledgement = nil; historySourceCursor = 0
        catalog = GaugeConfigurationDefaults.namedCatalog(InMemoryGaugeEndpoint.catalog); activeDraftKey = enabled ? "simulation" : "unselected"
        if enabled {
            storage = .init(fileURL: directory.appending(path: "demo/configuration-v1.json")); logArchive = demoLogArchive
            do { try editor.reload(from: storage, catalog: catalog) } catch { show(error, message: "Non è stato possibile aprire le impostazioni salvate.") }
        } else {
            storage = .init(fileURL: directory.appending(path: "unselected/configuration-v1.json")); logArchive = nativeLogArchive
            do { editor = try ConfigurationDraft(configuration: DemoData.configuration, catalog: catalog) } catch { diagnosticDetail = String(describing: error) }
        }
        backupEnabled = preferences.object(forKey: enabled ? "demo.backup.enabled" : "device.backup.enabled") == nil ? enabled : preferences.bool(forKey: enabled ? "demo.backup.enabled" : "device.backup.enabled")
        backupStatusText = enabled ? "Tutto pronto" : "Il backup iCloud non è disponibile in questa versione"
        lastBackupDate = nil; changingSession = false
        if enabled { await loadDemoPreferences(); await connectGauge() }
        await reloadLogs()
        if enabled { await refreshHistory() }
        gaugeNotice = enabled ? "Auto Gauge collegato" : "Collega il tuo dispositivo per continuare."
    }
    private func nativeSessionEnded(generation: UInt64) async {
        guard isNativeSession, nativePipeGeneration == generation, !changingSession else { return }
        nativePipeGeneration = nil; sessionEpoch += 1; let token = sessionEpoch
        gaugeConnected = false; gaugeNeedsRead = true
        gaugeBusy = false; logBusy = false; isSaving = false
        let transport = self.transport
        await transport.disconnect()
        let state = await transport.commitState()
        guard token == sessionEpoch else { return }
        gaugeCommitState = state
        gaugeNotice = "Dispositivo scollegato. Le impostazioni e i viaggi sono conservati."
    }
    func disconnectGauge() async {
        sessionEpoch += 1; let token = sessionEpoch; let transport = self.transport
        gaugeConnected = false; gaugeNeedsRead = true; gaugeBusy = false; logBusy = false; isSaving = false
        await transport.disconnect()
        guard token == sessionEpoch else { return }
        gaugeNotice = "Dispositivo scollegato"
    }
    func readGauge() async {
        guard gaugeConnected, !gaugeBusy else { return }
        let token = sessionEpoch; let transport = self.transport
        gaugeBusy = true; defer { if token == sessionEpoch { gaugeBusy = false } }
        do {
            let nextCatalog = try await transport.supportedPIDs()
            let snapshot = try await transport.configuration()
            guard token == sessionEpoch else { return }
            catalog = GaugeConfigurationDefaults.namedCatalog(nextCatalog); gaugeSnapshot = snapshot; gaugeNeedsRead = false
            gaugeCommitState = await transport.commitState()
            guard token == sessionEpoch else { return }
            pendingDeviceUpdate = !sameSettings(editor.saved, snapshot)
            saveStatusText = editor.hasChanges ? "Modifiche da salvare" : pendingDeviceUpdate ? "Modifiche salvate sul telefono" : "Modifiche salvate"
            gaugeNotice = "Impostazioni aggiornate"
        } catch { await refreshLinkAfterError(error, transport: transport, token: token) }
    }
    func useGaugeSnapshot() {
        guard let gaugeSnapshot, !gaugeNeedsRead else { return }
        edit { try $0.restore(gaugeSnapshot, catalog: catalog) }
        gaugeNotice = "Impostazioni caricate. Puoi modificarle e salvarle."
    }
    func validateOnGauge() async {
        guard gaugeConnected, !gaugeBusy else { return }
        let token = sessionEpoch; let transport = self.transport
        gaugeBusy = true; defer { if token == sessionEpoch { gaugeBusy = false } }
        do {
            try await transport.validateDraft(editor.working)
            guard token == sessionEpoch else { return }
            gaugeNotice = "Le impostazioni sono pronte per essere salvate."
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
            gaugeNotice = "Impostazioni aggiornate"
        } catch { await refreshLinkAfterError(error, transport: transport, token: token) }
    }
    private func receiveGaugePhase(_ phase: GaugeCommitState, token: Int) { if token == sessionEpoch { gaugeCommitState = phase } }
    private func refreshLinkAfterError(_ error: Error, transport: any GaugeSessionTransport, token: Int) async {
        let connected = await transport.isConnected(); let needsRead = await transport.requiresRead(); let state = await transport.commitState()
        guard token == sessionEpoch else { return }
        gaugeConnected = connected; gaugeNeedsRead = needsRead; gaugeCommitState = state
        show(error, message: "Non è stato possibile collegare il dispositivo. Riprova dopo aver controllato il dispositivo."); gaugeNotice = "Collegamento non disponibile"
    }
    func reloadLogs() async {
        guard !logBusy else { return }
        let token = sessionEpoch; let archive = logArchive
        logBusy = true; defer { if token == sessionEpoch { logBusy = false } }
        do {
            try await archive.reload()
            do { if !isDemoMode { try await cloud.recoverQualifiedImports(from: archive) } }
            catch { if token == sessionEpoch { show(error, message: "I viaggi sono sul telefono. La copia di sicurezza sarà aggiornata al prossimo tentativo.") } }
            let sessions = await archive.sessions(); let ready = await archive.isReady()
            guard token == sessionEpoch else { return }
            logSessions = sessions; logReady = ready
        } catch {
            guard token == sessionEpoch else { return }
            logReady = false; show(error, message: "Non è stato possibile aprire i viaggi salvati. I dati precedenti sono conservati.")
        }
    }
    func syncSimulatedHistory() async { await syncGaugeHistory() }
    func syncGaugeHistory() async {
        if isDemoMode { await refreshHistory(); return }
        guard isNativeSession else { return }
        guard logReady, !logBusy, gaugeConnected, !gaugeBusy else { return }
        let token = sessionEpoch; let binding = self.binding; let transport = binding.transport; let archive = logArchive
        logBusy = true; gaugeBusy = true
        defer { if token == sessionEpoch { logBusy = false; gaugeBusy = false } }
        do {
            let sources = try await binding.historySessionIDs()
            guard token == sessionEpoch else { return }
            guard !sources.isEmpty else { lastLogAcknowledgement = "Non ci sono nuovi viaggi"; return }
            for _ in 0..<4 {
                let source = sources[historySourceCursor % sources.count]; historySourceCursor = (historySourceCursor + 1) % sources.count
                let imported = try await transport.importHistoryPage(sessionID: source, archive: archive, limit: 16)
                // Transfer the archive's durable intents, including earlier
                // interrupted imports, independently of the current UI lease.
                if imported.qualifiedImport != nil {
                    do { try await cloud.recoverQualifiedImports(from: archive) }
                    catch { if token == sessionEpoch { show(error, message: "I viaggi sono salvati sul telefono. La copia di sicurezza resta da aggiornare.") } }
                }
                guard token == sessionEpoch else { return }
                logSessions = await archive.sessions()
                guard token == sessionEpoch else { return }
                lastLogAcknowledgement = imported.peerAcknowledged ? "Viaggi aggiornati" : "Viaggi salvati sul telefono"
                if imported.warning != nil { message = "I viaggi sono salvati. Ricollega il dispositivo per completare l’aggiornamento."; break }
            }
        } catch {
            let sessions = await archive.sessions()
            guard token == sessionEpoch else { return }
            logSessions = sessions
            await refreshLinkAfterError(error, transport: transport, token: token)
        }
    }
    func importDemoLogs() async { if isDemoMode { await refreshHistory() } }
    func edit(_ operation: (inout ConfigurationDraft) throws -> Void) {
        var candidate = editor
        do { try operation(&candidate); editor = candidate }
        catch { show(error, message: "Non è stato possibile modificare questa impostazione.") }
    }
    func save() async { await saveChanges() }
    func saveChanges() async {
        guard !isSaving, hasPendingChanges else { return }
        guard isDemoMode || isNativeSession else { message = "Collega il dispositivo prima di salvare le modifiche."; return }
        let token = sessionEpoch; let source = transport
        var savedOnPhone = !editor.hasChanges
        isSaving = true; saveStatusText = "Salvataggio in corso…"
        defer { if token == sessionEpoch { isSaving = false } }
        do {
            if editor.hasChanges { try editor.save(to: storage, catalog: catalog) }
            savedOnPhone = true; pendingDeviceUpdate = true
            saveStatusText = "Modifiche salvate. Collega il dispositivo per aggiornarlo."
            // Unknown delivery is reconciled by reading, never by repeating a
            // mutation. New editing is persisted before comparing that read.
            if !isDemoMode && gaugeCommitState == .unknown {
                if gaugeConnected { await readGauge() }
                guard token == sessionEpoch else { return }
                if let observed = gaugeSnapshot, !gaugeNeedsRead, sameSettings(observed, editor.saved) {
                    pendingDeviceUpdate = false; saveStatusText = editor.hasChanges ? "Modifiche da salvare" : "Modifiche salvate"
                }
                return
            }
            if isDemoMode && !gaugeConnected { await connectGauge() }
            guard token == sessionEpoch, canApplyToGauge else { return }
            let applied = try await source.applyDraft(editor.saved, onPhase: { _ in })
            guard token == sessionEpoch else { return }
            gaugeSnapshot = applied; gaugeCommitState = await source.commitState()
            guard token == sessionEpoch else { return }
            pendingDeviceUpdate = false
            saveStatusText = editor.hasChanges ? "Modifiche da salvare" : "Modifiche salvate"
            gaugeNotice = "Le impostazioni sono aggiornate."
        } catch {
            guard token == sessionEpoch else { return }
            diagnosticDetail = String(describing: error)
            let state = await source.commitState()
            guard token == sessionEpoch else { return }
            gaugeCommitState = state; gaugeNeedsRead = await source.requiresRead()
            saveStatusText = savedOnPhone ? "Modifiche salvate sul telefono. Verifica il collegamento." : "Salvataggio non riuscito"
            if !savedOnPhone { message = "Non è stato possibile salvare le modifiche. Puoi riprovare: le tue modifiche sono conservate." }
        }
    }
    func discardChanges() { editor.cancel(); saveStatusText = "Modifiche annullate" }
    func cancelEdits() { discardChanges() }
    func reloadSaved() {
        do { try editor.reload(from: storage, catalog: catalog) }
        catch { show(error, message: "Non è stato possibile aprire il salvataggio. Le tue modifiche sono conservate.") }
    }
    func restoreDefaults() {
        do {
            var defaults = try GaugeConfigurationDefaults.make(catalog: catalog)
            for dashboard in defaults.dashboards { GaugeClusterLayout.apply(to: &defaults, dashboardID: dashboard.id) }
            edit { try $0.restore(defaults, catalog: catalog) }; saveStatusText = "Modifiche da salvare"
        }
        catch { show(error, message: "Collega il dispositivo per vedere le misure disponibili.") }
    }
    func restoreExample() { restoreDefaults() }
    func useClusterLayout(dashboardID: Int) {
        edit { GaugeClusterLayout.apply(to: &$0.working, dashboardID: dashboardID) }
    }
    func addDashboard() {
        guard let pid = catalog.first else { message = "Nessuna misura disponibile."; return }
        edit { draft in
            let id = try draft.addDashboard(pid: pid)
            GaugeClusterLayout.apply(to: &draft.working, dashboardID: id)
        }
    }
    private func sameSettings(_ lhs: GaugeConfiguration, _ rhs: GaugeConfiguration) -> Bool {
        var first = lhs; var second = rhs; first.revision = 1; second.revision = 1
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
        guard let left = try? encoder.encode(first), let right = try? encoder.encode(second) else { return false }
        return left == right
    }
    private func show(_ error: Error, message text: String) { diagnosticDetail = String(describing: error); message = text }
}

extension AppModel {
    func historyTitle(for session: ImportedLogSession) -> String {
        if isDemoMode, let value = try? DemoJourneyFixture.trips.first(where: { $0.sessionID == session.sessionID }) { return value.title }
        let position = logSessions.firstIndex(where: { $0.id == session.id }).map { $0 + 1 } ?? 1
        return "Viaggio \(position)"
    }
    func historyDate(for session: ImportedLogSession) -> Date? {
        guard isDemoMode else { return nil }
        return try? DemoJourneyFixture.trips.first(where: { $0.sessionID == session.sessionID })?.date
    }
    func refreshHistory() async {
        guard isDemoMode else { await syncGaugeHistory(); return }
        guard !logBusy else { return }
        let token = sessionEpoch; let archive = demoLogArchive
        logBusy = true; defer { if token == sessionEpoch { logBusy = false } }
        do {
            if !(await archive.isReady()) { try await archive.reload() }
            for trip in try DemoJourneyFixture.trips {
                for page in try DemoJourneyFixture.pages(for: trip) {
                    guard token == sessionEpoch, isDemoMode else { return }
                    _ = try await archive.importPage(deviceID: DemoJourneyFixture.deviceID, data: page)
                }
            }
            let sessions = await archive.sessions()
            guard token == sessionEpoch, isDemoMode else { return }
            logSessions = sessions; logReady = true; lastLogAcknowledgement = "Viaggi aggiornati"
        } catch {
            guard token == sessionEpoch else { return }
            show(error, message: "Non è stato possibile aggiornare i viaggi. La copia precedente è conservata.")
        }
    }
    private func loadDemoPreferences() async {
        let token = sessionEpoch
        do {
            let version = try await demoStore.firmwareVersion()
            let backup = try await demoStore.backupSnapshot()
            guard token == sessionEpoch, isDemoMode else { return }
            firmwareCurrentVersion = version
            firmwareAvailableVersion = version == "0.14.0" ? nil : "0.14.0"
            demoFirmwareState = version == "0.14.0" ? .completed : .idle
            firmwareStatusText = version == "0.14.0" ? "Il dispositivo è aggiornato" : "È disponibile un aggiornamento"
            lastBackupDate = backup.lastBackupDate
            backupStatusText = backup.lastBackupDate == nil ? "Pronto per la prima copia di sicurezza" : "Copia di sicurezza disponibile"
        } catch { guard token == sessionEpoch else { return }; show(error, message: "Non è stato possibile aprire tutte le impostazioni. I salvataggi sono conservati.") }
    }
    func synchronizeBackup() async { await runBackupAction(restoring: false) }
    func restoreBackup() async { await runBackupAction(restoring: true) }
    private func runBackupAction(restoring: Bool) async {
        guard backupWork == nil else { return }
        backupGeneration += 1; let token = backupGeneration
        let task = Task { [self] in
            if restoring { await performRestoreBackup() } else { await performSynchronizeBackup() }
        }
        backupWork = task
        await withTaskCancellationHandler { await task.value } onCancel: { task.cancel() }
        if token == backupGeneration { backupWork = nil }
    }
    private func performSynchronizeBackup() async {
        guard !backupBusy else { return }
        guard backupEnabled else { backupStatusText = "Attiva la copia di sicurezza per continuare"; return }
        guard isDemoMode else { await performCloudBackup(restoring: false); return }
        let token = sessionEpoch; let sessions = logSessions
        backupBusy = true; defer { if token == sessionEpoch { backupBusy = false } }
        do {
            let result = try await demoStore.synchronize(sessions)
            guard token == sessionEpoch, isDemoMode else { return }
            lastBackupDate = result.lastBackupDate; backupStatusText = "Copia di sicurezza aggiornata"
        } catch { guard token == sessionEpoch else { return }; diagnosticDetail = String(describing: error); backupStatusText = "Non è stato possibile aggiornare la copia. Riprova." }
    }
    private func performRestoreBackup() async {
        guard !backupBusy else { return }
        guard isDemoMode else { await performCloudBackup(restoring: true); return }
        let token = sessionEpoch; let archive = demoLogArchive
        backupBusy = true; defer { if token == sessionEpoch { backupBusy = false } }
        do {
            if !(await archive.isReady()) { try await archive.reload() }
            let result = try await demoStore.restore(into: archive)
            let sessions = await archive.sessions()
            guard token == sessionEpoch, isDemoMode else { return }
            logSessions = sessions; logReady = true; lastBackupDate = result.lastBackupDate
            backupStatusText = result.lastBackupDate == nil ? "Non c’è ancora una copia da recuperare" : "Viaggi recuperati"
        } catch { guard token == sessionEpoch else { return }; diagnosticDetail = String(describing: error); backupStatusText = "Non è stato possibile recuperare i viaggi. La copia è conservata." }
    }
    private func performCloudBackup(restoring: Bool) async {
        guard cloud.configured else { backupStatusText = "Il backup iCloud non è disponibile in questa versione"; return }
        guard !backupBusy, !cloud.busy else { return }
        let token = sessionEpoch; backupBusy = true
        defer { if token == sessionEpoch { backupBusy = false } }
        backupStatusText = "Verifica dell’account in corso…"
        cloud.verify()
        guard await waitForCloud(token: token), !isDemoMode else { return }
        guard case .available = cloud.account else { backupStatusText = "Accedi a iCloud nelle impostazioni del telefono per continuare"; return }
        backupStatusText = restoring ? "Recupero dei viaggi in corso…" : "Aggiornamento della copia di sicurezza…"
        if restoring { cloud.restore(restart: cloud.restoreFinished) } else { cloud.synchronize() }
        guard await waitForCloud(token: token), !isDemoMode else { return }
        backupStatusText = cloud.notice
        if cloud.lastOperationSucceeded, !restoring { lastBackupDate = cloud.lastSynchronizationDate }
    }
    private func waitForCloud(token: Int) async -> Bool {
        let deadline = ContinuousClock().now.advanced(by: .seconds(30))
        while cloud.busy {
            guard token == sessionEpoch, !isDemoMode, !Task.isCancelled else { return false }
            guard ContinuousClock().now < deadline else {
                cloud.cancel(); backupStatusText = "La connessione è lenta. Puoi riprovare più tardi."; return false
            }
            do { try await Task.sleep(for: .milliseconds(100)) }
            catch { if token == sessionEpoch { cloud.cancel() }; return false }
        }
        return token == sessionEpoch && !isDemoMode && !Task.isCancelled
    }
    func checkFirmwareUpdate() async {
        guard isDemoMode, firmwareWork == nil else { return }
        await runFirmwareOperation(state: .checking, steps: 4)
    }
    func downloadFirmwareUpdate() async {
        guard isDemoMode, firmwareWork == nil, firmwareAvailableVersion != nil,
              demoFirmwareState == .available || demoFirmwareState == .idle else { return }
        await runFirmwareOperation(state: .downloading, steps: 20)
    }
    func installFirmwareUpdate() async {
        guard isDemoMode, firmwareWork == nil, demoFirmwareState == .downloaded else { return }
        await runFirmwareOperation(state: .installing, steps: 20)
    }
    func cancelFirmwareUpdate() {
        firmwareGeneration += 1; firmwareWork?.cancel(); firmwareWork = nil
        if demoFirmwareState == .checking || demoFirmwareState == .downloading || demoFirmwareState == .installing {
            demoFirmwareState = demoFirmwareState == .installing ? .downloaded : .available
            demoFirmwareProgress = 0; firmwareStatusText = "Operazione annullata"
        }
    }
    private func runFirmwareOperation(state: DemoFirmwareState, steps: Int) async {
        firmwareGeneration += 1; let token = firmwareGeneration; let source = sessionEpoch
        demoFirmwareState = state; demoFirmwareProgress = 0
        firmwareStatusText = state == .checking ? "Ricerca aggiornamenti…" : state == .downloading ? "Download in corso…" : "Installazione in corso…"
        let task = Task { [self] in
            do {
                for step in 1...steps {
                    try await Task.sleep(for: demoStepDelay)
                    try Task.checkCancellation()
                    guard token == firmwareGeneration, source == sessionEpoch, isDemoMode else { return }
                    demoFirmwareProgress = Double(step) / Double(steps)
                }
                try Task.checkCancellation()
                guard token == firmwareGeneration, source == sessionEpoch, isDemoMode else { return }
                switch state {
                case .checking:
                    firmwareAvailableVersion = firmwareCurrentVersion == "0.14.0" ? nil : "0.14.0"
                    demoFirmwareState = firmwareAvailableVersion == nil ? .completed : .available
                    firmwareStatusText = firmwareAvailableVersion == nil ? "Il dispositivo è aggiornato" : "Aggiornamento disponibile"
                case .downloading:
                    demoFirmwareState = .downloaded; firmwareStatusText = "Aggiornamento pronto per essere installato"
                case .installing:
                    try await demoStore.installFirmwareVersion("0.14.0")
                    guard token == firmwareGeneration, source == sessionEpoch, isDemoMode else { return }
                    firmwareCurrentVersion = "0.14.0"; firmwareAvailableVersion = nil
                    demoFirmwareState = .completed; firmwareStatusText = "Aggiornamento completato"
                default: break
                }
                guard token == firmwareGeneration, source == sessionEpoch, isDemoMode else { return }
                firmwareWork = nil
            } catch {
                guard token == firmwareGeneration, source == sessionEpoch else { return }
                diagnosticDetail = String(describing: error)
                demoFirmwareState = state == .installing ? .downloaded : .available
                firmwareStatusText = "Operazione non completata. Puoi riprovare."; firmwareWork = nil
            }
        }
        firmwareWork = task
        await withTaskCancellationHandler { await task.value } onCancel: { task.cancel() }
    }
}
