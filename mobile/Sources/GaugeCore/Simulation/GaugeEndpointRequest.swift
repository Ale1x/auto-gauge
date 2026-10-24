import Foundation

struct GaugeEndpointRequest: Codable {
    let op: String
    let configuration: GaugeConfiguration?
    let expectedRevision: Int?
    let sessionID: String?
    let afterSequence: LogUInt64?
    let limit: Int?
    let acknowledgedThrough: LogUInt64?
    enum CodingKeys: String, CodingKey { case op, configuration; case expectedRevision = "expected_revision", sessionID = "session_id", afterSequence = "after_sequence", limit, acknowledgedThrough = "acknowledged_through" }
    init(op: String, configuration: GaugeConfiguration? = nil, expectedRevision: Int? = nil, sessionID: String? = nil, afterSequence: LogUInt64? = nil, limit: Int? = nil, acknowledgedThrough: LogUInt64? = nil) {
        self.op = op; self.configuration = configuration; self.expectedRevision = expectedRevision
        self.sessionID = sessionID; self.afterSequence = afterSequence; self.limit = limit; self.acknowledgedThrough = acknowledgedThrough
    }
    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        op = try c.decode(String.self, forKey: .op)
        configuration = try? c.decode(GaugeConfiguration.self, forKey: .configuration)
        expectedRevision = try? c.decode(Int.self, forKey: .expectedRevision)
        sessionID = try? c.decode(String.self, forKey: .sessionID)
        afterSequence = try? c.decode(LogUInt64.self, forKey: .afterSequence)
        limit = c.contains(.limit) ? try? c.decode(Int.self, forKey: .limit) : 16
        acknowledgedThrough = try? c.decode(LogUInt64.self, forKey: .acknowledgedThrough)
    }
}
