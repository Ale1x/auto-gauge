#if os(macOS)
import Darwin
import Foundation
import XCTest
@testable import GaugeCore

/// Host-only chunk bridge; actual AG codecs/endpoint execute in the C process.
private final class OTAFramedProcess: @unchecked Sendable {
    let process: Process
    let input: FileHandle
    let output: FileHandle
    init(executable: URL, imageBytes: Int) throws {
        process = Process(); process.executableURL = executable
        process.arguments = ["--trusted-fixture-maintenance", "--image-bytes", String(imageBytes)]
        let stdin = Pipe(); let stdout = Pipe()
        input = stdin.fileHandleForWriting; output = stdout.fileHandleForReading
        process.standardInput = stdin; process.standardOutput = stdout; process.standardError = FileHandle.nullDevice
        try process.run()
        try stdin.fileHandleForReading.close(); try stdout.fileHandleForWriting.close()
    }
    func stop() { if process.isRunning { process.terminate() } }
    deinit { stop(); try? input.close(); try? output.close() }
    func chunk(_ bytes: Data) throws -> Data {
        var length = UInt32(bytes.count).littleEndian
        var message = withUnsafeBytes(of: &length) { Data($0) }; message.append(bytes)
        try input.write(contentsOf: message)
        let deadline = ContinuousClock().now.advanced(by: .seconds(5))
        func exact(_ count: Int) throws -> Data {
            var data = Data()
            while data.count < count {
                guard ContinuousClock().now < deadline else { throw OTATransferError.timeout }
                var descriptor = pollfd(fd: output.fileDescriptor, events: Int16(POLLIN), revents: 0)
                let ready = Darwin.poll(&descriptor, 1, 10)
                if ready < 0 && errno == EINTR { continue }
                guard ready >= 0 else { throw OTATransferError.notReady }
                if ready == 0 { continue }
                var buffer = [UInt8](repeating: 0, count: count - data.count)
                let readCount = Darwin.read(output.fileDescriptor, &buffer, buffer.count)
                if readCount < 0 && errno == EINTR { continue }
                guard readCount > 0 else { throw OTATransferError.notReady }
                data.append(contentsOf: buffer.prefix(readCount))
            }
            return data
        }
        let prefix = [UInt8](try exact(4))
        let count = (0..<4).reduce(UInt32(0)) { $0 | UInt32(prefix[$1]) << ($1 * 8) }
        guard count == 0 || count == 32 else { throw OTATransferError.invalidReply }
        return try exact(Int(count))
    }
}
private actor OTAFramedCPipe: GaugePacketPipe {
    let io: OTAFramedProcess
    var incoming = Data()
    var closed = false
    var dropSelectedACK: Bool
    var maxWrite = 0
    var receiveCount = 0
    var selectedACKs = 0
    init(executable: URL, imageBytes: Int, dropSelectedACK: Bool = false) throws {
        io = try OTAFramedProcess(executable: executable, imageBytes: imageBytes); self.dropSelectedACK = dropSelectedACK
    }
    func readyConnection() async throws -> GaugePipeConnection {
        guard !closed, io.process.isRunning else { throw GaugeLinkError.disconnected }
        return .init(generation: 1, maximumWriteBytes: 23)
    }
    func write(_ data: Data, generation: UInt64) async throws {
        guard !closed, generation == 1, (1...23).contains(data.count), incoming.isEmpty else { throw GaugeLinkError.disconnected }
        maxWrite = max(maxWrite, data.count)
        let io = self.io
        let response = try await withTaskCancellationHandler {
            try await Task.detached { try io.chunk(data) }.value
        } onCancel: { io.stop() }
        if !response.isEmpty {
            let frame = try GaugeFrameCodec.decode(response)
            let ack = try OTATransferCodec.decodeResponse(frame.payload)
            if ack.phase == .selected {
                selectedACKs += 1
                if dropSelectedACK { return }
            }
            incoming = response
        }
    }
    func receive(generation: UInt64) async throws -> GaugeIncomingChunk {
        guard !closed, generation == 1, !incoming.isEmpty else { throw OTATransferError.timeout }
        receiveCount += 1
        let data = Data(incoming.prefix(5)); incoming.removeFirst(data.count)
        return .init(data: data)
    }
    func disconnect(generation: UInt64) async { if generation == 1 { closed = true; io.stop() } }
    func metrics() -> (Int, Int, Int) { (maxWrite, receiveCount, selectedACKs) }
}
private struct OTAFramedExecutable {
    let directory: URL
    let executable: URL
    static let built: Result<Self, Error> = Result {
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("ag-ota-frame-" + UUID().uuidString)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        do {
            let executable = directory.appendingPathComponent("ota_framed")
            let process = Process(); process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
            process.arguments = ["cc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-I", root.appendingPathComponent("firmware/include").path,
                "-I", root.appendingPathComponent("shared/c").path] + ["firmware/src/ota_transfer.c", "firmware/src/ota_framed_endpoint.c",
                "shared/c/ag_frame.c", "shared/c/ag_stream.c", "firmware/tools/ota_framed_cli.c"].map { root.appendingPathComponent($0).path } + ["-o", executable.path]
            process.standardOutput = FileHandle.nullDevice; process.standardError = FileHandle.nullDevice
            try process.run(); process.waitUntilExit()
            guard process.terminationStatus == 0 else { throw OTATransferError.notReady }
            return Self(directory: directory, executable: executable)
        } catch { try? FileManager.default.removeItem(at: directory); throw error }
    }
}
final class OTAFramedCHostTests: XCTestCase {
    override class func tearDown() {
        if case let .success(built) = OTAFramedExecutable.built { try? FileManager.default.removeItem(at: built.directory) }
        super.tearDown()
    }
    private func candidate() throws -> VerifiedOTACandidate {
        let bundle = try OTAVectorBundle.generated.get(); let vector = try XCTUnwrap(bundle.cases.first { $0.expected_package })
        return try OTAPackageVerifier.verify(envelope: try XCTUnwrap(Data(base64Encoded: vector.envelope_base64)),
            image: try XCTUnwrap(Data(base64Encoded: bundle.image_base64)), trustedKeys: [bundle.key_id: try XCTUnwrap(Data(base64Encoded: bundle.public_key_der_base64))],
            hardwareID: bundle.hardware_id, inactiveCapacity: bundle.inactive_capacity, acceptedCounter: try XCTUnwrap(UInt64(bundle.accepted_counter)))
    }
    func testSenderThroughFragmentedAGToRealC() async throws {
        let candidate = try candidate(); let built = try OTAFramedExecutable.built.get()
        let pipe = try OTAFramedCPipe(executable: built.executable, imageBytes: candidate.imageSnapshot.count)
        let transport = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true)
        let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
        do {
            try await transport.open(); try await sender.upload(candidate, transferID: 91)
            let ready = await sender.state; XCTAssertEqual(ready, .ready)
            let before = await pipe.metrics(); XCTAssertEqual(before.0, 23); XCTAssertGreaterThan(before.1, 1); XCTAssertEqual(before.2, 0)
            try await sender.select(); let selected = await sender.state; XCTAssertEqual(selected, .selected)
            let after = await pipe.metrics(); XCTAssertEqual(after.2, 1)
        } catch { await transport.close(); throw error }
        await transport.close()
    }
    func testLostSelectedACKClosesSessionAndPreventsRetry() async throws {
        let candidate = try candidate(); let built = try OTAFramedExecutable.built.get()
        let pipe = try OTAFramedCPipe(executable: built.executable, imageBytes: candidate.imageSnapshot.count, dropSelectedACK: true)
        let transport = OTAPacketTransport(pipe: pipe, allowHostDevelopmentMutations: true)
        let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
        do {
            try await transport.open(); try await sender.upload(candidate, transferID: 92)
            do { try await sender.select(); XCTFail("Dropped select ACK must fail") } catch {}
            let unknown = await sender.state; XCTAssertEqual(unknown, .unknown)
            let mayHaveChanged = await sender.selectionMayHaveChanged; XCTAssertTrue(mayHaveChanged)
            do { try await sender.select(); XCTFail("No second select") } catch {}
            do { try await transport.open(); XCTFail("No reopen") } catch {}
            let metrics = await pipe.metrics(); XCTAssertEqual(metrics.2, 1)
        } catch { await transport.close(); throw error }
        await transport.close()
    }
}
#endif
