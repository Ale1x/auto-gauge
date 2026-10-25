import Foundation

/// Local preparation only. File IO and synchronous verification stay on this actor,
/// outside MainActor. Saved bytes are untrusted on reopen and always reverified.
public actor OTAPackagePreparation {
    private let archiveURL: URL
    private var envelope: Data?
    private var image: Data?
    private var manifestFile: OTAPreparationSelection?
    private var imageFile: OTAPreparationSelection?
    private var candidate: VerifiedOTACandidate?
    private static let header = Data([0x41, 0x47, 0x4f, 0x50, 1, 0, 0, 0])
    public init(archiveURL: URL) { self.archiveURL = archiveURL }
    public func snapshot() -> OTAPreparationSnapshot {
        .init(manifestFile: manifestFile, imageFile: imageFile, verifiedManifest: candidate?.manifest)
    }
    public func importFile(_ url: URL, kind: OTAPreparationFileKind) throws -> OTAPreparationSnapshot {
        candidate = nil
        try Task.checkCancellation()
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        let bytes = try Self.read(url, maximumBytes: kind.maximumBytes)
        try Task.checkCancellation()
        let selection = OTAPreparationSelection(name: url.lastPathComponent, byteCount: bytes.count)
        switch kind {
        case .manifest: envelope = bytes; manifestFile = selection
        case .image: image = bytes; imageFile = selection
        }
        return snapshot()
    }
    public func verify(trust: OTAPreparationTrust?) throws -> OTAPreparationSnapshot {
        candidate = nil
        guard let trust else { throw OTAPreparationError.trustUnavailable }
        guard let envelope, let image else { throw OTAPreparationError.missingFiles }
        try Task.checkCancellation()
        let verified = try Self.verify(envelope: envelope, image: image, trust: trust)
        try Task.checkCancellation()
        candidate = verified
        return snapshot()
    }
    public func verifiedCandidate() throws -> VerifiedOTACandidate {
        guard let candidate else { throw OTAPreparationError.notVerified }; return candidate
    }
    public func invalidateVerification() { candidate = nil }
    /// Atomic replacement in the phone sandbox, not proof of physical fsync durability.
    /// No trust material, paths, installed state or accepted floor is serialized.
    public func save() throws {
        guard let candidate else { throw OTAPreparationError.notVerified }
        try Task.checkCancellation()
        var archive = Self.header
        Self.append(UInt32(candidate.envelopeSnapshot.count), to: &archive)
        Self.append(UInt32(candidate.imageSnapshot.count), to: &archive)
        archive.append(candidate.envelopeSnapshot); archive.append(candidate.imageSnapshot)
        try FileManager.default.createDirectory(at: archiveURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Task.checkCancellation()
        try archive.write(to: archiveURL, options: .atomic)
        try Task.checkCancellation()
    }
    /// A missing archive returns false. An invalid or newly untrusted archive is
    /// left unchanged; no stored 'verified' bit can bypass current verification.
    public func restore(trust: OTAPreparationTrust?) throws -> Bool {
        candidate = nil
        guard FileManager.default.fileExists(atPath: archiveURL.path) else { return false }
        guard let trust else { throw OTAPreparationError.trustUnavailable }
        let archive = try Self.read(archiveURL, maximumBytes: 16 + OTAPackageVerifier.maximumEnvelopeBytes + OTAPackageVerifier.maximumImageBytes)
        guard archive.count >= 16, archive.prefix(8) == Self.header else { throw OTAPreparationError.invalidArchive }
        let envelopeBytes = Int(Self.readUInt32(archive, at: 8)); let imageBytes = Int(Self.readUInt32(archive, at: 12))
        guard (1...OTAPackageVerifier.maximumEnvelopeBytes).contains(envelopeBytes),
              (1...OTAPackageVerifier.maximumImageBytes).contains(imageBytes),
              archive.count == 16 + envelopeBytes + imageBytes else { throw OTAPreparationError.invalidArchive }
        let envelope = archive.subdata(in: 16..<(16 + envelopeBytes))
        let image = archive.subdata(in: (16 + envelopeBytes)..<archive.count)
        try Task.checkCancellation()
        let verified = try Self.verify(envelope: envelope, image: image, trust: trust)
        try Task.checkCancellation()
        self.envelope = envelope; self.image = image; candidate = verified
        manifestFile = .init(name: "Manifesto salvato", byteCount: envelopeBytes)
        imageFile = .init(name: "Immagine salvata", byteCount: imageBytes)
        return true
    }
    private static func verify(envelope: Data, image: Data, trust: OTAPreparationTrust) throws -> VerifiedOTACandidate {
        try OTAPackageVerifier.verify(envelope: envelope, image: image, trustedKeys: trust.trustedKeys,
            hardwareID: trust.hardwareID, inactiveCapacity: trust.inactiveCapacity, acceptedCounter: trust.acceptedCounter)
    }
    private static func read(_ url: URL, maximumBytes: Int) throws -> Data {
        guard url.isFileURL, (try? url.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true else {
            throw OTAPreparationError.invalidFile
        }
        let file = try FileHandle(forReadingFrom: url); defer { try? file.close() }
        var data = Data()
        while data.count <= maximumBytes {
            try Task.checkCancellation()
            let size = min(65_536, maximumBytes + 1 - data.count)
            guard let chunk = try file.read(upToCount: size), !chunk.isEmpty else { break }
            data.append(chunk)
        }
        guard !data.isEmpty, data.count <= maximumBytes else { throw OTAPreparationError.fileTooLarge }
        return data
    }
    private static func append(_ value: UInt32, to data: inout Data) {
        for shift in stride(from: 0, to: 32, by: 8) { data.append(UInt8(truncatingIfNeeded: value >> shift)) }
    }
    private static func readUInt32(_ data: Data, at offset: Int) -> UInt32 {
        (0..<4).reduce(0) { $0 | UInt32(data[offset + $1]) << ($1 * 8) }
    }
}
