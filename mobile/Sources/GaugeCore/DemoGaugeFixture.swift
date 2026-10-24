import Foundation

public struct DemoGaugeFixture: Sendable {
    public let configuration: GaugeConfiguration
    public let historySource: SimulatedHistorySource
    public static func load() throws -> Self {
        struct History: Decodable { let session_id: LogSessionID; let capacity: Int; let first_sequence: LogUInt64; let records: [HistoricalLogRecord] }
        struct Contents: Decodable { let initial_configuration: GaugeConfiguration; let history_source: History }
        guard let url = Bundle.module.url(forResource: "rpc-demo-fixture", withExtension: "json") else { throw LogImportError.invalidPage }
        let data = try Data(contentsOf: url)
        guard data.count <= 32 * 1024 else { throw LogImportError.capacity }
        let contents = try JSONDecoder().decode(Contents.self, from: data)
        let history = try SimulatedHistorySource(sessionID: contents.history_source.session_id, records: contents.history_source.records, capacity: contents.history_source.capacity, firstSequence: contents.history_source.first_sequence.value)
        return Self(configuration: contents.initial_configuration, historySource: history)
    }
}
