import SwiftUI
import GaugeCore

@main
struct AutoGaugeApp: App {
    @StateObject private var model = AppModel()
    var body: some Scene {
        WindowGroup {
            RootView().environmentObject(model).preferredColorScheme(.dark)
        }
    }
}

@MainActor
final class AppModel: ObservableObject {
    @Published var editor: ConfigurationDraft
    @Published var message: String?
    @Published var logSessions: [ImportedLogSession] = []
    @Published var logReady = false
    @Published var logBusy = false
    @Published var lastLogAcknowledgement: String?
    private let logArchive: LocalLogArchive
    let catalog = DemoData.catalog
    private let storage: AtomicFileConfigurationStorage
    var configuration: GaugeConfiguration {
        get { editor.working }
        set { editor.working = newValue }
    }
    var savedRevision: Int { editor.saved.revision }

    init() {
        let directory = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        storage = .init(fileURL: directory.appendingPathComponent("AutoGauge/simulated-configuration-v1.json"))
        logArchive = LocalLogArchive(storage: AtomicLogFileStorage(fileURL: directory.appendingPathComponent("AutoGauge/simulated-histories-v1.json")))
        editor = try! ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
        do {
            if let data = try storage.read() {
                let saved = try JSONDecoder().decode(GaugeConfiguration.self, from: data)
                editor = try ConfigurationDraft(configuration: saved, catalog: DemoData.catalog)
            } else if let legacy = UserDefaults.standard.data(forKey: "auto-gauge.simulated-configuration.v1") {
                let saved = try JSONDecoder().decode(GaugeConfiguration.self, from: legacy)
                let migrated = try ConfigurationDraft(configuration: saved, catalog: DemoData.catalog)
                try storage.write(legacy)
                editor = migrated
            }
        } catch { message = "La bozza locale non è leggibile. È stato caricato l’esempio senza sovrascrivere il file salvato. \(error.localizedDescription)" }
        Task { await reloadLogs(importDemoIfEmpty: true) }
    }
    func reloadLogs(importDemoIfEmpty: Bool = false) async {
        guard !logBusy else { return }
        logBusy = true
        do {
            try await logArchive.reload()
            logSessions = await logArchive.sessions()
            logReady = true
            logBusy = false
            if importDemoIfEmpty && logSessions.isEmpty { await importDemoLogs() }
        } catch {
            logBusy = false
            logReady = false
            message = "Ricarica storico non riuscita. Nessun dato sovrascritto e nessun nuovo ACK. \(error.localizedDescription)"
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
                lastLogAcknowledgement = "Sessione \(ack.sessionID.value.prefix(8)) · fino alla sequenza \(ack.acknowledgedThrough.decimal)"
                logSessions = await logArchive.sessions()
            }
        } catch { message = "Pagina storico rifiutata: nessun ACK per la pagina fallita. \(error.localizedDescription)" }
    }
    func edit(_ operation: (inout ConfigurationDraft) throws -> Void) {
        var candidate = editor
        do { try operation(&candidate); editor = candidate }
        catch { message = error.localizedDescription }
    }
    func save() async {
        do {
            try editor.save(to: storage, catalog: catalog)
            message = "Bozza salvata su questo iPhone con revisione \(editor.saved.revision). Nessun dispositivo reale modificato."
        } catch { message = error.localizedDescription }
    }
    func cancelEdits() { editor.cancel() }
    func reloadSaved() {
        do { try editor.reload(from: storage, catalog: catalog) }
        catch { message = "Ricarica non riuscita: la bozza corrente è stata conservata. \(error.localizedDescription)" }
    }
    func restoreExample() { edit { try $0.restore(DemoData.configuration, catalog: catalog) } }
    func addDashboard() { edit { _ = try $0.addDashboard(pid: catalog[0]) } }
}

private enum Palette {
    static let accent = Color(red: 0.43, green: 0.91, blue: 0.76)
    static let muted = Color(red: 0.58, green: 0.65, blue: 0.68)
    static let panel = Color(red: 0.075, green: 0.105, blue: 0.12)
}

struct RootView: View {
    @EnvironmentObject private var model: AppModel
    var body: some View {
        TabView {
            NavigationStack { DeviceView() }.tabItem { Label("Dispositivo", systemImage: "circle.hexagongrid") }
            NavigationStack { DashboardListView() }.tabItem { Label("Dashboard", systemImage: "rectangle.3.group") }
            NavigationStack { SessionsView() }.tabItem { Label("Storici", systemImage: "clock.arrow.circlepath") }
            NavigationStack { AccountView() }.tabItem { Label("Account", systemImage: "person.crop.circle") }
        }
        .tint(Palette.accent)
        .safeAreaInset(edge: .top, spacing: 0) {
            HStack(spacing: 7) {
                Image(systemName: "testtube.2")
                Text("MODALITÀ SIMULATA").fontWeight(.semibold)
                Text("· dati dimostrativi").foregroundStyle(Palette.muted)
            }
            .font(.caption2).frame(maxWidth: .infinity).padding(.vertical, 9)
            .background(Palette.panel)
            .accessibilityElement(children: .combine)
        }
        .alert("Auto Gauge", isPresented: Binding(get: { model.message != nil }, set: { if !$0 { model.message = nil } })) {
            Button("Va bene", role: .cancel) { model.message = nil }
        } message: { Text(model.message ?? "") }
    }
}

struct DeviceView: View {
    @EnvironmentObject private var model: AppModel
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                HStack(alignment: .top) {
                    VStack(alignment: .leading, spacing: 5) {
                        Text("IL TUO GAUGE").font(.caption).tracking(2).foregroundStyle(Palette.muted)
                        Text("Pronto per prendere forma.").font(.largeTitle.bold())
                    }
                    Spacer()
                    Image(systemName: "circle.dotted.circle.fill").font(.system(size: 38)).foregroundStyle(Palette.accent)
                }
                VStack(alignment: .leading, spacing: 16) {
                    Label("Gauge dimostrativo", systemImage: "circle.fill").font(.headline).foregroundStyle(Palette.accent)
                    Text("Prepara dashboard e preferenze. Le modifiche restano nella simulazione fino all’integrazione con il dispositivo.")
                        .font(.subheadline).foregroundStyle(.secondary)
                    HStack {
                        stat("Dashboard", "\(model.configuration.dashboards.count) / 3")
                        Spacer()
                        stat("Revisione locale", "\(model.savedRevision)")
                    }
                }.padding(20).background(Palette.panel, in: RoundedRectangle(cornerRadius: 22))
                VStack(spacing: 0) {
                    NavigationLink { BrightnessView() } label: {
                        settingsRow("Luminosità", detail: model.configuration.brightness.mode.nameIT, icon: "sun.max")
                    }
                    Divider().padding(.leading, 52)
                    NavigationLink { CatalogView() } label: {
                        settingsRow("Catalogo misure", detail: "5 PID di esempio", icon: "list.bullet.rectangle")
                    }
                    Divider().padding(.leading, 52)
                    NavigationLink { FirmwareView() } label: {
                        settingsRow("Firmware e aggiornamenti", detail: "Integrazione in preparazione", icon: "arrow.down.circle")
                    }
                }.padding(.horizontal, 16).background(Palette.panel, in: RoundedRectangle(cornerRadius: 22))
                VStack(alignment: .leading, spacing: 8) {
                    Label("Collegamento al dispositivo", systemImage: "antenna.radiowaves.left.and.right").font(.headline)
                    Text("Più telefoni potranno essere autorizzati, con un solo collegamento Bluetooth attivo alla volta. La sessione autenticata è ancora da integrare.")
                        .font(.subheadline).foregroundStyle(.secondary)
                    Button("Cerca un gauge") {}.buttonStyle(.bordered).disabled(true)
                }
                Text("Il gauge funzionerà autonomamente in auto. Questa app serve a configurarlo e a sincronizzare gli storici.")
                    .font(.footnote).foregroundStyle(Palette.muted)
            }.padding(24)
        }
        .navigationTitle("Auto Gauge").navigationBarTitleDisplayMode(.inline)
    }
    private func stat(_ title: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(title).font(.caption).foregroundStyle(.secondary)
            Text(value).font(.title3.monospacedDigit().weight(.semibold))
        }
    }
    private func settingsRow(_ title: String, detail: String, icon: String) -> some View {
        HStack(spacing: 14) {
            Image(systemName: icon).foregroundStyle(Palette.accent).frame(width: 24)
            VStack(alignment: .leading, spacing: 4) {
                Text(title).font(.subheadline.weight(.semibold)).foregroundStyle(.primary)
                Text(detail).font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            Image(systemName: "chevron.right").font(.caption).foregroundStyle(.secondary)
        }.padding(.vertical, 17)
    }
}

struct DashboardListView: View {
    @EnvironmentObject private var model: AppModel
    var body: some View {
        List {
            Section {
                Text("Personalizza il display del gauge. L’anteprima usa valori fissi di esempio e non riceve dati live.")
                    .font(.subheadline).foregroundStyle(.secondary)
            }
            Section {
                ForEach(model.configuration.dashboards) { dashboard in
                    NavigationLink {
                        DashboardEditorView(dashboardID: dashboard.id)
                    } label: {
                        HStack {
                            Text(String(dashboard.id)).font(.headline.monospaced()).foregroundStyle(Palette.accent).frame(width: 26)
                            VStack(alignment: .leading) {
                                Text(dashboard.name)
                                Text("\(dashboard.widgets.count) misure").font(.caption).foregroundStyle(.secondary)
                            }
                        }
                    }
                }.onDelete { offsets in
                    let ids = offsets.map { model.configuration.dashboards[$0].id }
                    model.edit { draft in for id in ids { try draft.removeDashboard(id: id) } }
                }
                Button { model.addDashboard() } label: { Label("Aggiungi dashboard", systemImage: "plus.circle") }
                    .disabled(model.configuration.dashboards.count >= 3)
            } header: { Text("Le tue dashboard") } footer: { Text("Da 1 a 3 dashboard, fino a 5 misure per ciascuna. Gli ID delle dashboard esistenti non cambiano dopo una rimozione.") }
            DraftActionsView()
        }.navigationTitle("Dashboard")
    }
}

struct DraftActionsView: View {
    @EnvironmentObject private var model: AppModel
    var body: some View {
        Section {
            Label(model.editor.hasChanges ? "Modifiche non salvate" : "Bozza salvata · revisione \(model.savedRevision)", systemImage: model.editor.hasChanges ? "pencil.circle" : "checkmark.circle")
                .font(.subheadline).foregroundStyle(model.editor.hasChanges ? .orange : Palette.accent)
            Button("Salva bozza locale") { Task { await model.save() } }.disabled(!model.editor.hasChanges)
            Button("Annulla modifiche") { model.cancelEdits() }.disabled(!model.editor.hasChanges)
            Button("Ricarica bozza salvata") { model.reloadSaved() }
            Button("Ripristina esempio nella bozza") { model.restoreExample() }
        } footer: { Text("Salva conserva tutte le dashboard e la luminosità su questo iPhone. Annulla ripristina l’ultimo salvataggio. Ripristina prepara l’esempio senza salvarlo. Nessun invio al gauge reale.") }
    }
}

struct DashboardEditorView: View {
    @EnvironmentObject private var model: AppModel
    let dashboardID: Int
    private var selectedID: Int? {
        model.editor.selection?.dashboardID == dashboardID ? model.editor.selection?.widgetID : nil
    }
    var body: some View {
        if let dashboard = model.configuration.dashboards.first(where: { $0.id == dashboardID }) {
            Form {
                Section {
                    GaugePreview(dashboard: dashboard, selectedID: selectedID, onSelect: select, onMove: move)
                        .frame(maxWidth: .infinity).padding(.vertical, 12)
                    Text("ANTEPRIMA · VALORI DIMOSTRATIVI").font(.caption2.weight(.semibold)).foregroundStyle(Palette.muted)
                        .frame(maxWidth: .infinity)
                    Text("Tocca una misura per selezionarla. Trascinala oppure usa i controlli di posizione e dimensione.")
                        .font(.caption).foregroundStyle(.secondary)
                }.listRowBackground(Color.clear)
                let clipped = model.editor.clippedWidgetIDs(dashboardID: dashboardID)
                if !clipped.isEmpty {
                    Section {
                        Label("\(clipped.count) misure tagliate dal bordo circolare", systemImage: "exclamationmark.triangle")
                            .foregroundStyle(.orange).font(.subheadline)
                        Text("Il riquadro supera il bordo del display e l’anteprima viene tagliata. Controlla posizione e dimensioni prima di salvare.")
                            .font(.caption).foregroundStyle(.secondary)
                    }
                }
                Section(dashboard.name) {
                    ForEach(dashboard.widgets) { widget in
                        Button { select(widget.id) } label: {
                            HStack {
                                VStack(alignment: .leading) {
                                    Text(widget.pid.nameIT).foregroundStyle(.primary)
                                    Text("Misura \(widget.id) · \(widget.pid.unit)").font(.caption).foregroundStyle(.secondary)
                                }
                                Spacer()
                                if clipped.contains(widget.id) { Image(systemName: "exclamationmark.triangle").foregroundStyle(.orange).accessibilityLabel("Tagliata sul bordo circolare") }
                                Image(systemName: selectedID == widget.id ? "checkmark.circle.fill" : "circle")
                            }
                        }.accessibilityLabel("\(widget.pid.nameIT), misura \(widget.id)")
                         .accessibilityValue(selectedID == widget.id ? "Selezionata" : "Non selezionata")
                    }.onDelete { offsets in
                        let ids = offsets.map { dashboard.widgets[$0].id }
                        model.edit { draft in for id in ids { try draft.removeWidget(dashboardID: dashboardID, widgetID: id) } }
                    }
                    Menu {
                        ForEach(model.catalog) { pid in
                            Button(pid.nameIT) { model.edit { _ = try $0.addWidget(dashboardID: dashboardID, pid: pid) } }
                        }
                    } label: { Label("Aggiungi misura", systemImage: "plus") }
                    .disabled(dashboard.widgets.count >= 5)
                }
                if let selectedID, let widget = dashboard.widgets.first(where: { $0.id == selectedID }),
                   let rectangle = try? EditingRectangle(widget.frame) {
                    Section("Posizione e dimensioni · misura \(selectedID)") {
                        ForEach(GeometryField.allCases, id: \.self) { field in
                            geometryControl(field, rectangle: rectangle, widgetID: selectedID)
                        }
                    }
                }
                DraftActionsView()
            }.navigationTitle("Modifica dashboard").navigationBarTitleDisplayMode(.inline)
             .onAppear { if selectedID == nil, let id = dashboard.widgets.first?.id { select(id) } }
        } else {
            ContentUnavailableView("Dashboard rimossa", systemImage: "rectangle.slash", description: Text("Torna all’elenco per scegliere un’altra dashboard."))
        }
    }
    private func select(_ id: Int) { model.edit { try $0.select(dashboardID: dashboardID, widgetID: id) } }
    private func move(_ id: Int, _ x: Int, _ y: Int) {
        model.edit {
            try $0.select(dashboardID: dashboardID, widgetID: id)
            try $0.setGeometry(dashboardID: dashboardID, widgetID: id, field: .x, units: x)
            try $0.setGeometry(dashboardID: dashboardID, widgetID: id, field: .y, units: y)
        }
    }
    private func geometryControl(_ field: GeometryField, rectangle: EditingRectangle, widgetID: Int) -> some View {
        let bounds = rectangle.range(for: field)
        let value = Binding<Double>(get: { Double(rectangle[field]) }, set: { value in
            model.edit { try $0.setGeometry(dashboardID: dashboardID, widgetID: widgetID, field: field, units: Int(value.rounded())) }
        })
        return VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(field.titleIT)
                Spacer()
                Text("\(rectangle[field]) / 10000").font(.caption.monospacedDigit()).foregroundStyle(.secondary)
            }
            if bounds.lowerBound < bounds.upperBound {
                Slider(value: value, in: Double(bounds.lowerBound)...Double(bounds.upperBound), step: 1)
                    .accessibilityLabel(field.titleIT).accessibilityValue("\(rectangle[field]) su diecimila")
            }
            Stepper("Regola \(field.titleIT.lowercased())", value: Binding<Int>(get: { rectangle[field] }, set: { units in
                model.edit { try $0.setGeometry(dashboardID: dashboardID, widgetID: widgetID, field: field, units: units) }
            }), in: bounds, step: 1).font(.caption)
        }
    }
}

struct GaugePreview: View {
    let dashboard: Dashboard
    var selectedID: Int? = nil
    var onSelect: (Int) -> Void = { _ in }
    var onMove: (Int, Int, Int) -> Void = { _, _, _ in }
    @State private var draggedID: Int?
    @State private var dragOrigin: EditingRectangle?
    var body: some View {
        GeometryReader { proxy in
            let size = proxy.size.width
            ZStack {
                Circle().fill(.black)
                Circle().stroke(Palette.accent.opacity(0.22), lineWidth: 2)
                ForEach(dashboard.widgets) { widget in
                    VStack(spacing: 2) {
                        Text(exampleValue(widget.pid.pid)).font(.system(size: widget.frame.height > 0.22 ? 28 : 21, weight: .medium, design: .rounded)).monospacedDigit()
                        Text(widget.pid.unit).font(.system(size: 9)).foregroundStyle(Palette.accent)
                        Text(widget.pid.nameIT).font(.system(size: 8)).foregroundStyle(Palette.muted).lineLimit(1).minimumScaleFactor(0.6)
                    }
                    .frame(width: size * widget.frame.width, height: size * widget.frame.height)
                    .background(selectedID == widget.id ? Palette.accent.opacity(0.10) : Color.clear)
                    .overlay { Rectangle().stroke(selectedID == widget.id ? Palette.accent : Color.clear, lineWidth: 1) }
                    .contentShape(Rectangle())
                    .onTapGesture { onSelect(widget.id) }
                    .gesture(DragGesture(minimumDistance: 8).onChanged { value in
                        if draggedID != widget.id { draggedID = widget.id; dragOrigin = try? EditingRectangle(widget.frame) }
                        if let origin = dragOrigin {
                            onMove(widget.id, origin.x + Int(value.translation.width / size * 10000), origin.y + Int(value.translation.height / size * 10000))
                        }
                    }.onEnded { _ in draggedID = nil; dragOrigin = nil })
                    .position(x: size * (widget.frame.x + widget.frame.width / 2), y: size * (widget.frame.y + widget.frame.height / 2))
                    .accessibilityElement(children: .ignore)
                    .accessibilityLabel("\(widget.pid.nameIT), valore dimostrativo \(exampleValue(widget.pid.pid)) \(widget.pid.unit)")
                    .accessibilityValue(selectedID == widget.id ? "Misura selezionata" : "Tocca per selezionare")
                    .accessibilityAddTraits(.isButton)
                    .accessibilityAction { onSelect(widget.id) }
                }
            }.clipShape(Circle())
        }.frame(width: 250, height: 250)
         .accessibilityElement(children: .contain)
         .accessibilityLabel("Anteprima statica di \(dashboard.name), con valori dimostrativi")
    }
    private func exampleValue(_ pid: Int) -> String {
        switch pid { case 12: return "2.150"; case 13: return "72"; case 5: return "89"; case 4: return "38"; case 15: return "24"; default: return "—" }
    }
}

struct BrightnessView: View {
    @EnvironmentObject private var model: AppModel
    var body: some View {
        Form {
            Section("Modalità") {
                Picker("Controllo", selection: $model.configuration.brightness.mode) {
                    ForEach(BrightnessMode.allCases, id: \.self) { mode in Text(mode.nameIT).tag(mode) }
                }.pickerStyle(.segmented)
            }
            Section {
                HStack { Image(systemName: "sun.min"); Slider(value: Binding(get: { Double(model.configuration.brightness.manualPercent) }, set: { model.configuration.brightness.manualPercent = Int($0) }), in: 0...100, step: 1); Image(systemName: "sun.max") }
                Text("\(model.configuration.brightness.manualPercent)%").monospacedDigit()
            } header: { Text("Valore manuale") } footer: { Text("In automatica il gauge userà il sensore ambiente. Curva e limiti del display sono ancora da validare; il valore manuale viene conservato.") }
            Button("Salva nella simulazione") { Task { await model.save() } }
        }.navigationTitle("Luminosità")
    }
}

struct CatalogView: View {
    @EnvironmentObject private var model: AppModel
    var body: some View {
        List {
            Section { Text("Catalogo dimostrativo Service 01. I PID realmente disponibili arriveranno dalla discovery del gauge; la compatibilità con la tua auto non è ancora verificata.").font(.subheadline).foregroundStyle(.secondary) }
            ForEach(model.catalog) { pid in
                HStack {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(pid.nameIT)
                        Text(String(format: "Service 01 · PID %02X · ECU %@", pid.pid, pid.ecuSource)).font(.caption.monospaced()).foregroundStyle(.secondary)
                    }
                    Spacer(); Text(pid.unit).font(.caption).foregroundStyle(Palette.accent)
                }
            }
        }.navigationTitle("Catalogo misure")
    }
}

struct SessionsView: View {
    @EnvironmentObject private var model: AppModel
    var body: some View {
        List {
            Section {
                Label("Storici importati da fixture locali", systemImage: "tray.and.arrow.down").font(.headline)
                Text("Dati dimostrativi letti dal bundle e salvati nell’archivio di questo iPhone. Nessuna lettura dal veicolo o telemetria live.")
                    .font(.subheadline).foregroundStyle(.secondary)
            }
            if model.logBusy { Section { ProgressView("Importazione locale in corso…") } }
            if model.logSessions.isEmpty && !model.logBusy {
                Section { Text(model.logReady ? "Nessuna sessione importata." : "Archivio locale non disponibile. Riprova la ricarica.").foregroundStyle(.secondary) }
            }
            Section("Sessioni su questo iPhone") {
                ForEach(model.logSessions) { session in
                    NavigationLink {
                        HistoricalSessionView(sessionID: session.id)
                    } label: {
                        VStack(alignment: .leading, spacing: 5) {
                            Text("Sessione \(session.sessionID.value.prefix(8))").font(.headline)
                            Text("\(session.records.count) record · \(session.lost.count) intervalli di perdita").font(.subheadline).foregroundStyle(.secondary)
                            Text(session.deviceID).font(.caption.monospaced()).foregroundStyle(Palette.muted)
                        }.padding(.vertical, 4)
                    }
                }
            }
            Section {
                Button("Importa di nuovo le fixture locali") { Task { await model.importDemoLogs() } }
                    .disabled(!model.logReady || model.logBusy)
                Button("Ricarica archivio locale") { Task { await model.reloadLogs() } }.disabled(model.logBusy)
                if let acknowledgement = model.lastLogAcknowledgement {
                    Label("Scrittura locale confermata", systemImage: "checkmark.circle").foregroundStyle(Palette.accent)
                    Text(acknowledgement).font(.caption.monospacedDigit())
                }
                Button("Sincronizza dal gauge") {}.disabled(true)
            } footer: {
                Text("Reimportare la stessa pagina non duplica i record. I cursori appartengono a questa installazione: le conferme locali non cancellano il buffer del gauge né gli storici di altri telefoni. Bluetooth e cloud restano disattivati.")
            }
            Section {
                Text("Archivio dimostrativo limitato: a capacità piena l’import viene rifiutato senza eliminare record già importati.")
                    .font(.footnote).foregroundStyle(.secondary)
            }
        }.navigationTitle("Storici")
    }
}

struct HistoricalSessionView: View {
    @EnvironmentObject private var model: AppModel
    let sessionID: String
    var body: some View {
        if let session = model.logSessions.first(where: { $0.id == sessionID }) {
            List {
                Section("Sessione dimostrativa") {
                    LabeledContent("Dispositivo", value: session.deviceID)
                    LabeledContent("Sessione", value: session.sessionID.value).font(.caption.monospaced())
                    LabeledContent("Cursore locale", value: session.acknowledgedThrough.decimal).monospacedDigit()
                    LabeledContent("Orario assoluto", value: "Non disponibile")
                    Text("I tempi sono relativi all’avvio del gauge. Nessuna data di guida viene ricostruita senza un’ancora temporale affidabile.")
                        .font(.caption).foregroundStyle(.secondary)
                }
                if !session.lost.isEmpty {
                    Section("Perdite dichiarate dal produttore della pagina") {
                        ForEach(session.lost.indices, id: \.self) { index in
                            let interval = session.lost[index]
                            Label("Sequenze \(interval.first.decimal)–\(interval.last.decimal)", systemImage: "exclamationmark.triangle")
                                .font(.subheadline.monospacedDigit()).foregroundStyle(.orange)
                        }
                    }
                }
                Section("Record importati") {
                    ForEach(session.records) { record in
                        VStack(alignment: .leading, spacing: 6) {
                            HStack {
                                Text("Sequenza \(record.sequence.decimal)").font(.subheadline.monospacedDigit())
                                Spacer()
                                Text(record.status.titleIT).font(.caption.weight(.semibold))
                                    .foregroundStyle(record.status == .valid ? Palette.accent : .orange)
                            }
                            Text(record.relativeTimeIT).font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                            Text(String(format: "Service 01 · PID %02X · ECU 0x%X", record.pid, record.ecuSource))
                                .font(.caption.monospaced()).foregroundStyle(.secondary)
                            if record.status == .valid {
                                Text("Bytes OBD: \(record.rawHex)").font(.caption.monospaced()).textSelection(.enabled)
                            } else {
                                Text("—").font(.title2).accessibilityLabel("Dato non disponibile")
                            }
                        }.padding(.vertical, 5).accessibilityElement(children: .combine)
                    }
                }
            }.navigationTitle("Dettaglio storico").navigationBarTitleDisplayMode(.inline)
        } else {
            ContentUnavailableView("Sessione non disponibile", systemImage: "clock", description: Text("Torna all’elenco degli storici."))
        }
    }
}

struct FirmwareView: View {
    var body: some View {
        Form {
            Section("Dispositivo dimostrativo") {
                LabeledContent("Firmware", value: "Non collegato")
                LabeledContent("Aggiornamento", value: "Non disponibile")
            }
            Section {
                Button("Verifica aggiornamenti") {}.disabled(true)
            } footer: { Text("Gli aggiornamenti BLE saranno abilitati dopo verifica di firma, compatibilità hardware e procedura di recupero. Nessun file firmware viene scaricato o installato da questa versione.") }
        }.navigationTitle("Firmware")
    }
}

struct AccountView: View {
    var body: some View {
        Form {
            Section {
                Label("Account da configurare", systemImage: "person.crop.circle.badge.questionmark").font(.headline)
                Text("Provider di accesso e sincronizzazione cloud non ancora scelti. La modalità dimostrativa funziona senza account.").foregroundStyle(.secondary)
            }
            Section {
                Button("Accedi") {}.disabled(true)
                Button("Crea account") {}.disabled(true)
            } footer: { Text("Nessun account creato, nessun dato inviato a servizi esterni. Le sole preferenze dimostrative sono salvate localmente.") }
            Section("Associazione") {
                LabeledContent("Collegamento Bluetooth", value: "Non attivo")
                Text("Più telefoni possono essere autorizzati e utilizzati in momenti diversi. È consentito un solo collegamento Bluetooth attivo alla volta; cambiare telefono non richiede reset. Autenticazione e gestione dei telefoni autorizzati sono ancora da integrare.").font(.footnote).foregroundStyle(.secondary)
            }
            Section("Manutenzione fisica separata") {
                Text("Il reset volontario richiede USB e conferma fisica qualificata. Cancellerà credenziali, configurazione e log locali del gauge; non eliminerà dati cloud o storici già sincronizzati. Non serve per aggiungere o cambiare telefono.")
                    .font(.footnote).foregroundStyle(.secondary)
                Text("Questa app non esegue alcun reset reale.").font(.caption).foregroundStyle(.secondary)
            }
        }.navigationTitle("Account")
    }
}
