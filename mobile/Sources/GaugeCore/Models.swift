import Foundation

public struct PIDDescriptor: Codable, Hashable, Identifiable, Sendable {
    public var service: Int
    public var pid: Int
    public var ecuSource: String
    public var nameIT: String
    public var unit: String
    public var supported: Bool
    public var id: String { "\(service):\(pid):\(ecuSource)" }

    public init(service: Int = 1, pid: Int, ecuSource: String = "unknown", nameIT: String, unit: String, supported: Bool = true) {
        self.service = service; self.pid = pid; self.ecuSource = ecuSource
        self.nameIT = nameIT; self.unit = unit; self.supported = supported
    }
}

public struct WidgetFrame: Codable, Equatable, Sendable {
    public var x: Double
    public var y: Double
    public var width: Double
    public var height: Double
    public init(x: Double, y: Double, width: Double, height: Double) {
        self.x = x; self.y = y; self.width = width; self.height = height
    }
    public var isValid: Bool {
        guard [x, y, width, height].allSatisfy(\.isFinite), x >= 0, y >= 0, width > 0, height > 0, x + width <= 1, y + height <= 1 else { return false }
        let q = [x, y, width, height].map { Int(($0 * 10000).rounded()) }
        return q[2] > 0 && q[3] > 0 && q[0] + q[2] <= 10000 && q[1] + q[3] <= 10000
    }
}

public struct GaugeWidget: Codable, Equatable, Identifiable, Sendable {
    public var id: Int
    public var pid: PIDDescriptor
    public var frame: WidgetFrame
    public init(id: Int = 1, pid: PIDDescriptor, frame: WidgetFrame) {
        self.id = id; self.pid = pid; self.frame = frame
    }
}

public struct Dashboard: Codable, Equatable, Identifiable, Sendable {
    public var id: Int
    public var name: String { "Dashboard \(id)" }
    public var widgets: [GaugeWidget]
    public init(id: Int = 1, widgets: [GaugeWidget]) {
        self.id = id; self.widgets = widgets
    }
    enum CodingKeys: String, CodingKey { case id, widgets }
}

public enum BrightnessMode: String, Codable, CaseIterable, Sendable {
    case manual
    case automatic = "auto"
    public var nameIT: String { self == .manual ? "Manuale" : "Automatica" }
}

public struct BrightnessSettings: Codable, Equatable, Sendable {
    public var mode: BrightnessMode
    public var manualPercent: Int
    enum CodingKeys: String, CodingKey { case mode; case manualPercent = "manual_percent" }
    public init(mode: BrightnessMode = .automatic, manualPercent: Int = 65) {
        self.mode = mode; self.manualPercent = manualPercent
    }
}

public struct GaugeConfiguration: Codable, Equatable, Sendable {
    public var schemaVersion: Int
    public var revision: Int
    public var dashboards: [Dashboard]
    public var brightness: BrightnessSettings
    enum CodingKeys: String, CodingKey { case schemaVersion = "schema_version"; case revision, dashboards, brightness }
    public init(schemaVersion: Int = 1, revision: Int = 1, dashboards: [Dashboard], brightness: BrightnessSettings = .init()) {
        self.schemaVersion = schemaVersion; self.revision = revision; self.dashboards = dashboards; self.brightness = brightness
    }
}

public enum ConfigurationError: Error, Equatable, LocalizedError {
    case version, dashboardCount, widgetCount, duplicateID, invalidFrame, unsupportedPID, brightness, revision, identifier
    public var errorDescription: String? {
        switch self {
        case .version: return "Versione della configurazione non supportata."
        case .dashboardCount: return "Sono consentite da 1 a 3 dashboard."
        case .widgetCount: return "Ogni dashboard deve contenere da 1 a 5 misure."
        case .duplicateID: return "Gli identificatori della configurazione devono essere univoci."
        case .invalidFrame: return "Una misura supera i limiti dell’area di anteprima."
        case .unsupportedPID: return "La configurazione contiene un PID non disponibile nel catalogo."
        case .brightness: return "La luminosità deve essere compresa fra 0 e 100."
        case .revision: return "La revisione della configurazione non è valida."
        case .identifier: return "Gli identificatori devono essere compresi fra 1 e 255."
        }
    }
}

public enum ConfigurationValidator {
    public static func validate(_ config: GaugeConfiguration, catalog: [PIDDescriptor]) throws {
        guard config.schemaVersion == 1 else { throw ConfigurationError.version }
        guard (1...2_147_483_647).contains(config.revision) else { throw ConfigurationError.revision }
        guard (1...3).contains(config.dashboards.count) else { throw ConfigurationError.dashboardCount }
        guard (0...100).contains(config.brightness.manualPercent) else { throw ConfigurationError.brightness }
        let supported = Set(catalog.filter { $0.supported && $0.service == 1 }.map(\.pid))
        var ids = Set<Int>()
        for dashboard in config.dashboards {
            guard (1...255).contains(dashboard.id) else { throw ConfigurationError.identifier }
            guard ids.insert(dashboard.id).inserted else { throw ConfigurationError.duplicateID }
            guard (1...5).contains(dashboard.widgets.count) else { throw ConfigurationError.widgetCount }
            var widgetIDs = Set<Int>()
            for widget in dashboard.widgets {
                guard (1...255).contains(widget.id), (1...255).contains(widget.pid.pid) else { throw ConfigurationError.identifier }
                guard widgetIDs.insert(widget.id).inserted else { throw ConfigurationError.duplicateID }
                guard widget.frame.isValid else { throw ConfigurationError.invalidFrame }
                guard widget.pid.service == 1, supported.contains(widget.pid.pid) else { throw ConfigurationError.unsupportedPID }
            }
        }
    }
}

public enum DemoData {
    public static let catalog: [PIDDescriptor] = [
        .init(pid: 12, ecuSource: "DEMO-7E8", nameIT: "Regime motore", unit: "giri/min"),
        .init(pid: 13, ecuSource: "DEMO-7E8", nameIT: "Velocità", unit: "km/h"),
        .init(pid: 5, ecuSource: "DEMO-7E8", nameIT: "Temperatura refrigerante", unit: "°C"),
        .init(pid: 4, ecuSource: "DEMO-7E8", nameIT: "Carico motore", unit: "%"),
        .init(pid: 15, ecuSource: "DEMO-7E8", nameIT: "Temperatura aspirazione", unit: "°C")
    ]
    public static let configuration = GaugeConfiguration(dashboards: [
        Dashboard(id: 1, widgets: [
            GaugeWidget(id: 1, pid: catalog[0], frame: .init(x: 0.25, y: 0.20, width: 0.50, height: 0.25)),
            GaugeWidget(id: 2, pid: catalog[1], frame: .init(x: 0.15, y: 0.55, width: 0.30, height: 0.20)),
            GaugeWidget(id: 3, pid: catalog[2], frame: .init(x: 0.55, y: 0.55, width: 0.30, height: 0.20))
        ])
    ])
}

extension WidgetFrame {
    enum CodingKeys: String, CodingKey { case x, y, width, height }
    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        x = Double(try c.decode(Int.self, forKey: .x)) / 10000
        y = Double(try c.decode(Int.self, forKey: .y)) / 10000
        width = Double(try c.decode(Int.self, forKey: .width)) / 10000
        height = Double(try c.decode(Int.self, forKey: .height)) / 10000
    }
    public func encode(to encoder: Encoder) throws {
        guard isValid else { throw ConfigurationError.invalidFrame }
        let quantized = [x, y, width, height].map { Int(($0 * 10000).rounded()) }
        guard quantized[2] > 0, quantized[3] > 0,
              quantized[0] + quantized[2] <= 10000,
              quantized[1] + quantized[3] <= 10000 else { throw ConfigurationError.invalidFrame }
        var c = encoder.container(keyedBy: CodingKeys.self)
        try c.encode(quantized[0], forKey: .x)
        try c.encode(quantized[1], forKey: .y)
        try c.encode(quantized[2], forKey: .width)
        try c.encode(quantized[3], forKey: .height)
    }
}

extension GaugeWidget {
    enum CodingKeys: String, CodingKey { case id, pid; case frame = "rect" }
    public init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        id = try c.decode(Int.self, forKey: .id)
        let number = try c.decode(Int.self, forKey: .pid)
        pid = DemoData.catalog.first { $0.pid == number } ?? PIDDescriptor(pid: number, nameIT: "PID \(number)", unit: "", supported: false)
        frame = try c.decode(WidgetFrame.self, forKey: .frame)
    }
    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: CodingKeys.self)
        try c.encode(id, forKey: .id)
        try c.encode(pid.pid, forKey: .pid)
        try c.encode(frame, forKey: .frame)
    }
}
