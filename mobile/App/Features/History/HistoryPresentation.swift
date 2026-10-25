import Foundation
import GaugeCore

/// Presentation only: identifiers and transport metadata remain in the archive.
enum HistoryPresentation {
    static func duration(_ records: [HistoricalLogRecord]) -> String {
        guard let first = records.map(\.monotonicMS.value).min(),
              let last = records.map(\.monotonicMS.value).max() else { return "Durata non disponibile" }
        let seconds = (last - first) / 1_000
        if seconds < 60 { return "\(seconds) s" }
        if seconds < 3_600 { return seconds % 60 == 0 ? "\(seconds / 60) min" : "\(seconds / 60) min \(seconds % 60) s" }
        return "\(seconds / 3_600) h \((seconds % 3_600) / 60) min"
    }

    static func elapsed(_ seconds: Double) -> String {
        guard seconds.isFinite, seconds >= 0, seconds < Double(UInt64.max) else { return "—" }
        let whole = UInt64(seconds)
        if whole >= 3_600 { return "\(whole / 3_600):\(padded((whole % 3_600) / 60)):\(padded(whole % 60))" }
        return "\(whole / 60):\(padded(whole % 60))"
    }

    static func elapsed(_ timestamp: LogUInt64, from origin: LogUInt64?) -> String {
        guard let origin, timestamp >= origin else { return "Tempo non disponibile" }
        return elapsed(Double((timestamp.value - origin.value) / 1_000))
    }

    static func measure(_ pid: Int) -> String {
        HistoryPIDDefinition.find(pid)?.nameIT ?? "Altra misura"
    }

    static func value(_ record: HistoricalLogRecord) -> Double? {
        guard record.status == .valid else { return nil }
        return HistoryPIDDefinition.find(record.pid)?.value(rawHex: record.rawHex)
    }

    private static func padded(_ value: UInt64) -> String { value < 10 ? "0\(value)" : String(value) }
}
