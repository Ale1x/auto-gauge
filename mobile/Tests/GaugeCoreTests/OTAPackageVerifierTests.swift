#if os(macOS)
import Foundation
import XCTest
@testable import GaugeCore

final class OTAPackageVerifierTests: XCTestCase {
    private func bytes(_ text:String) throws -> Data { try XCTUnwrap(Data(base64Encoded:text)) }
    private func verify(_ vector:OTAVectorCase,bundle:OTAVectorBundle) throws -> VerifiedOTACandidate {
        let keyID = vector.key_id ?? bundle.key_id
        return try OTAPackageVerifier.verify(envelope:bytes(vector.envelope_base64),image:bytes(vector.image_base64 ?? bundle.image_base64),
            trustedKeys:[keyID:bytes(vector.key_der_base64 ?? bundle.public_key_der_base64)],hardwareID:vector.hardware_id ?? bundle.hardware_id,
            inactiveCapacity:vector.inactive_capacity ?? bundle.inactive_capacity,
            acceptedCounter:XCTUnwrap(UInt64(vector.accepted_counter ?? bundle.accepted_counter)))
    }
    func testCommonEphemeralOpenSSLVectors() throws {
        let bundle = try OTAVectorBundle.generated.get()
        XCTAssertEqual(bundle.schema_version,1); XCTAssertGreaterThanOrEqual(bundle.cases.count,46)
        XCTAssertGreaterThanOrEqual(bundle.cases.filter(\.expected_package).count,7)
        for vector in bundle.cases {
            if vector.expected_package {
                let candidate = try verify(vector,bundle:bundle)
                XCTAssertEqual(candidate.manifest.signingBytes,try bytes(vector.canonical_base64),vector.name)
                XCTAssertEqual(candidate.imageSnapshot,try bytes(vector.image_base64 ?? bundle.image_base64),vector.name)
                XCTAssertEqual(candidate.envelopeSnapshot,try bytes(vector.envelope_base64),vector.name)
            } else {
                XCTAssertThrowsError(try verify(vector,bundle:bundle),vector.name)
            }
        }
    }
    func testSecurityRejectsSaltMGFPaddingDomainAndNonexactRSA3072() throws {
        let bundle = try OTAVectorBundle.generated.get()
        for name in ["signature_salt20","signature_mgf_sha384","signature_pkcs1","signature_wrong_domain"] {
            let vector = try XCTUnwrap(bundle.cases.first { $0.name == name })
            XCTAssertThrowsError(try verify(vector,bundle:bundle)) { XCTAssertEqual($0 as? OTAPackageError,.signature,name) }
        }
        for name in ["rsa3070_key","key_spki_not_pkcs1","key_der_trailing","key_der_garbage"] {
            let vector = try XCTUnwrap(bundle.cases.first { $0.name == name })
            XCTAssertThrowsError(try verify(vector,bundle:bundle)) { XCTAssertEqual($0 as? OTAPackageError,.publicKey,name) }
        }
    }
    func testCandidateOwnsDeepSnapshotsAfterMutableSourceChanges() throws {
        let bundle = try OTAVectorBundle.generated.get()
        let vector = try XCTUnwrap(bundle.cases.first { $0.name == "valid" })
        let originalImage = try bytes(bundle.image_base64); let originalEnvelope = try bytes(vector.envelope_base64)
        let imageMemory = try XCTUnwrap(NSMutableData(data:originalImage))
        let envelopeMemory = try XCTUnwrap(NSMutableData(data:originalEnvelope))
        let sourceImage = Data(bytesNoCopy:imageMemory.mutableBytes,count:imageMemory.length,deallocator:.none)
        let sourceEnvelope = Data(bytesNoCopy:envelopeMemory.mutableBytes,count:envelopeMemory.length,deallocator:.none)
        let candidate = try OTAPackageVerifier.verify(envelope:sourceEnvelope,image:sourceImage,
            trustedKeys:[bundle.key_id:bytes(bundle.public_key_der_base64)],hardwareID:bundle.hardware_id,
            inactiveCapacity:bundle.inactive_capacity,acceptedCounter:XCTUnwrap(UInt64(bundle.accepted_counter)))
        imageMemory.mutableBytes.storeBytes(of:UInt8(0),as:UInt8.self)
        envelopeMemory.mutableBytes.storeBytes(of:UInt8(0),as:UInt8.self)
        XCTAssertEqual(candidate.imageSnapshot,originalImage); XCTAssertEqual(candidate.envelopeSnapshot,originalEnvelope)
        var callerCopy = candidate.imageSnapshot; callerCopy[0] ^= 1
        XCTAssertEqual(candidate.imageSnapshot,originalImage)
        XCTAssertEqual(candidate.manifest.releaseCounter,9_007_199_254_740_993)
    }
    func testReplayTrustCapacityProfileAndImageFailuresHaveDistinctReasons() throws {
        let bundle = try OTAVectorBundle.generated.get()
        let expected: [String:OTAPackageError] = ["counter_replay":.replay,"counter_higher_floor":.replay,"unknown_key":.untrustedKey,
            "hardware_mismatch":.hardware,"slot_too_small":.capacity,"image_mutated":.image,"image_trailing":.image,"image_truncated":.image,
            "esp_version32":.profile,"esp_image_unaligned":.profile]
        for (name,error) in expected {
            let vector = try XCTUnwrap(bundle.cases.first { $0.name == name })
            XCTAssertThrowsError(try verify(vector,bundle:bundle)) { XCTAssertEqual($0 as? OTAPackageError,error,name) }
        }
        let vector = try XCTUnwrap(bundle.cases.first { $0.name == "valid_release_max" })
        XCTAssertEqual(try verify(vector,bundle:bundle).manifest.releaseCounter,UInt64.max)
    }
}
#endif
