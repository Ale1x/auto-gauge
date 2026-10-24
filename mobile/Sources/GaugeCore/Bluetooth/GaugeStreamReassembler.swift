import Foundation

/// One incomplete frame, no magic scanning or resynchronization after corruption.
public struct GaugeStreamReassembler: Sendable {
    public static let maximumBufferedBytes = 4112
    public static let maximumChunkBytes = 512
    private var buffer = Data()
    private var expectedBytes: Int?
    private var firstByte: ContinuousClock.Instant?
    private let frameTimeout: Duration
    public init(frameTimeout: Duration = .seconds(10)) { self.frameTimeout = frameTimeout }
    public var bufferedBytes: Int { buffer.count }
    public var deadline: ContinuousClock.Instant? { firstByte.map { $0.advanced(by: frameTimeout) } }
    public mutating func reset() { buffer.removeAll(keepingCapacity: false); expectedBytes = nil; firstByte = nil }
    public mutating func checkTimeout(at now: ContinuousClock.Instant) throws {
        if let deadline, now >= deadline { reset(); throw GaugeLinkError.timeout }
    }
    public mutating func feed(_ chunk: Data, at now: ContinuousClock.Instant) throws -> [GaugeFrame] {
        do {
            try checkTimeout(at: now)
            guard !chunk.isEmpty, chunk.count <= Self.maximumChunkBytes else { throw GaugeLinkError.invalidChunk }
            var output: [GaugeFrame] = []
            var offset = chunk.startIndex
            while offset < chunk.endIndex {
                if buffer.isEmpty { firstByte = now }
                let target = expectedBytes ?? 12
                let count = min(target - buffer.count, chunk.endIndex - offset)
                guard count > 0, buffer.count + count <= Self.maximumBufferedBytes else { throw GaugeLinkError.invalidChunk }
                buffer.append(contentsOf: chunk[offset..<(offset + count)]); offset += count
                if expectedBytes == nil && buffer.count == 12 {
                    let bytes = Array(buffer)
                    guard bytes[0] == 0x41, bytes[1] == 0x47 else { throw FrameCodecError.magic }
                    guard bytes[2] == 1 else { throw FrameCodecError.version }
                    guard let kind = GaugeFrameKind(rawValue: bytes[3]) else { throw FrameCodecError.kind }
                    let id = UInt32(bytes[4]) | UInt32(bytes[5]) << 8 | UInt32(bytes[6]) << 16 | UInt32(bytes[7]) << 24
                    guard kind == .event ? id == 0 : id != 0 else { throw FrameCodecError.requestID }
                    guard bytes[10] == 0, bytes[11] == 0 else { throw FrameCodecError.reserved }
                    let length = Int(bytes[8]) | Int(bytes[9]) << 8
                    guard length <= GaugeFrameCodec.maximumPayload else { throw FrameCodecError.length }
                    expectedBytes = length + GaugeFrameCodec.overhead
                }
                if let expectedBytes, buffer.count == expectedBytes {
                    output.append(try GaugeFrameCodec.decode(buffer)); reset()
                }
            }
            return output
        } catch { reset(); throw error }
    }
}
