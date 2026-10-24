import Foundation
import Observation
import GaugeCore

@MainActor @Observable
final class HistoryTimelineModel {
    private(set) var sessions: [HistoryTimelineSession] = []
    private(set) var pidChoices: [Int] = []
    private(set) var ecuChoices: [UInt32] = []
    private(set) var projection: HistoryTimelineProjection?
    private(set) var tableRows: [HistoryTimelineRow] = []
    private(set) var preparing = false
    private(set) var notice: String?
    private(set) var page = 0
    private var sessionSelection: String?
    private var pidSelection: Int?
    private var ecuSelection: UInt32?
    private let preferredSession: String?
    private var initialSelectionApplied = false
    private var generation = 0
    @ObservationIgnored private var work: Task<Void, Never>?
    static let rowsPerPage = 50
    init(preferredSession: String?) { self.preferredSession = preferredSession }
    var selectedSessionID: String? {
        get { sessionSelection }
        set { guard newValue != sessionSelection else { return }; sessionSelection = newValue; updateOptions() }
    }
    var selectedPID: Int? {
        get { pidSelection }
        set { guard newValue != pidSelection else { return }; pidSelection = newValue; updateECUs() }
    }
    var selectedECU: UInt32? {
        get { ecuSelection }
        set { guard newValue != ecuSelection else { return }; ecuSelection = newValue; prepare() }
    }
    var session: HistoryTimelineSession? { sessions.first { $0.id == sessionSelection } }
    var definition: HistoryPIDDefinition? { pidSelection.flatMap(HistoryPIDDefinition.find) }
    var pageCount: Int { max(1, ((projection?.rows.count ?? 0) + Self.rowsPerPage - 1) / Self.rowsPerPage) }
    func load(_ input: HistoryTimelineInput) {
        do {
            switch input {
            case .gauge(let imported, let ready):
                sessions = try HistoryTimelineSession.imported(imported)
                notice = ready ? nil : "Ultima copia locale disponibile: la ricarica dell’archivio non è stata verificata. Torna agli storici per ricaricarla."
            case .cloud(let items): sessions = try HistoryTimelineSession.cloud(items); notice = nil
            }
            if !sessions.contains(where: { $0.id == sessionSelection }) {
                sessionSelection = !initialSelectionApplied ? sessions.first(where: { $0.id == preferredSession })?.id : nil
                if sessionSelection == nil { sessionSelection = sessions.first?.id }
            }
            initialSelectionApplied = true
            updateOptions()
        } catch {
            cancel(); sessions = []; projection = nil; tableRows = []; pidChoices = []; ecuChoices = []
            notice = "Archivio non analizzabile. Nessun dato originale è stato modificato. \(error.localizedDescription)"
        }
    }
    private func updateOptions() {
        pidChoices = Array(Set(session?.records.map(\.pid) ?? [])).sorted()
        if !pidChoices.contains(where: { $0 == pidSelection }) { pidSelection = pidChoices.first }
        updateECUs()
    }
    private func updateECUs() {
        ecuChoices = Array(Set(session?.records.filter { $0.pid == pidSelection }.map(\.ecuSource) ?? [])).sorted()
        if !ecuChoices.contains(where: { $0 == ecuSelection }) { ecuSelection = ecuChoices.first }
        prepare()
    }
    private func prepare() {
        generation += 1; work?.cancel(); page = 0; projection = nil; tableRows = []
        guard let session else { preparing = false; return }
        let pid = pidSelection; let ecu = ecuSelection
        preparing = true; let token = generation
        work = Task { [weak self] in
            let background = Task.detached { try HistoryTimelineProjector.project(session, pid: pid, ecu: ecu) }
            do {
                let result = try await withTaskCancellationHandler { try await background.value } onCancel: { background.cancel() }
                guard let self, !Task.isCancelled, self.generation == token else { return }
                self.projection = result; self.preparing = false; self.updateRows()
            } catch {
                guard let self, !Task.isCancelled, self.generation == token else { return }
                self.preparing = false; self.notice = "Analisi non disponibile: \(error.localizedDescription)"
            }
        }
    }
    func cancel() { generation += 1; work?.cancel(); work = nil; preparing = false }
    func previousPage() { guard page > 0 else { return }; page -= 1; updateRows() }
    func nextPage() { guard page + 1 < pageCount else { return }; page += 1; updateRows() }
    private func updateRows() {
        guard let rows = projection?.rows else { tableRows = []; return }
        let start = page * Self.rowsPerPage
        tableRows = Array(rows.dropFirst(start).prefix(Self.rowsPerPage))
    }
    static func pidTitle(_ pid: Int) -> String {
        let hex = String(pid, radix: 16).uppercased()
        return HistoryPIDDefinition.find(pid).map { "\($0.nameIT) · PID \(hex)" } ?? "PID \(hex) · formula non disponibile nell’app"
    }
    static func ecuTitle(_ ecu: UInt32) -> String { "ECU 0x" + String(ecu, radix: 16).uppercased() }
}
