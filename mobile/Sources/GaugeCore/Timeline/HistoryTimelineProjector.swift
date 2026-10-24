import Foundation

public enum HistoryTimelineProjector {
    public static func project(_ session: HistoryTimelineSession, pid: Int?, ecu: UInt32?, maximumChartPoints: Int = 240) throws -> HistoryTimelineProjection {
        var rows: [HistoryTimelineRow] = []; var points: [HistoryChartPoint] = []
        var gaps: [LogLossInterval] = []; var regressionCount = 0
        var previousSequence: UInt64?; var latestTime: UInt64?; var segment = 0
        let origin = session.records.first?.monotonicMS
        let definition = pid.flatMap(HistoryPIDDefinition.find)
        for record in session.records {
            try Task.checkCancellation()
            if let previousSequence, record.sequence.value > previousSequence, record.sequence.value - previousSequence > 1 {
                gaps.append(try .init(first: previousSequence + 1, last: record.sequence.value - 1)); segment += 1
            }
            previousSequence = record.sequence.value
            let regressed = latestTime.map { record.monotonicMS.value < $0 } ?? false
            if regressed { regressionCount += 1; segment += 1 }
            else { latestTime = record.monotonicMS.value }
            guard record.pid == pid, record.ecuSource == ecu else { continue }
            let value: Double?; let quality: HistorySampleQuality
            if record.status != .valid { value = nil; quality = .sourceStatus(record.status) }
            else if regressed { value = nil; quality = .clockRegression }
            else if let definition {
                value = definition.value(rawHex: record.rawHex)
                quality = value == nil ? .wrongPayloadLength : .decoded
            } else { value = nil; quality = .formulaUnavailable }
            rows.append(.init(record: record, value: value, quality: quality))
            if let value, let origin, record.monotonicMS >= origin {
                points.append(.init(sequence: record.sequence, monotonicMS: record.monotonicMS, offsetMS: record.monotonicMS.value - origin.value, value: value, segment: segment))
            } else { segment += 1 }
        }
        let reduced = try HistoryChartDecimator.sample(points, maximum: maximumChartPoints)
        let observations = try session.losses.map { loss in
            try Task.checkCancellation()
            return HistoryLossObservation(interval: loss, availableRecords: session.records.filter { $0.sequence >= loss.first && $0.sequence <= loss.last }.count)
        }
        let statuses: [LogRecordStatus] = [.valid, .unsupported, .unavailable, .stale, .timeout, .error]
        let counts = statuses.compactMap { status -> HistoryQualityCount? in
            let count = rows.filter { $0.record.status == status }.count
            return count > 0 ? .init(status: status, count: count) : nil
        }
        return .init(rows: rows, chart: reduced.points, numericCount: points.count, drawLines: reduced.lines, originMS: origin,
                     timePrecisionLimited: points.contains { $0.offsetMS > 9_007_199_254_740_991 }, qualityCounts: counts,
                     lossObservations: observations, copyGaps: gaps, clockRegressions: regressionCount)
    }
}
