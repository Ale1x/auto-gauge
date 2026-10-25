/// Product-facing labels and an initial layout using only available measures.
public enum GaugeConfigurationDefaults {
    public static func namedCatalog(_ source: [PIDDescriptor]) -> [PIDDescriptor] {
        source.map { item in
            guard let definition = HistoryPIDDefinition.find(item.pid) else { return item }
            var result = item; result.nameIT = definition.nameIT; result.unit = definition.unit; return result
        }
    }
    public static func normalizedLabels(_ configuration: GaugeConfiguration, catalog: [PIDDescriptor]) -> GaugeConfiguration {
        var result = configuration
        for dashboard in result.dashboards.indices {
            for widget in result.dashboards[dashboard].widgets.indices {
                if let descriptor = catalog.first(where: { $0.pid == result.dashboards[dashboard].widgets[widget].pid.pid }) {
                    result.dashboards[dashboard].widgets[widget].pid = descriptor
                }
            }
        }
        return result
    }
    public static func make(catalog source: [PIDDescriptor]) throws -> GaugeConfiguration {
        let available = namedCatalog(source).filter { $0.supported && $0.service == 1 }
        guard !available.isEmpty else { throw ConfigurationError.unsupportedPID }
        let preferred = [12, 13, 5].compactMap { number in available.first(where: { $0.pid == number }) }
        let choices = preferred.isEmpty ? Array(available.prefix(3)) : preferred
        let frames: [WidgetFrame] = choices.count == 1
            ? [.init(x: 0.2, y: 0.3, width: 0.6, height: 0.35)]
            : [.init(x: 0.25, y: 0.2, width: 0.5, height: 0.25), .init(x: 0.15, y: 0.55, width: 0.3, height: 0.2), .init(x: 0.55, y: 0.55, width: 0.3, height: 0.2)]
        let widgets = choices.enumerated().map { index, pid in GaugeWidget(id: index + 1, pid: pid, frame: frames[index]) }
        let result = GaugeConfiguration(dashboards: [.init(id: 1, widgets: widgets)])
        try ConfigurationValidator.validate(result, catalog: available); return result
    }
}
