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
    @Published var configuration = DemoData.configuration
    @Published var message: String?
    @Published var savedRevision = 0
    let catalog = DemoData.catalog
    private let transport = SimulatedGaugeTransport()
    private let draftKey = "auto-gauge.simulated-configuration.v1"

    init() {
        if let data = UserDefaults.standard.data(forKey: draftKey),
           let draft = try? JSONDecoder().decode(GaugeConfiguration.self, from: data),
           (try? ConfigurationValidator.validate(draft, catalog: DemoData.catalog)) != nil {
            configuration = draft
            savedRevision = draft.revision
        }
    }
    func save() async {
        do {
            var candidate = configuration
            candidate.revision += 1
            try await transport.apply(candidate)
            let data = try JSONEncoder().encode(candidate)
            UserDefaults.standard.set(data, forKey: draftKey)
            configuration = candidate
            savedRevision = candidate.revision
            message = "Configurazione salvata nella simulazione e su questo iPhone. Nessun dispositivo reale modificato."
        } catch { message = error.localizedDescription }
    }
    func addDashboard() {
        guard configuration.dashboards.count < 3 else { return }
        configuration.dashboards.append(Dashboard(id: (1...255).first { candidate in !configuration.dashboards.contains { $0.id == candidate } }!, widgets: [
            GaugeWidget(pid: catalog[0], frame: .init(x: 0.25, y: 0.3, width: 0.5, height: 0.3))
        ]))
    }
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
                    Text("Ricerca Bluetooth e associazione esclusiva saranno disponibili dopo l’integrazione della sessione autenticata.")
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
                ForEach(model.configuration.dashboards.indices, id: \.self) { index in
                    NavigationLink {
                        DashboardEditorView(index: index)
                    } label: {
                        HStack {
                            Text(String(index + 1)).font(.headline.monospaced()).foregroundStyle(Palette.accent).frame(width: 26)
                            VStack(alignment: .leading) {
                                Text(model.configuration.dashboards[index].name)
                                Text("\(model.configuration.dashboards[index].widgets.count) misure").font(.caption).foregroundStyle(.secondary)
                            }
                        }
                    }
                }
                .onDelete { offsets in
                    guard model.configuration.dashboards.count - offsets.count >= 1 else { return }
                    model.configuration.dashboards.remove(atOffsets: offsets)
                }
                Button { model.addDashboard() } label: { Label("Aggiungi dashboard", systemImage: "plus.circle") }
                    .disabled(model.configuration.dashboards.count >= 3)
            } header: { Text("Le tue dashboard") } footer: { Text("Da 1 a 3 dashboard, fino a 5 misure per ciascuna.") }
            Section {
                Button("Salva nella simulazione") { Task { await model.save() } }
            } footer: { Text("Nessun invio al gauge reale. La bozza viene conservata su questo iPhone.") }
        }.navigationTitle("Dashboard")
    }
}

struct DashboardEditorView: View {
    @EnvironmentObject private var model: AppModel
    let index: Int
    @State private var selectedWidgetID: Int?
    var body: some View {
        if model.configuration.dashboards.indices.contains(index) {
            Form {
                Section {
                    GaugePreview(dashboard: model.configuration.dashboards[index])
                        .frame(maxWidth: .infinity).padding(.vertical, 12)
                    Text("ANTEPRIMA · VALORI DIMOSTRATIVI").font(.caption2.weight(.semibold)).foregroundStyle(Palette.muted)
                        .frame(maxWidth: .infinity)
                }.listRowBackground(Color.clear)
                Section("Dashboard") { Text(model.configuration.dashboards[index].name) }
                Section("Misure") {
                    ForEach(model.configuration.dashboards[index].widgets) { widget in
                        Button { selectedWidgetID = widget.id } label: {
                            HStack {
                                VStack(alignment: .leading) {
                                    Text(widget.pid.nameIT).foregroundStyle(.primary)
                                    Text(widget.pid.unit).font(.caption).foregroundStyle(.secondary)
                                }
                                Spacer()
                                Image(systemName: selectedWidgetID == widget.id ? "slider.horizontal.3" : "chevron.right")
                            }
                        }
                    }.onDelete { offsets in
                        guard model.configuration.dashboards[index].widgets.count - offsets.count >= 1 else { return }
                        model.configuration.dashboards[index].widgets.remove(atOffsets: offsets)
                    }
                    Menu {
                        ForEach(model.catalog) { pid in
                            Button(pid.nameIT) {
                                model.configuration.dashboards[index].widgets.append(GaugeWidget(id: (1...255).first { candidate in !model.configuration.dashboards[index].widgets.contains { $0.id == candidate } }!, pid: pid, frame: .init(x: 0.25, y: 0.35, width: 0.5, height: 0.2)))
                            }
                        }
                    } label: { Label("Aggiungi misura", systemImage: "plus") }
                    .disabled(model.configuration.dashboards[index].widgets.count >= 5)
                }
                if let widgetIndex = model.configuration.dashboards[index].widgets.firstIndex(where: { $0.id == selectedWidgetID }) {
                    Section("Posizione e dimensioni") {
                        geometrySlider("Posizione X", value: $model.configuration.dashboards[index].widgets[widgetIndex].frame.x)
                        geometrySlider("Posizione Y", value: $model.configuration.dashboards[index].widgets[widgetIndex].frame.y)
                        geometrySlider("Larghezza", value: $model.configuration.dashboards[index].widgets[widgetIndex].frame.width, minimum: 0.1)
                        geometrySlider("Altezza", value: $model.configuration.dashboards[index].widgets[widgetIndex].frame.height, minimum: 0.1)
                    }
                }
                Section {
                    Button("Salva nella simulazione") { Task { await model.save() } }
                } footer: { Text("L’editor libero è una proposta di sviluppo. Le geometrie fuori area vengono rifiutate al salvataggio.") }
            }.navigationTitle("Modifica dashboard").navigationBarTitleDisplayMode(.inline)
        }
    }
    private func geometrySlider(_ name: String, value: Binding<Double>, minimum: Double = 0) -> some View {
        VStack(alignment: .leading) {
            HStack { Text(name); Spacer(); Text("\(Int(value.wrappedValue * 100))%").foregroundStyle(.secondary).monospacedDigit() }
            Slider(value: value, in: minimum...1, step: 0.01)
        }
    }
}

struct GaugePreview: View {
    let dashboard: Dashboard
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
                    .position(x: size * (widget.frame.x + widget.frame.width / 2), y: size * (widget.frame.y + widget.frame.height / 2))
                }
            }.clipShape(Circle())
        }.frame(width: 250, height: 250)
        .accessibilityLabel("Anteprima statica della dashboard \(dashboard.name), con valori dimostrativi")
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
    var body: some View {
        List {
            Section {
                Label("Storici dimostrativi", systemImage: "clock").font(.headline)
                Text("Il gauge conserverà un buffer limitato e lo sincronizzerà con l’iPhone. Nessuna registrazione reale è stata importata.").font(.subheadline).foregroundStyle(.secondary)
            }
            Section("Esempio di sessione") {
                LabeledContent("Origine", value: "Simulazione locale")
                LabeledContent("Tempo assoluto", value: "Non disponibile")
                LabeledContent("Durata", value: "18 min · esempio")
                LabeledContent("Dati persi", value: "Segnalazione prevista")
            }
            Section {
                Button("Sincronizza dal gauge") {}.disabled(true)
            } footer: { Text("Nessuna telemetria live sull’iPhone. Resume, deduplica e persistenza delle sessioni reali sono da integrare.") }
        }.navigationTitle("Storici")
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
                LabeledContent("iPhone proprietario", value: "Non associato")
                Text("Il prodotto prevede un solo iPhone associato alla volta. Recupero e trasferimento di proprietà sono ancora da definire.").font(.footnote).foregroundStyle(.secondary)
            }
        }.navigationTitle("Account")
    }
}
