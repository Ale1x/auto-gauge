#if os(macOS)
import Foundation
import Darwin
@testable import GaugeCore

/// Test-only subprocess I/O. No Process or fixture admission is included in the iOS app.
final class CHostProcessIO: @unchecked Sendable {
    let process: Process
    private let input: FileHandle
    private let output: FileHandle
    private let lock = NSLock()
    private var stopped = false
    private var readCount = 0
    private var outputClosed = false
    init(executable: URL, store: URL, control: URL, seed: Bool) throws {
        process = Process(); process.executableURL = executable
        process.arguments = ["--trusted-fixture-admission", "--store-file", store.path, "--control-file", control.path] + (seed ? ["--seed-initial"] : [])
        let stdin = Pipe(); let stdout = Pipe()
        input = stdin.fileHandleForWriting; output = stdout.fileHandleForReading
        process.standardInput = stdin; process.standardOutput = stdout; process.standardError = FileHandle.nullDevice
        try process.run()
        try stdin.fileHandleForReading.close(); try stdout.fileHandleForWriting.close()
    }
    var running: Bool {
        lock.lock(); defer { lock.unlock() }
        return !stopped && process.isRunning
    }
    func write(_ data: Data) throws { try input.write(contentsOf: data) }
    private func beginRead() throws -> Int32 {
        lock.lock(); defer { lock.unlock() }
        guard !stopped, !outputClosed else { throw GaugeLinkError.disconnected }
        readCount += 1; return output.fileDescriptor
    }
    private func finishRead() { lock.lock(); readCount -= 1; lock.unlock() }
    private var hasReader: Bool { lock.lock(); defer { lock.unlock() }; return readCount != 0 }
    private func closeOutput() {
        lock.lock(); defer { lock.unlock() }
        if !outputClosed { outputClosed = true; try? output.close() }
    }
    func confirmStopped() async throws {
        stop()
        let deadline = ContinuousClock().now.advanced(by: .seconds(2))
        while process.isRunning || hasReader {
            guard ContinuousClock().now < deadline else { throw GaugeLinkError.timeout }
            try await Task.sleep(for: .milliseconds(2))
        }
        closeOutput()
    }
    func read() async throws -> Data {
        let descriptor = try beginRead(); defer { finishRead() }
        return try await withTaskCancellationHandler {
            try Task.checkCancellation()
            return try await Task.detached {
                // Foundation read(upToCount:) may wait for the full count on a pipe.
                // One POSIX read returns available stream bytes without demanding a whole frame.
                var bytes = [UInt8](repeating: 0, count: 512)
                while true {
                    let count = bytes.withUnsafeMutableBytes { buffer -> Int in
                        guard let base = buffer.baseAddress else { return -1 }
                        return Darwin.read(descriptor, base, buffer.count)
                    }
                    if count < 0 && errno == EINTR { continue }
                    guard count >= 0 else { throw NSError(domain: NSPOSIXErrorDomain, code: Int(errno)) }
                    return Data(bytes.prefix(count))
                }
            }.value
        } onCancel: { self.stop() }
    }
    func stop() {
        lock.lock(); defer { lock.unlock() }
        guard !stopped else { return }; stopped = true
        if process.isRunning { process.terminate() }
        try? input.close()
    }
}

struct CHostCompiler {
    static let result: Result<(binary: URL, directory: URL), any Error> = Result {
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("auto-gauge-c-build-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let tool = ["/opt/homebrew/bin/cmake", "/usr/local/bin/cmake", "/usr/bin/cmake"].first { FileManager.default.isExecutableFile(atPath: $0) }
        let executable = URL(fileURLWithPath: tool ?? "/usr/bin/env")
        for arguments in [["-S", root.appendingPathComponent("firmware").path, "-B", directory.path, "-DAG_SANITIZERS=OFF"], ["--build", directory.path, "--target", "gauge_device_endpoint_cli", "-j", "2"]] {
            let process = Process(); process.executableURL = executable
            process.arguments = (tool == nil ? ["cmake"] : []) + arguments
            let log = directory.appendingPathComponent("compile.log")
            _ = FileManager.default.createFile(atPath: log.path, contents: nil)
            let output = try FileHandle(forWritingTo: log)
            process.standardOutput = output; process.standardError = output
            try process.run(); process.waitUntilExit(); try output.close()
            guard process.terminationStatus == 0 else {
                let file = try FileHandle(forReadingFrom: log); defer { try? file.close() }
                let bytes = try file.read(upToCount: 4096) ?? Data()
                throw NSError(domain: "CHostCompiler", code: Int(process.terminationStatus), userInfo: [NSLocalizedDescriptionKey: String(decoding: bytes, as: UTF8.self)])
            }
        }
        return (directory.appendingPathComponent("gauge_device_endpoint_cli"), directory)
    }
}

actor CHostEndpointHub {
    let executable: URL
    let storeFile: URL
    let controlFile: URL
    private var io: CHostProcessIO?
    private var active: String?
    private var connecting = false
    private var generation: UInt64 = 0
    private var readingGeneration: UInt64?
    private(set) var requestIDs: [UInt32] = []
    private(set) var operations: [String] = []
    private var requestDecoder = GaugeStreamReassembler()
    private var responseDecoder = GaugeStreamReassembler()
    private var pendingID: UInt32?
    private var pendingOperation: String?
    private var streamFailed = false
    init(executable: URL, directory: URL) {
        self.executable = executable; storeFile = directory.appendingPathComponent("device-config.bin"); controlFile = directory.appendingPathComponent("control.txt")
    }
    func connect(client: String) async throws -> UInt64 {
        guard active == nil, !connecting else { throw GaugeLinkError.busy }
        connecting = true; defer { connecting = false }
        guard client == "a" || client == "b" else { throw GaugeLinkError.notReady }
        let startProcess = io?.running != true
        if startProcess, let io { try await io.confirmStopped() }
        // Validate/write the sidecar before publishing a lease or starting a new writer.
        // Failure leaves active=nil and cannot strand an inaccessible connection.
        try command("connect-\(client)")
        if startProcess {
            io = try CHostProcessIO(executable: executable, store: storeFile, control: controlFile, seed: !FileManager.default.fileExists(atPath: storeFile.path))
        }
        generation += 1; active = client; requestDecoder.reset(); responseDecoder.reset(); pendingID = nil; pendingOperation = nil; streamFailed = false
        return generation
    }
    func command(_ name: String) throws { try Data((name + "\n").utf8).write(to: controlFile, options: .atomic) }
    func write(_ data: Data, generation lease: UInt64) throws {
        guard lease == generation, active != nil, let io, io.running else { throw GaugeLinkError.disconnected }
        for frame in try requestDecoder.feed(data, at: ContinuousClock().now) {
            guard pendingID == nil else { throw GaugeLinkError.busy }
            let operation = try JSONDecoder().decode(GaugeEndpointRequest.self, from: frame.payload).op
            requestIDs.append(frame.requestID); operations.append(operation)
            pendingID = frame.requestID; pendingOperation = operation
        }
        try io.write(data)
    }
    func receive(generation lease: UInt64) async throws -> GaugeIncomingChunk {
        guard lease == generation, active != nil, readingGeneration != lease, let io else { throw GaugeLinkError.disconnected }
        readingGeneration = lease
        defer { if readingGeneration == lease { readingGeneration = nil } }
        let data = try await io.read()
        guard lease == generation, active != nil, !data.isEmpty else { throw GaugeLinkError.disconnected }
        do {
            for frame in try responseDecoder.feed(data, at: ContinuousClock().now) {
                guard frame.kind == .response, frame.requestID == pendingID else { throw GaugeLinkError.invalidReply }
                let reply = try JSONDecoder().decode(GaugeRPCResponse.self, from: frame.payload)
                if !(pendingOperation == "config.apply" && reply.phase == "received" && reply.error == nil) {
                    pendingID = nil; pendingOperation = nil
                }
            }
        } catch { streamFailed = true; throw error }
        return .init(data: data)
    }
    func disconnect(generation lease: UInt64) async {
        guard lease == generation else { return }
        // Unread old responses or partial request bytes must never become a new lease's ID1.
        // Clean A/B/A handoffs keep the process; a dirty disconnect restarts it from the store file.
        let dirty = readingGeneration == lease || pendingID != nil || requestDecoder.bufferedBytes != 0 || responseDecoder.bufferedBytes != 0 || streamFailed
        let oldIO = io
        if dirty { oldIO?.stop() }
        active = nil; generation += 1; requestDecoder.reset(); responseDecoder.reset(); pendingID = nil; pendingOperation = nil
        if dirty, let oldIO { try? await oldIO.confirmStopped() }
    }
    func shutdown() async {
        let old = io; old?.stop()
        if let old { try? await old.confirmStopped() }
        io = nil; active = nil; generation += 1
    }
    func pendingReadGeneration() -> UInt64? { readingGeneration }
    func processIdentifier() -> Int32? { io?.process.processIdentifier }
}

actor CHostPacketPipe: DevelopmentGaugePacketPipe {
    let hub: CHostEndpointHub
    let client: String
    private var lease: UInt64?
    init(hub: CHostEndpointHub, client: String) { self.hub = hub; self.client = client }
    func connect() async throws {
        if lease != nil { return }
        lease = try await hub.connect(client: client)
    }
    func readyConnection() async throws -> GaugePipeConnection {
        guard let lease else { throw GaugeLinkError.disconnected }; return .init(generation: lease, maximumWriteBytes: 20)
    }
    func write(_ data: Data, generation: UInt64) async throws {
        guard lease == generation else { throw GaugeLinkError.disconnected }; try await hub.write(data, generation: generation)
    }
    func receive(generation: UInt64) async throws -> GaugeIncomingChunk {
        guard lease == generation else { throw GaugeLinkError.disconnected }; return try await hub.receive(generation: generation)
    }
    func disconnect(generation: UInt64) async {
        guard lease == generation else { return }; lease = nil; await hub.disconnect(generation: generation)
    }
}
#endif
