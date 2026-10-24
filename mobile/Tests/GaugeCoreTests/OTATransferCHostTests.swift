#if os(macOS)
import Darwin
import Foundation
import XCTest
@testable import GaugeCore

/// Host-only direct payload stdio harness. No SDK, flash, PSA or radio runs here.
private final class OTACProcessIO: @unchecked Sendable {
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
    func exchange(_ bytes: Data, deadline: ContinuousClock.Instant) throws -> Data {
        var count = UInt32(bytes.count).littleEndian
        var message = withUnsafeBytes(of: &count) { Data($0) }; message.append(bytes)
        try input.write(contentsOf: message)
        func readExact(_ count: Int) throws -> Data {
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
        let size = [UInt8](try readExact(4))
        let length = (0..<4).reduce(UInt32(0)) { $0 | UInt32(size[$1]) << ($1 * 8) }
        guard length == 16 else { throw OTATransferError.invalidReply }
        return try readExact(16)
    }
}
private actor OTACHostTransport: OTATransferTransport {
    let io: OTACProcessIO
    var opcodes: [OTATransferOpcode] = []
    let dropImageACK: Bool
    init(executable: URL, imageBytes: Int, dropImageACK: Bool = false) throws {
        io = try OTACProcessIO(executable: executable, imageBytes: imageBytes); self.dropImageACK = dropImageACK
    }
    func currentGeneration() async throws -> UInt64 {
        guard io.process.isRunning else { throw OTATransferError.notReady }; return 1
    }
    func exchange(_ payload: Data, generation: UInt64, deadline: ContinuousClock.Instant) async throws -> Data {
        guard generation == 1 else { throw OTATransferError.generationChanged }
        let request = try OTATransferCodec.decodeRequest(payload); opcodes.append(request.opcode)
        let io = self.io
        let response = try await withTaskCancellationHandler {
            try await Task.detached { try io.exchange(payload, deadline: deadline) }.value
        } onCancel: { io.stop() }
        if dropImageACK && request.opcode == .image { throw OTATransferError.timeout }
        return response
    }
    func stop() { io.stop() }
}
private struct OTACExecutable {
    let directory: URL
    let executable: URL
    static let built: Result<Self, Error> = Result {
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("ag-ota-c-" + UUID().uuidString)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        do {
            let executable = directory.appendingPathComponent("ota_transfer")
            let process = Process(); process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
            process.arguments = ["cc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-I", root.appendingPathComponent("firmware/include").path,
                root.appendingPathComponent("firmware/src/ota_transfer.c").path,
                root.appendingPathComponent("firmware/tools/ota_transfer_cli.c").path, "-o", executable.path]
            process.standardOutput = FileHandle.nullDevice; process.standardError = FileHandle.nullDevice
            try process.run(); process.waitUntilExit()
            guard process.terminationStatus == 0 else { throw OTATransferError.notReady }
            return Self(directory: directory, executable: executable)
        } catch { try? FileManager.default.removeItem(at: directory); throw error }
    }
}
final class OTATransferCHostTests: XCTestCase {
    override class func tearDown() {
        if case let .success(built) = OTACExecutable.built { try? FileManager.default.removeItem(at: built.directory) }
        super.tearDown()
    }
    private func candidate() throws -> VerifiedOTACandidate {
        let bundle = try OTAVectorBundle.generated.get()
        let fixture = try XCTUnwrap(bundle.cases.first { $0.expected_package })
        return try OTAPackageVerifier.verify(envelope: try XCTUnwrap(Data(base64Encoded: fixture.envelope_base64)),
            image: try XCTUnwrap(Data(base64Encoded: bundle.image_base64)),
            trustedKeys: [bundle.key_id: try XCTUnwrap(Data(base64Encoded: bundle.public_key_der_base64))],
            hardwareID: bundle.hardware_id, inactiveCapacity: bundle.inactive_capacity,
            acceptedCounter: try XCTUnwrap(UInt64(bundle.accepted_counter)))
    }
    func testRealCUploadStopsBeforeSeparateSelect() async throws {
        let candidate = try candidate(); let built = try OTACExecutable.built.get()
        let transport = try OTACHostTransport(executable: built.executable, imageBytes: candidate.imageSnapshot.count)
        let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
        do {
            try await sender.upload(candidate, transferID: 41)
            let state = await sender.state; XCTAssertEqual(state, .ready)
            let before = await transport.opcodes; XCTAssertFalse(before.contains(.select))
            let acknowledged = await sender.imageBytesAcknowledged; XCTAssertEqual(acknowledged, UInt32(candidate.imageSnapshot.count))
            try await sender.select()
            let selected = await sender.state; XCTAssertEqual(selected, .selected)
            let after = await transport.opcodes; XCTAssertEqual(after.filter { $0 == .select }.count, 1)
        } catch { await transport.stop(); throw error }
        await transport.stop()
    }
    func testLostACKFromRealCPreventsRetryOrSelect() async throws {
        let candidate = try candidate(); let built = try OTACExecutable.built.get()
        let transport = try OTACHostTransport(executable: built.executable, imageBytes: candidate.imageSnapshot.count, dropImageACK: true)
        let sender = OTATransferSender(transport: transport, allowHostDevelopmentMutations: true)
        do { try await sender.upload(candidate, transferID: 42); XCTFail("Lost ACK must fail") } catch {}
        let state = await sender.state; XCTAssertEqual(state, .unknown)
        do { try await sender.upload(candidate, transferID: 43); XCTFail("No restart") } catch {}
        do { try await sender.select(); XCTFail("No select") } catch {}
        let sent = await transport.opcodes
        XCTAssertEqual(sent.filter { $0 == .image }.count, 1); XCTAssertFalse(sent.contains(.select))
        await transport.stop()
    }
}
#endif
