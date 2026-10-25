import Foundation
@testable import GaugeCore

/// Behavioral native-shaped server for local authorization/data-flow tests only.
actor QualifiedSessionTestPipe: GaugeRPCExchangePipe {
    var generation: UInt64 = 71
    var connected = true
    var simulation = false
    var deviceID = "QUALIFIED-HOST-TEST"
    var configuration = DemoData.configuration
    var incoming: [GaugeIncomingChunk] = []
    var decoder = GaugeStreamReassembler()
    var opcodes: [String] = []
    var window = false
    var begins = 0
    var ends = 0
    var disconnects = 0
    var dropApplied = false
    var revokeOnApply: QualifiedSessionTestProvider?
    var committedArchive: QualifiedSessionTestStorage?
    var acks = 0
    var sessions = [String(repeating: "01", count: 16), String(repeating: "02", count: 16)]
    func setSimulation(_ value: Bool) { simulation = value }
    func setDeviceID(_ value: String) { deviceID = value }
    func setDropApplied(_ value: Bool) { dropApplied = value }
    func setRevokeOnApply(_ value: QualifiedSessionTestProvider) { revokeOnApply = value }
    func requireArchive(_ value: QualifiedSessionTestStorage) { committedArchive = value }
    func reconnect() { generation += 1; connected = true; decoder.reset(); incoming = []; window = false; revokeOnApply = nil; dropApplied = false }
    func readyConnection() async throws -> GaugePipeConnection {
        guard connected else { throw GaugeLinkError.disconnected }; return .init(generation: generation, maximumWriteBytes: 31)
    }
    func beginRequest(generation: UInt64) async throws {
        guard connected, generation == self.generation, !window, incoming.isEmpty else { throw GaugeLinkError.busy }
        window = true; begins += 1
    }
    func endRequest(generation: UInt64) async throws {
        guard connected, generation == self.generation, window, incoming.isEmpty, decoder.bufferedBytes == 0 else { throw GaugeLinkError.invalidReply }
        window = false; ends += 1
    }
    func write(_ data: Data, generation: UInt64) async throws {
        guard connected, generation == self.generation, window else { throw GaugeLinkError.disconnected }
        let frames = try decoder.feed(data, at: .now)
        for frame in frames {
            guard let request = try JSONSerialization.jsonObject(with: frame.payload) as? [String: Any], let op = request["op"] as? String else { throw GaugeLinkError.invalidReply }
            opcodes.append(op)
            var replies: [[String: Any]] = []
            var base: [String: Any] = ["op": op, "simulation": simulation]
            switch op {
            case "hello":
                base["protocol_version"] = 1; base["authenticated"] = true; base["device_id"] = deviceID
                base["supported_service01_pids"] = [4,5,12,13,15,16,17]; replies = [base]
            case "config.get":
                base["phase"] = "current"; base["persistence"] = "durable"
                base["configuration"] = try JSONSerialization.jsonObject(with: JSONEncoder().encode(configuration)); replies = [base]
            case "config.validate": base["phase"] = "validated"; replies = [base]
            case "config.apply":
                configuration = try JSONDecoder().decode(GaugeConfiguration.self, from: JSONSerialization.data(withJSONObject: request["configuration"] as Any))
                base["phase"] = "received"; replies = [base]
                if !dropApplied { base["phase"] = "applied"; base["persistence"] = "durable"; base["applied_revision"] = configuration.revision; replies.append(base) }
                if let revokeOnApply { await revokeOnApply.setAllowed(false) }
            case "logs.sessions":
                let after = request["after_session_id"] as? String; let limit = request["limit"] as? Int ?? 16
                let remaining = sessions.filter { after == nil || $0 > (after ?? "") }
                let page = Array(remaining.prefix(limit)); let more = remaining.count > page.count
                replies = [["op": op, "phase": "sessions", "sessions": page, "has_more": more, "next_after_session_id": more ? (page.last as Any) : NSNull()]]
            case "logs.read":
                let session = try LogSessionID(request["session_id"] as? String ?? "")
                let after = UInt64(request["after_sequence"] as? String ?? "0") ?? 0
                let records: [HistoricalLogRecord] = after == 0 ? [try .init(sequence: 1, monotonicMS: 100, pid: 4, ecuSource: 0x7e8, status: .valid, rawHex: "01")] : []
                let page = try HistoricalLogPage(sessionID: session, records: records)
                base["page"] = try JSONSerialization.jsonObject(with: JSONEncoder().encode(page)); replies = [base]
            case "logs.ack":
                guard committedArchive?.data() != nil else { throw GaugeLinkError.invalidReply }
                acks += 1; base["session_id"] = request["session_id"]; base["acknowledged_through"] = request["acknowledged_through"]; base["source_records_deleted"] = false; replies = [base]
            default: base["phase"] = "rejected"; base["error"] = "not_implemented"; replies = [base]
            }
            for reply in replies {
                let packet = try GaugeFrameCodec.encode(.init(kind: .response, requestID: frame.requestID, payload: JSONSerialization.data(withJSONObject: reply)))
                for at in stride(from: 0, to: packet.count, by: 83) { incoming.append(.init(data: packet.subdata(in: at..<min(packet.count, at + 83)))) }
            }
        }
    }
    func receive(generation: UInt64) async throws -> GaugeIncomingChunk {
        guard connected, generation == self.generation, window else { throw GaugeLinkError.disconnected }
        guard !incoming.isEmpty else { throw GaugeLinkError.timeout }; return incoming.removeFirst()
    }
    func disconnect(generation: UInt64) async {
        guard generation == self.generation else { return }; disconnects += 1; connected = false; window = false; incoming = []; decoder.reset()
    }
}
