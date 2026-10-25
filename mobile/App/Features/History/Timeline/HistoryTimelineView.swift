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
    private var importedSession: ImportedLogSession? {
        model.logSessions.first { $0.id == timeline.session?.id }
    }
    private var title: String {
        importedSession.map { model.historyTitle(for: $0) } ?? "Il tuo viaggio"
    }

    var body: some View {
        ScrollViewReader { proxy in
            List {
                Section {
                    VStack(alignment: .leading, spacing: AppTheme.smallSpacing) {
                        Label(title, systemImage: "car.side.fill").font(.title2).bold()
                        if let session = importedSession, let date = model.historyDate(for: session) {
                            Text(date, format: .dateTime.weekday(.wide).day().month(.wide).hour().minute())
                                .foregroundStyle(.secondary)
                        }
                        if let session = timeline.session {
                            Label(HistoryPresentation.duration(session.records), systemImage: "clock")
                                .foregroundStyle(.secondary)
                        }
                    }.padding(.vertical, AppTheme.smallSpacing)
                    if let notice = timeline.notice { Text(notice).foregroundStyle(.secondary) }
                }
                HistoryTimelineFiltersView(timeline: timeline)
                if timeline.preparing { Section { ProgressView("Preparazione del grafico") } }
                if let projection = timeline.projection {
                    HistoryTimelineChartView(projection: projection, definition: timeline.definition)
                    HistoryTimelineQualityView(projection: projection, source: source)
                    HistoryTimelineTableView(timeline: timeline)
                } else if !timeline.preparing {
                    Section {
                        ContentUnavailableView("Nessuna misura disponibile", systemImage: "chart.xyaxis.line",
                            description: Text("Aggiorna i viaggi per vedere le misure salvate."))
                    }
                }
            }
            .navigationTitle("Dettaglio viaggio")
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
