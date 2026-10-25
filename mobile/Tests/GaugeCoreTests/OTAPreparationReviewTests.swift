#if os(macOS)
import Foundation
import XCTest
@testable import GaugeCore

final class OTAPreparationReviewTests: XCTestCase {
    func testMissingRestoreCannotRetainCandidateUnderMissingOrChangedTrust() async throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("ag-prep-review-" + UUID().uuidString)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let bundle = try OTAVectorBundle.generated.get()
        let vector = try XCTUnwrap(bundle.cases.first { $0.expected_package })
        let manifest = directory.appendingPathComponent("manifest.json")
        let image = directory.appendingPathComponent("image.bin")
        try XCTUnwrap(Data(base64Encoded: vector.envelope_base64)).write(to: manifest)
        try XCTUnwrap(Data(base64Encoded: bundle.image_base64)).write(to: image)
        let trust = OTAPreparationTrust(hardwareID: bundle.hardware_id, inactiveCapacity: bundle.inactive_capacity,
            acceptedCounter: try XCTUnwrap(UInt64(bundle.accepted_counter)),
            trustedKeys: [bundle.key_id: try XCTUnwrap(Data(base64Encoded: bundle.public_key_der_base64))])
        let session = OTAPackagePreparation(archiveURL: directory.appendingPathComponent("missing.agop"))
        _ = try await session.importFile(manifest, kind: .manifest); _ = try await session.importFile(image, kind: .image)
        _ = try await session.verify(trust: trust)
        let oldCandidate = try await session.verifiedCandidate()
        let restored = try await session.restore(trust: nil); XCTAssertFalse(restored)
        do { _ = try await session.verifiedCandidate(); XCTFail("Missing current trust must not retain a candidate") }
        catch { XCTAssertEqual(error as? OTAPreparationError, .notVerified) }
        _ = try await session.verify(trust: trust)
        let updated = OTAPreparationTrust(hardwareID: trust.hardwareID, inactiveCapacity: trust.inactiveCapacity,
            acceptedCounter: oldCandidate.manifest.releaseCounter, trustedKeys: trust.trustedKeys)
        let absent = try await session.restore(trust: updated); XCTAssertFalse(absent)
        do { _ = try await session.verifiedCandidate(); XCTFail("Missing archive must not retain verification under an earlier floor") }
        catch { XCTAssertEqual(error as? OTAPreparationError, .notVerified) }
    }
}
#endif
