#if os(macOS)
import Foundation
import XCTest
@testable import GaugeCore

final class OTAPackagePreparationTests: XCTestCase {
    private func directory() throws -> URL {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("ag-preparation-" + UUID().uuidString)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        addTeardownBlock { try? FileManager.default.removeItem(at: url) }; return url
    }
    private func fixture(_ directory: URL) throws -> (URL, URL, OTAPreparationTrust) {
        let bundle = try OTAVectorBundle.generated.get(); let vector = try XCTUnwrap(bundle.cases.first { $0.expected_package })
        let manifest = directory.appendingPathComponent("manifest.json"); let image = directory.appendingPathComponent("image.bin")
        try XCTUnwrap(Data(base64Encoded: vector.envelope_base64)).write(to: manifest)
        try XCTUnwrap(Data(base64Encoded: bundle.image_base64)).write(to: image)
        return (manifest, image, .init(hardwareID: bundle.hardware_id, inactiveCapacity: bundle.inactive_capacity,
            acceptedCounter: try XCTUnwrap(UInt64(bundle.accepted_counter)), trustedKeys: [bundle.key_id: try XCTUnwrap(Data(base64Encoded: bundle.public_key_der_base64))]))
    }
    private func prepare(_ session: OTAPackagePreparation, manifest: URL, image: URL, trust: OTAPreparationTrust) async throws -> OTAPreparationSnapshot {
        _ = try await session.importFile(manifest, kind: .manifest); _ = try await session.importFile(image, kind: .image)
        return try await session.verify(trust: trust)
    }
    func testPrepareSaveReopenReverifiesExactSnapshot() async throws {
        let directory = try directory(); let (manifest, image, trust) = try fixture(directory)
        let archive = directory.appendingPathComponent("nested/prepared.agop"); let session = OTAPackagePreparation(archiveURL: archive)
        let snapshot = try await prepare(session, manifest: manifest, image: image, trust: trust)
        let candidate = try await session.verifiedCandidate()
        XCTAssertEqual(snapshot.verifiedManifest, candidate.manifest)
        try await session.save()
        let reopened = OTAPackagePreparation(archiveURL: archive)
        let restored = try await reopened.restore(trust: trust); XCTAssertTrue(restored)
        let candidateAgain = try await reopened.verifiedCandidate()
        XCTAssertEqual(candidateAgain.envelopeSnapshot, candidate.envelopeSnapshot)
        XCTAssertEqual(candidateAgain.imageSnapshot, candidate.imageSnapshot)
        let bytes = try Data(contentsOf: archive)
        XCTAssertEqual(bytes.count, 16 + candidate.envelopeSnapshot.count + candidate.imageSnapshot.count)
    }
    func testMissingTrustAndFilesNeverVerifyOrSave() async throws {
        let directory = try directory(); let session = OTAPackagePreparation(archiveURL: directory.appendingPathComponent("prepared.agop"))
        do { _ = try await session.verify(trust: nil); XCTFail("No trust") } catch { XCTAssertEqual(error as? OTAPreparationError, .trustUnavailable) }
        do { try await session.save(); XCTFail("No candidate") } catch { XCTAssertEqual(error as? OTAPreparationError, .notVerified) }
        let (_, _, trust) = try fixture(directory)
        do { _ = try await session.verify(trust: trust); XCTFail("No files") } catch { XCTAssertEqual(error as? OTAPreparationError, .missingFiles) }
        let missing = try await session.restore(trust: nil); XCTAssertFalse(missing)
    }
    func testFileSnapshotIsIndependentAndReplacementInvalidates() async throws {
        let directory = try directory(); let (manifest, image, trust) = try fixture(directory)
        let session = OTAPackagePreparation(archiveURL: directory.appendingPathComponent("prepared.agop"))
        _ = try await session.importFile(manifest, kind: .manifest); _ = try await session.importFile(image, kind: .image)
        let original = try Data(contentsOf: image)
        try Data([0]).write(to: image)
        _ = try await session.verify(trust: trust)
        let candidate = try await session.verifiedCandidate(); XCTAssertEqual(candidate.imageSnapshot, original)
        _ = try await session.importFile(image, kind: .image)
        do { _ = try await session.verifiedCandidate(); XCTFail("Replacement invalidates") } catch { XCTAssertEqual(error as? OTAPreparationError, .notVerified) }
        do { _ = try await session.verify(trust: trust); XCTFail("Changed image") } catch { XCTAssertEqual(error as? OTAPackageError, .image) }
    }
    func testFailedImportInvalidatesPreviousVerification() async throws {
        let directory = try directory(); let (manifest, image, trust) = try fixture(directory)
        let session = OTAPackagePreparation(archiveURL: directory.appendingPathComponent("prepared.agop"))
        let previous = try await prepare(session, manifest: manifest, image: image, trust: trust)
        do { _ = try await session.importFile(directory, kind: .manifest); XCTFail("Directory rejected") } catch { XCTAssertEqual(error as? OTAPreparationError, .invalidFile) }
        let current = await session.snapshot()
        XCTAssertEqual(current.manifestFile, previous.manifestFile); XCTAssertNil(current.verifiedManifest)
        do { try await session.save(); XCTFail("Must reverify") } catch { XCTAssertEqual(error as? OTAPreparationError, .notVerified) }
    }
    func testRestoreUsesCurrentFloorAndNeverRewritesRejectedArchive() async throws {
        let directory = try directory(); let (manifest, image, trust) = try fixture(directory)
        let archive = directory.appendingPathComponent("prepared.agop"); let session = OTAPackagePreparation(archiveURL: archive)
        let snapshot = try await prepare(session, manifest: manifest, image: image, trust: trust); try await session.save()
        let before = try Data(contentsOf: archive)
        let floor = try XCTUnwrap(snapshot.verifiedManifest?.releaseCounter)
        let updated = OTAPreparationTrust(hardwareID: trust.hardwareID, inactiveCapacity: trust.inactiveCapacity, acceptedCounter: floor, trustedKeys: trust.trustedKeys)
        let reopened = OTAPackagePreparation(archiveURL: archive)
        do { _ = try await reopened.restore(trust: updated); XCTFail("Current floor") } catch { XCTAssertEqual(error as? OTAPackageError, .replay) }
        do { _ = try await reopened.restore(trust: nil); XCTFail("Current trust required") } catch { XCTAssertEqual(error as? OTAPreparationError, .trustUnavailable) }
        XCTAssertEqual(try Data(contentsOf: archive), before)
        let current = await reopened.snapshot(); XCTAssertNil(current.verifiedManifest)
    }
    func testArchiveCorruptionTruncationAndTrailingAreRejected() async throws {
        let directory = try directory(); let (manifest, image, trust) = try fixture(directory)
        let archive = directory.appendingPathComponent("prepared.agop"); let session = OTAPackagePreparation(archiveURL: archive)
        _ = try await prepare(session, manifest: manifest, image: image, trust: trust); try await session.save()
        let good = try Data(contentsOf: archive)
        var wrongHeader = good; wrongHeader[7] = 1
        var wrongLength = good; wrongLength[8] = 0; wrongLength[9] = 0; wrongLength[10] = 0; wrongLength[11] = 0
        var alteredImage = good; alteredImage[alteredImage.count - 1] ^= 1
        for bytes in [Data(good.prefix(15)), Data(good.dropLast()), good + Data([0]), wrongHeader, wrongLength, alteredImage] {
            try bytes.write(to: archive)
            let reopened = OTAPackagePreparation(archiveURL: archive)
            do { _ = try await reopened.restore(trust: trust); XCTFail("Corrupt archive rejected") } catch {}
            let current = await reopened.snapshot(); XCTAssertNil(current.verifiedManifest)
            XCTAssertEqual(try Data(contentsOf: archive), bytes)
        }
    }
    func testBoundedRegularFileImport() async throws {
        let directory = try directory(); let file = directory.appendingPathComponent("input")
        let session = OTAPackagePreparation(archiveURL: directory.appendingPathComponent("prepared.agop"))
        for bytes in [Data(), Data(repeating: 0, count: 4097)] {
            try bytes.write(to: file)
            do { _ = try await session.importFile(file, kind: .manifest); XCTFail("Envelope bound") } catch { XCTAssertEqual(error as? OTAPreparationError, .fileTooLarge) }
        }
        try Data(repeating: 0, count: OTAPackageVerifier.maximumImageBytes + 1).write(to: file)
        do { _ = try await session.importFile(file, kind: .image); XCTFail("Image bound") } catch { XCTAssertEqual(error as? OTAPreparationError, .fileTooLarge) }
        do { _ = try await session.importFile(try XCTUnwrap(URL(string: "https://example.invalid/image")), kind: .image); XCTFail("No network") } catch { XCTAssertEqual(error as? OTAPreparationError, .invalidFile) }
    }
    func testSaveFailureDoesNotInventSavedStateOrDestroyParent() async throws {
        let directory = try directory(); let (manifest, image, trust) = try fixture(directory)
        let parent = directory.appendingPathComponent("parent-is-file"); let original = Data("unchanged".utf8); try original.write(to: parent)
        let session = OTAPackagePreparation(archiveURL: parent.appendingPathComponent("prepared.agop"))
        _ = try await prepare(session, manifest: manifest, image: image, trust: trust)
        do { try await session.save(); XCTFail("Parent is file") } catch {}
        XCTAssertEqual(try Data(contentsOf: parent), original)
        let current = await session.snapshot(); XCTAssertNotNil(current.verifiedManifest)
    }
    func testCancelledVerificationNeverPublishesCandidate() async throws {
        let directory = try directory(); let (manifest, image, trust) = try fixture(directory)
        let session = OTAPackagePreparation(archiveURL: directory.appendingPathComponent("prepared.agop"))
        _ = try await prepare(session, manifest: manifest, image: image, trust: trust)
        let cancelled = Task {
            withUnsafeCurrentTask { $0?.cancel() }
            return try await session.verify(trust: trust)
        }
        do { _ = try await cancelled.value; XCTFail("Cancelled work") } catch { XCTAssertTrue(error is CancellationError) }
        let snapshot = await session.snapshot(); XCTAssertNil(snapshot.verifiedManifest)
        do { try await session.save(); XCTFail("Cannot save cancelled verification") } catch { XCTAssertEqual(error as? OTAPreparationError, .notVerified) }
    }
    func testLocalTrustCopiesExternallyMutablePublicKey() async throws {
        let directory = try directory(); let (manifest, image, trust) = try fixture(directory)
        let keyID = try XCTUnwrap(trust.trustedKeys.keys.first)
        let mutable = NSMutableData(data: try XCTUnwrap(trust.trustedKeys[keyID]))
        let key = Data(bytesNoCopy: mutable.mutableBytes, count: mutable.length, deallocator: .none)
        let local = OTAPreparationTrust(hardwareID: trust.hardwareID, inactiveCapacity: trust.inactiveCapacity,
            acceptedCounter: trust.acceptedCounter, trustedKeys: [keyID: key])
        mutable.resetBytes(in: NSRange(location: 0, length: mutable.length))
        let session = OTAPackagePreparation(archiveURL: directory.appendingPathComponent("prepared.agop"))
        let snapshot = try await prepare(session, manifest: manifest, image: image, trust: local)
        XCTAssertNotNil(snapshot.verifiedManifest)
    }

}
#endif
