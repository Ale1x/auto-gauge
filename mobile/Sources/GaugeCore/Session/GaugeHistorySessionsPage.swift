import Foundation

/// Lexicographic non-snapshot catalog page. SID ordering is never chronological.
public struct GaugeHistorySessionsPage: Sendable, Equatable {
    public let sessions: [LogSessionID]
    public let hasMore: Bool
    public let nextAfterSessionID: LogSessionID?
    static func decode(_ data: Data, after: String?, limit: Int) throws -> Self {
        try StrictLogJSON.check(data)
        guard (1...16).contains(limit), let object = try JSONSerialization.jsonObject(with: data) as? [String: Any],
              Set(object.keys) == ["op", "phase", "sessions", "has_more", "next_after_session_id"],
              object["op"] as? String == "logs.sessions", object["phase"] as? String == "sessions" else { throw GaugeLinkError.invalidReply }
        let reply = try JSONDecoder().decode(GaugeRPCResponse.self, from: data)
        guard let sessions = reply.sessions, let more = reply.hasMore, sessions.count <= limit,
              Set(sessions).count == sessions.count,
              sessions.map(\.value) == sessions.map(\.value).sorted(),
              sessions.allSatisfy({ after == nil || $0.value > (after ?? "") }) else { throw GaugeLinkError.invalidReply }
        let next = try reply.nextAfterSessionID.map(LogSessionID.init)
        guard more ? (!sessions.isEmpty && next == sessions.last) : (next == nil) else { throw GaugeLinkError.invalidReply }
        return .init(sessions: sessions, hasMore: more, nextAfterSessionID: next)
    }
}
