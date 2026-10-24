import SwiftUI
import GaugeCore

struct HistoryTimelineView: View {
    @Environment(AppModel.self) private var model
    @State private var timeline: HistoryTimelineModel
    let source: HistoryTimelineSource
    init(source: HistoryTimelineSource, initialSessionID: String? = nil) {
        self.source = source
        _timeline = State(initialValue: HistoryTimelineModel(preferredSession: initialSessionID))
    }
    private var input: HistoryTimelineInput {
        source == .simulatedGauge ? .gauge(model.logSessions, ready: model.logReady) : .cloud(model.cloud.restoredItems)
    }
    var body: some View {
        ScrollViewReader { proxy in
        List {
            Section {
                Label(source.titleIT, systemImage: source == .simulatedGauge ? "testtube.2" : "icloud.and.arrow.down").font(.headline)
                if let notice = timeline.notice { Label(notice, systemImage: "exclamationmark.triangle").foregroundStyle(AppTheme.warning) }
            }
            HistoryTimelineFiltersView(timeline: timeline)
            if timeline.preparing { Section { ProgressView("Preparazione grafico e tabella") } }
            if let projection = timeline.projection {
                HistoryTimelineChartView(projection: projection, definition: timeline.definition)
                HistoryTimelineOriginView(source: source, session: timeline.session, projection: projection)
                HistoryTimelineQualityView(projection: projection, source: source)
                HistoryTimelineTableView(timeline: timeline)
            } else if !timeline.preparing {
                Section { ContentUnavailableView("Nessuna serie disponibile", systemImage: "chart.xyaxis.line", description: Text("Importa o recupera uno storico, poi seleziona sessione, misura ed ECU. I raw restano consultabili nel dettaglio della sessione.")) }
            }
        }
        .navigationTitle("Misure nel tempo")
        .navigationBarTitleDisplayMode(.inline)
        .onChange(of: timeline.projection != nil) { _, ready in if ready { debugScroll(proxy) } }
        .task(id: input) { timeline.load(input) }
        .onDisappear(perform: timeline.cancel)
        }
    }
    private func debugScroll(_ proxy: ScrollViewProxy) {
        #if DEBUG
        let args = ProcessInfo.processInfo.arguments
        if args.contains("--history-timeline-chart") { proxy.scrollTo("timeline-chart", anchor: .top) }
        if args.contains("--history-timeline-table") { proxy.scrollTo("timeline-table", anchor: .top) }
        #endif
    }
}
