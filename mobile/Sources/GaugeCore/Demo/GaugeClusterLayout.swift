/// Layout derived from the 466 × 466 Waveshare ClusterScene.
/// Measures and identities remain those of the user's configuration.
public enum GaugeClusterLayout {
    public static func frames(count: Int) -> [WidgetFrame] {
        func frame(_ x: Double, _ y: Double, _ width: Double, _ height: Double) -> WidgetFrame {
            .init(x: x / 466, y: y / 466, width: width / 466, height: height / 466)
        }
        guard (1...5).contains(count) else { return [] }
        if count <= 3 {
            let main = frame(68, 132, 330, 130)
            if count == 1 { return [main] }
            if count == 2 { return [main, frame(153, 307, 160, 70)] }
            return [main, frame(67, 307, 160, 70), frame(239, 307, 160, 70)]
        }
        let detail = [frame(67, 139, 160, 70), frame(239, 139, 160, 70),
                      frame(83, 245, 300, 70), frame(88, 351, 140, 60), frame(238, 351, 140, 60)]
        return Array(detail.prefix(max(0, min(5, count))))
    }
    public static func apply(to configuration: inout GaugeConfiguration, dashboardID: Int) {
        guard let index = configuration.dashboards.firstIndex(where: { $0.id == dashboardID }) else { return }
        let positions = frames(count: configuration.dashboards[index].widgets.count)
        for widget in positions.indices { configuration.dashboards[index].widgets[widget].frame = positions[widget] }
    }
    public static func upgradingDefaultDemo(_ configuration: GaugeConfiguration) -> GaugeConfiguration {
        var updated = configuration
        let original = DemoData.configuration.dashboards[0].widgets.map(\.frame)
        for dashboard in configuration.dashboards where dashboard.widgets.map(\.frame) == original {
            apply(to: &updated, dashboardID: dashboard.id)
        }
        return updated
    }
}
