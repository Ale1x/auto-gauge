/// Optional pipe lifecycle for exclusive native RPC sessions. A complete exchange
/// owns the receive window, including RECEIVED and final APPLIED frames.
public protocol GaugeRPCExchangePipe: GaugePacketPipe {
    func beginRequest(generation: UInt64) async throws
    func endRequest(generation: UInt64) async throws
}
