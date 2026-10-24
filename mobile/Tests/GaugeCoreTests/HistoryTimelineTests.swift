import Foundation
import XCTest
@testable import GaugeCore

final class HistoryTimelineTests: XCTestCase {
    private let sid = try? LogSessionID(String(repeating: "a", count: 32))
    private func record(_ sequence: UInt64, ms: UInt64 = 0, pid: Int = 12, ecu: UInt32 = 0x7e8, status: LogRecordStatus = .valid, raw: String = "1a10") throws -> HistoricalLogRecord {
        try .init(sequence: sequence, monotonicMS: ms, pid: pid, ecuSource: ecu, status: status, rawHex: raw)
    }
    private func session(_ records: [HistoricalLogRecord], losses: [LogLossInterval] = [], source: HistoryTimelineSource = .simulatedGauge) throws -> HistoryTimelineSession {
        try .init(deviceID: "TEST-DEVICE", sessionID: XCTUnwrap(sid), source: source, records: records, losses: losses)
    }
    func testSevenQualifiedFormulaBoundariesAndExactPayloadLengths() throws {
        let vectors: [(Int, String, Double)] = [(4,"00",0),(4,"ff",100),(5,"00",-40),(5,"ff",215),(12,"0000",0),(12,"ffff",16383.75),(12,"1a10",1668),(13,"ff",255),(15,"00",-40),(15,"ff",215),(16,"ffff",655.35),(17,"00",0),(17,"ff",100)]
        XCTAssertEqual(HistoryPIDDefinition.qualified.count, 7)
        for (pid, raw, expected) in vectors {
            let decoder = try XCTUnwrap(HistoryPIDDefinition.find(pid))
            XCTAssertEqual(try XCTUnwrap(decoder.value(rawHex: raw)), expected, accuracy: 0.000001)
            XCTAssertNil(decoder.value(rawHex: raw + "00")); XCTAssertNil(decoder.value(rawHex: ""))
        }
        XCTAssertNil(HistoryPIDDefinition.find(0x42)) // Wider firmware catalog does not imply this app has qualified it.
    }
    func testNonvalidStatusesNeverBecomeZeroAndRawRecordsStayUnmodified() throws {
        var rows = [try record(1, raw: "0000")]
        for (index, status) in [LogRecordStatus.unsupported,.unavailable,.stale,.timeout,.error].enumerated() {
            rows.append(try record(UInt64(index + 2), ms: UInt64(index + 1), status: status, raw: ""))
        }
        let input = try session(rows); let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
        let before = try encoder.encode(input.records)
        let result = try HistoryTimelineProjector.project(input, pid: 12, ecu: 0x7e8)
        XCTAssertEqual(result.numericCount, 1); XCTAssertEqual(result.chart.first?.value, 0)
        XCTAssertTrue(result.rows.dropFirst().allSatisfy { $0.value == nil })
        XCTAssertEqual(result.rows.count, 6); XCTAssertEqual(result.qualityCounts.count, 6)
        XCTAssertEqual(try encoder.encode(input.records), before)
    }
    func testUnknownFormulaAndWrongRawSizeAreExplicitNotDecoded() throws {
        let unknown = try session([record(1, pid: 0x42, raw: "03e8")])
        let a = try HistoryTimelineProjector.project(unknown, pid: 0x42, ecu: 0x7e8)
        XCTAssertEqual(a.rows.first?.quality, .formulaUnavailable); XCTAssertTrue(a.chart.isEmpty)
        let malformed = try session([record(1, raw: "1a"),record(2, ms: 1, raw: "410c1a10")])
        let b = try HistoryTimelineProjector.project(malformed, pid: 12, ecu: 0x7e8)
        XCTAssertTrue(b.rows.allSatisfy { $0.quality == .wrongPayloadLength }); XCTAssertTrue(b.chart.isEmpty)
    }
    func testPIDAndECUFiltersDoNotMixUnitsOrInferLossFromOtherMeasurements() throws {
        let input = try session([record(1,ms:100),record(2,ms:101,pid:5,raw:"50"),record(3,ms:102,ecu:0x7e9),record(4,ms:103)])
        let selected = try HistoryTimelineProjector.project(input, pid: 12, ecu: 0x7e8)
        XCTAssertEqual(selected.rows.map(\.record.sequence.value), [1,4]); XCTAssertTrue(selected.copyGaps.isEmpty)
        XCTAssertEqual(Set(selected.chart.map(\.segment)).count, 1)
        let temperature = try HistoryTimelineProjector.project(input, pid: 5, ecu: 0x7e8)
        XCTAssertEqual(temperature.rows.count, 1); XCTAssertEqual(temperature.chart.first?.value, 40)
    }
    func testQualityAndRealSequenceGapsSplitLinesWithoutInventingGapTimes() throws {
        let input = try session([record(1,ms:0),record(2,ms:1,status:.timeout,raw:""),record(3,ms:2),record(6,ms:5)],losses:[.init(first:4,last:5)])
        let result = try HistoryTimelineProjector.project(input, pid: 12, ecu: 0x7e8)
        XCTAssertEqual(result.chart.count, 3); XCTAssertEqual(Set(result.chart.map(\.segment)).count, 3)
        XCTAssertEqual(result.copyGaps, [try .init(first: 4, last: 5)])
        XCTAssertEqual(result.lossObservations.first?.unavailableCount, 2)
    }
    func testCloudLossObservationCoexistsWithRecoveredRecordsAndOtherSessions() throws {
        let sessionID = try XCTUnwrap(sid); let other = try LogSessionID(String(repeating:"b",count:32))
        let items = try [CloudHistoryItem(deviceID:"A",sessionID:sessionID,loss:.init(first:1,last:2)),
                         .init(deviceID:"A",sessionID:sessionID,record:record(1)),.init(deviceID:"A",sessionID:sessionID,record:record(2,ms:1)),
                         .init(deviceID:"A",sessionID:other,record:record(1)),.init(deviceID:"B",sessionID:sessionID,record:record(1))]
        let sessions = try HistoryTimelineSession.cloud(items); XCTAssertEqual(sessions.count, 3)
        let selected = try XCTUnwrap(sessions.first { $0.deviceID == "A" && $0.sessionID == sessionID })
        let result = try HistoryTimelineProjector.project(selected, pid:12, ecu:0x7e8)
        XCTAssertEqual(result.numericCount,2); XCTAssertEqual(result.lossObservations.first?.availableRecords,2)
        XCTAssertEqual(result.lossObservations.first?.unavailableCount,0); XCTAssertTrue(result.copyGaps.isEmpty)
        XCTAssertEqual(Set(result.chart.map(\.segment)).count,1)
    }
    func testLossOnlySessionRemainsVisibleWithoutFakePIDOrDate() throws {
        let input = try session([], losses:[.init(first:1,last:UInt64.max)],source:.restoredCloud)
        let result = try HistoryTimelineProjector.project(input,pid:nil,ecu:nil)
        XCTAssertNil(result.originMS); XCTAssertTrue(result.chart.isEmpty); XCTAssertEqual(result.lossObservations.first?.unavailableCount,UInt64.max)
    }
    func testUInt64SubtractsOriginBeforeDoubleAndRetainsExactCounters() throws {
        let base = UInt64.max - 2
        let result = try HistoryTimelineProjector.project(session([record(1,ms:base),record(2,ms:base+1),record(3,ms:UInt64.max)]),pid:12,ecu:0x7e8)
        XCTAssertEqual(result.chart.map(\.offsetMS),[0,1,2]); XCTAssertEqual(result.chart.map(\.seconds),[0,0.001,0.002])
        XCTAssertEqual(result.rows.last?.record.monotonicMS.decimal,String(UInt64.max)); XCTAssertFalse(result.timePrecisionLimited)
        let large = try HistoryTimelineProjector.project(session([record(1),record(2,ms:UInt64.max)]),pid:12,ecu:0x7e8)
        XCTAssertTrue(large.timePrecisionLimited)
    }
    func testClockRegressionsAreNotSortedAwayAndBreakFollowingSegment() throws {
        let result = try HistoryTimelineProjector.project(session([record(1,ms:100),record(2,ms:50),record(3,ms:120)]),pid:12,ecu:0x7e8)
        XCTAssertEqual(result.clockRegressions,1); XCTAssertEqual(result.rows[1].quality,.clockRegression)
        XCTAssertNil(result.rows[1].value); XCTAssertEqual(Set(result.chart.map(\.segment)).count,2)
    }
    func testDecimationKeepsGlobalExtremesEndpointsAndOnlyOriginalRecords() throws {
        var records: [HistoricalLogRecord] = []
        for n in 1...1024 {
            let raw: String = n == 500 ? "ffff" : (n == 501 ? "0000" : "1a10")
            records.append(try record(UInt64(n), ms: UInt64(n), raw: raw))
        }
        let result = try HistoryTimelineProjector.project(session(records),pid:12,ecu:0x7e8,maximumChartPoints:40)
        XCTAssertLessThanOrEqual(result.chart.count,40); XCTAssertTrue(result.drawLines); XCTAssertTrue(result.decimated)
        XCTAssertEqual(result.rows.count,1024)
        XCTAssertTrue([1,500,501,1024].allSatisfy { expected in result.chart.contains { $0.sequence.value == UInt64(expected) } })
        for point in result.chart {
            let original = records.first { $0.sequence == point.sequence }
            XCTAssertEqual(original?.monotonicMS, point.monotonicMS)
        }
    }
    func testTooManyDiscontinuitiesUsesPointsOnlyAndNeverBridgesGaps() throws {
        var rows: [HistoricalLogRecord] = []
        for n in 1...100 {
            let invalid = n % 2 == 0
            rows.append(try record(UInt64(n), ms: UInt64(n), status: invalid ? .timeout : .valid, raw: invalid ? "" : "1a10"))
        }
        let result = try HistoryTimelineProjector.project(session(rows),pid:12,ecu:0x7e8,maximumChartPoints:10)
        XCTAssertFalse(result.drawLines); XCTAssertLessThanOrEqual(result.chart.count,10)
        XCTAssertEqual(result.numericCount,50)
    }
    func testBoundsAndDuplicateIdentityRejectWithoutChangingInput() throws {
        let one = try record(1)
        XCTAssertThrowsError(try session([one,one]))
        let many = try (1...1025).map { try record(UInt64($0)) }
        XCTAssertThrowsError(try session(many))
        XCTAssertThrowsError(try HistoryTimelineProjector.project(session([one]),pid:12,ecu:0x7e8,maximumChartPoints:3))
    }
}
