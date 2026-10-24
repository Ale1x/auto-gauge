import Foundation
import XCTest
@testable import GaugeCore

final class OTAManifestTests: XCTestCase {
    private var manifest: String {
        "{\"schema_version\":1,\"target\":\"esp32s3\",\"hardware_id\":\"GAUGE-A\",\"firmware_version\":\"1.2.3+dev\",\"release_counter\":\"18446744073709551615\",\"image_bytes\":16,\"image_sha256\":\"\(String(repeating:"0",count:64))\",\"signing_key_id\":\"ephemeral.test\"}"
    }
    func testExactCanonicalDomainAndSortedASCIIBytesPreserveUInt64() throws {
        let parsed = try OTAManifest(json:OTAStrictJSON.parse(Data(manifest.utf8)))
        XCTAssertEqual(parsed.releaseCounter,UInt64.max)
        let expected = "AUTO-GAUGE-OTA-MANIFEST\0V1\0{\"firmware_version\":\"1.2.3+dev\",\"hardware_id\":\"GAUGE-A\",\"image_bytes\":16,\"image_sha256\":\"\(String(repeating:"0",count:64))\",\"release_counter\":\"18446744073709551615\",\"schema_version\":1,\"signing_key_id\":\"ephemeral.test\",\"target\":\"esp32s3\"}"
        XCTAssertEqual(parsed.signingBytes,Data(expected.utf8))
        let escaped = manifest.replacingOccurrences(of:"GAUGE-A",with:"\\u0047AUGE-A")
        XCTAssertEqual(try OTAManifest(json:OTAStrictJSON.parse(Data(escaped.utf8))).signingBytes,parsed.signingBytes)
    }
    func testClosedSchemaRejectsDuplicateSemanticKeysNumbersAndTrailingInput() throws {
        let invalid = [
            manifest.replacingOccurrences(of:"\"schema_version\":1",with:"\"schema_version\":1,\"schema_version\":1"),
            manifest.replacingOccurrences(of:"\"schema_version\":1",with:"\"schema_version\":1,\"\\u0073chema_version\":1"),
            manifest.replacingOccurrences(of:"\"schema_version\":1",with:"\"schema_version\":true"),
            manifest.replacingOccurrences(of:"\"schema_version\":1",with:"\"schema_version\":1.0"),
            manifest.replacingOccurrences(of:"\"image_bytes\":16",with:"\"image_bytes\":1.6e1"),
            manifest.replacingOccurrences(of:"\"image_bytes\":16",with:"\"image_bytes\":016"),
            manifest.replacingOccurrences(of:"\"image_bytes\":16",with:"\"image_bytes\":null"),
            manifest.replacingOccurrences(of:"\"image_bytes\":16",with:"\"image_bytes\":[]"),
            manifest.replacingOccurrences(of:"\"image_bytes\":16",with:"\"image_bytes\":16,\"extra\":1"),
            manifest + "{}",manifest + "x",String(repeating:"{",count:4096)
        ]
        for raw in invalid {
            XCTAssertThrowsError(try OTAManifest(json:OTAStrictJSON.parse(Data(raw.utf8))),raw)
        }
        XCTAssertNoThrow(try OTAManifest(json:OTAStrictJSON.parse(Data((" \n"+manifest+"\t\r").utf8))))
    }
    func testASCIIIdentifiersCanonicalCounterAndDigestBounds() throws {
        for invalid in ["", "-bad", "a+b", "a/b", "é", String(repeating:"a",count:65)] {
            let raw = manifest.replacingOccurrences(of:"GAUGE-A",with:invalid)
            XCTAssertThrowsError(try OTAManifest(json:OTAStrictJSON.parse(Data(raw.utf8))))
        }
        for invalid in ["00","+1","-1"," 1","18446744073709551616", "١"] {
            let raw = manifest.replacingOccurrences(of:String(UInt64.max),with:invalid)
            XCTAssertThrowsError(try OTAManifest(json:OTAStrictJSON.parse(Data(raw.utf8))))
        }
        for size in [0,-1,16777217] {
            let raw = manifest.replacingOccurrences(of:"\"image_bytes\":16",with:"\"image_bytes\":\(size)")
            XCTAssertThrowsError(try OTAManifest(json:OTAStrictJSON.parse(Data(raw.utf8))))
        }
        XCTAssertThrowsError(try OTAManifest(json:OTAStrictJSON.parse(Data(manifest.replacingOccurrences(of:String(repeating:"0",count:64),with:String(repeating:"A",count:64)).utf8))))
    }
    func testReferenceSchemaAndESPProfileAreExplicitlyDifferentAtBounds() throws {
        let version31 = manifest.replacingOccurrences(of:"1.2.3+dev",with:String(repeating:"v",count:31))
        let atLimit = try OTAManifest(json:OTAStrictJSON.parse(Data(version31.utf8)))
        XCTAssertNoThrow(try atLimit.validateESPProfile())
        for count in [32,64] {
            let raw = manifest.replacingOccurrences(of:"1.2.3+dev",with:String(repeating:"v",count:count))
            let reference = try OTAManifest(json:OTAStrictJSON.parse(Data(raw.utf8)))
            XCTAssertThrowsError(try reference.validateESPProfile()) { XCTAssertEqual($0 as? OTAPackageError,.profile) }
        }
        let tooLong = manifest.replacingOccurrences(of:"1.2.3+dev",with:String(repeating:"v",count:65))
        XCTAssertThrowsError(try OTAManifest(json:OTAStrictJSON.parse(Data(tooLong.utf8))))
        let unaligned = try OTAManifest(json:OTAStrictJSON.parse(Data(manifest.replacingOccurrences(of:"\"image_bytes\":16",with:"\"image_bytes\":17").utf8)))
        XCTAssertThrowsError(try unaligned.validateESPProfile())
        let maximum = try OTAManifest(json:OTAStrictJSON.parse(Data(manifest.replacingOccurrences(of:"\"image_bytes\":16",with:"\"image_bytes\":16777216").utf8)))
        XCTAssertEqual(maximum.imageBytes,16*1024*1024); XCTAssertNoThrow(try maximum.validateESPProfile())
    }
    func testOuterBoundsRejectBeforeUntrustedImageOrKeysAreUsed() throws {
        for envelope in [Data(),Data(repeating:32,count:4097),Data([0xff])] {
            XCTAssertThrowsError(try OTAPackageVerifier.verify(envelope:envelope,image:Data(),trustedKeys:[:],hardwareID:"GAUGE-A",inactiveCapacity:16,acceptedCounter:0))
        }
    }
}
