import Foundation

public enum HistoryChartDecimator {
    public static func sample(_ points: [HistoryChartPoint], maximum: Int) throws -> (points: [HistoryChartPoint], lines: Bool) {
        guard (4...240).contains(maximum), points.count <= 1024 else { throw HistoryTimelineError.capacity }
        guard points.count > maximum else { return (points, true) }
        var required = Set<Int>()
        for index in points.indices {
            if index == 0 || points[index - 1].segment != points[index].segment { required.insert(index) }
            if index == points.count - 1 || points[index + 1].segment != points[index].segment { required.insert(index) }
        }
        if let min = points.indices.min(by: { points[$0].value < points[$1].value }) { required.insert(min) }
        if let max = points.indices.max(by: { points[$0].value < points[$1].value }) { required.insert(max) }
        let lines = required.count <= maximum
        var selected: Set<Int> = lines ? required : [0, points.count - 1]
        let buckets = (maximum - selected.count) / 2
        if buckets > 0 {
            for bucket in 0..<buckets {
                let start = bucket * points.count / buckets
                let end = (bucket + 1) * points.count / buckets
                let range = start..<end
                if let min = range.min(by: { points[$0].value < points[$1].value }) { selected.insert(min) }
                if let max = range.max(by: { points[$0].value < points[$1].value }) { selected.insert(max) }
            }
        }
        return (selected.sorted().map { points[$0] }, lines)
    }
}
