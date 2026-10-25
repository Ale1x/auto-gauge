import Foundation

/// One installation attempt, retained by the app across navigation. Only an
/// injected local provider can grant a route. No retry/reset API exists.
public actor OTAInstallationController {
    public nonisolated let configured: Bool
    private var manifest: OTAManifest?
    private let provider: (any OTAInstallationSessionProvider)?
    private let timeout: Duration
    private var session: (any OTAInstallationSession)?
    private var sender: OTATransferSender?
    private var state: OTAInstallationState = .idle
    private var progress = OTAInstallationProgress()
    private var selectionMayHaveChanged = false
    private var epoch: UInt64 = 0
    private var busy = false
    public init(provider: (any OTAInstallationSessionProvider)? = nil, operationTimeout: Duration = .seconds(60)) {
        self.provider = provider; configured = provider != nil; timeout = operationTimeout
    }
    public func snapshot() -> OTAInstallationSnapshot {
        .init(manifest: manifest, state: state, progress: progress, selectionMayHaveChanged: selectionMayHaveChanged)
    }
    public func upload(_ candidate: VerifiedOTACandidate, transferID: UInt32,
                       onProgress: @escaping @Sendable (OTAInstallationProgress) async -> Void = { _ in }) async throws {
        guard !busy else { throw OTATransferError.busy }
        guard state == .idle else { throw state == .unknown ? OTATransferError.outcomeUnknown : OTATransferError.notReady }
        guard let provider else { throw OTATransferError.unqualified }
        guard transferID != 0, timeout > .zero, timeout <= .seconds(15 * 60) else { throw OTATransferError.invalidPayload }
        try Task.checkCancellation()
        busy = true; defer { busy = false }
        let token = epoch; manifest = candidate.manifest; state = .opening
        do {
            let opened = try await Self.open(provider, candidate: candidate, deadline: .now.advanced(by: timeout))
            guard token == epoch else { await opened.close(); throw CancellationError() }
            session = opened
            try Task.checkCancellation()
            let sender = OTATransferSender(transport: opened, allowHostDevelopmentMutations: true, operationTimeout: timeout)
            self.sender = sender; state = .uploading
            try await sender.upload(candidate, transferID: transferID) { [weak self] value in
                if await self?.record(value, token: token) == true { await onProgress(value) }
            }
            guard token == epoch else { throw CancellationError() }
            state = .ready
        } catch {
            if token == epoch { state = .unknown; epoch &+= 1 }
            let old = session; session = nil
            await old?.close()
            throw error
        }
    }
    public func select() async throws {
        guard !busy else { throw OTATransferError.busy }
        guard state == .ready, let sender else { throw state == .unknown ? OTATransferError.outcomeUnknown : OTATransferError.notReady }
        try Task.checkCancellation()
        busy = true; defer { busy = false }
        let token = epoch
        state = .selecting; selectionMayHaveChanged = true
        do {
            try await sender.select()
            guard token == epoch else { throw CancellationError() }
            state = .selected
            let old = session; session = nil; await old?.close()
        } catch {
            if token == epoch { state = .unknown; epoch &+= 1 }
            let old = session; session = nil; await old?.close()
            throw error
        }
    }
    /// Stops local delivery and closes this lease. It never sends a blind cancel,
    /// retries selection or claims to undo flash writes that already occurred.
    public func cancel() async {
        guard state != .idle, state != .selected else { return }
        epoch &+= 1; state = .unknown
        let old = session; session = nil; await old?.close()
    }
    private static func open(_ provider: any OTAInstallationSessionProvider, candidate: VerifiedOTACandidate,
                             deadline: ContinuousClock.Instant) async throws -> any OTAInstallationSession {
        try await withThrowingTaskGroup(of: (any OTAInstallationSession).self) { group in
            group.addTask {
                let opened = try await provider.openSession(for: candidate, deadline: deadline)
                do {
                    try Task.checkCancellation()
                    guard ContinuousClock().now < deadline else { throw OTATransferError.timeout }
                    return opened
                } catch { await opened.close(); throw error }
            }
            group.addTask { try await ContinuousClock().sleep(until: deadline); throw OTATransferError.timeout }
            defer { group.cancelAll() }
            guard let opened = try await group.next() else { throw OTATransferError.timeout }
            do {
                try Task.checkCancellation()
                guard ContinuousClock().now < deadline else { throw OTATransferError.timeout }
                return opened
            } catch { await opened.close(); throw error }
        }
    }
    private func record(_ value: OTAInstallationProgress, token: UInt64) -> Bool {
        guard token == epoch, state == .uploading else { return false }
        progress = value; return true
    }
}
