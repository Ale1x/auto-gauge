import Foundation
import XCTest
@testable import GaugeCore

final class HistoryScalarCatalogTests: XCTestCase {
    func testQualifiedIDsAreExactlyScalarAllowlistWithStableMetadata() throws {
        let allowed = [0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x1f,0x21,0x22,0x23,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x33,0x3c,0x3d,0x3e,0x3f,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x52,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x61,0x62,0x63,0xa6]
        XCTAssertEqual(HistoryPIDDefinition.qualified.map(\.id), allowed)
        XCTAssertEqual(Set(HistoryPIDDefinition.qualified.map(\.nameIT)).count, 53)
        for pid in 0...255 {
            XCTAssertEqual(HistoryPIDDefinition.find(pid) != nil, allowed.contains(pid))
        }
        for definition in HistoryPIDDefinition.qualified {
            XCTAssertFalse(definition.nameIT.isEmpty); XCTAssertFalse(definition.unit.isEmpty)
            XCTAssertTrue([1,2,4].contains(definition.byteCount))
        }
        XCTAssertEqual(HistoryPIDDefinition.find(0x44)?.unit, "rapporto")
        XCTAssertEqual(HistoryPIDDefinition.find(0x63)?.unit, "N·m")
    }
    func testEveryScalarMatchesIndependentCOracleForMinimumMiddleMaximumAndPattern() throws {
        let root = URL(fileURLWithPath:#filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let data = try Data(contentsOf:root.appendingPathComponent("shared/fixtures/diagnostic-scalars.cases.json"))
        let fixture = try XCTUnwrap(JSONSerialization.jsonObject(with:data) as? [String:Any])
        XCTAssertEqual(fixture["schema_version"] as? Int,1); XCTAssertEqual(fixture["pid_count"] as? Int,53)
        let cases = try XCTUnwrap(fixture["cases"] as? [[String:Any]])
        XCTAssertEqual(cases.count,212)
        let units = [1:"%",2:"°C",3:"giri/min",4:"km/h",5:"g/s",6:"kPa",7:"V",8:"s",9:"km",10:"rapporto",11:"conteggio",12:"°",13:"min",14:"L/h",15:"N·m"]
        var counts: [Int:Int] = [:]
        for vector in cases {
            let pid = try XCTUnwrap(vector["pid"] as? Int)
            let raw = try XCTUnwrap(vector["raw"] as? String)
            let expected = try XCTUnwrap(vector["value"] as? Double)
            let unit = try XCTUnwrap(vector["unit_code"] as? Int)
            let definition = try XCTUnwrap(HistoryPIDDefinition.find(pid))
            XCTAssertEqual(definition.unit,units[unit],"PID \(pid)")
            XCTAssertEqual(definition.byteCount * 2,raw.utf8.count)
            XCTAssertEqual(try XCTUnwrap(definition.value(rawHex:raw)),expected,accuracy:max(1e-10,abs(expected)*1e-12),"PID \(pid), raw \(raw)")
            counts[pid,default:0] += 1
        }
        XCTAssertEqual(Set(counts.keys),Set(HistoryPIDDefinition.qualified.map(\.id)))
        XCTAssertTrue(counts.values.allSatisfy { $0 == 4 })
        // Explicit edge cases guard against truncation, clamping, signed U32 and rounded ratios.
        XCTAssertEqual(HistoryPIDDefinition.find(0x44)?.value(rawHex:"8000"),1)
        XCTAssertEqual(HistoryPIDDefinition.find(0x43)?.value(rawHex:"ffff"),25700)
        XCTAssertEqual(HistoryPIDDefinition.find(0xa6)?.value(rawHex:"ffffffff"),429496729.5)
        XCTAssertEqual(HistoryPIDDefinition.find(0x5d)?.value(rawHex:"0001"),-209.9921875)
    }
    func testEveryScalarRejectsWrongSizeAndNoncanonicalRawWithoutPrefixDecode() throws {
        for definition in HistoryPIDDefinition.qualified {
            let exact = String(repeating:"ab", count:definition.byteCount)
            XCTAssertNotNil(definition.value(rawHex:exact))
            for bytes in 0...8 where bytes != definition.byteCount {
                XCTAssertNil(definition.value(rawHex:String(repeating:"00",count:bytes)), "PID \(definition.id), bytes \(bytes)")
            }
            for invalid in [exact.uppercased(), "0x"+exact, " "+exact, exact+" ", String(repeating:"g", count:exact.count)] {
                XCTAssertNil(definition.value(rawHex:invalid))
            }
        }
    }
    func testAllScalarStatusesPreserveRawAndOnlyValidValuesArePlotted() throws {
        let sid = try LogSessionID(String(repeating:"c", count:32))
        let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
        for definition in HistoryPIDDefinition.qualified {
            let raw = String(repeating:"ff",count:definition.byteCount)
            let statuses: [LogRecordStatus] = [.valid,.unsupported,.unavailable,.stale,.timeout,.error]
            let records = try statuses.enumerated().map { index, status in
                try HistoricalLogRecord(sequence:UInt64(index+1),monotonicMS:UInt64(index),pid:definition.id,ecuSource:0x7e8,status:status,rawHex:status == .valid ? raw : "")
            }
            let original = try encoder.encode(records)
            let session = try HistoryTimelineSession(deviceID:"TEST-53",sessionID:sid,source:.simulatedGauge,records:records,losses:[])
            let projection = try HistoryTimelineProjector.project(session,pid:definition.id,ecu:0x7e8)
            XCTAssertEqual(projection.numericCount,1); XCTAssertEqual(projection.rows.count,6)
            XCTAssertEqual(projection.rows.first?.value,definition.value(rawHex:raw))
            XCTAssertTrue(projection.rows.dropFirst().allSatisfy { $0.value == nil })
            XCTAssertEqual(try encoder.encode(session.records),original)
        }
    }
}
