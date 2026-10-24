enum GaugePreviewValue {
    static func value(for pid: Int) -> String {
        switch pid { case 12: "2.150"; case 13: "72"; case 5: "89"; case 4: "38"; case 15: "24"; default: "—" }
    }
}
