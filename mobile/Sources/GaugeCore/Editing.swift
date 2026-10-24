import Foundation

public enum EditorError: Error, LocalizedError {
    case missingDashboard, missingWidget, lastDashboard, lastWidget, dashboardLimit, widgetLimit, noIdentifier, revisionLimit
    public var errorDescription: String? {
        switch self {
        case .missingDashboard: return "La dashboard selezionata non è più disponibile."
        case .missingWidget: return "La misura selezionata non è più disponibile."
        case .lastDashboard: return "Mantieni almeno una dashboard."
        case .lastWidget: return "Mantieni almeno una misura nella dashboard."
        case .dashboardLimit: return "Hai raggiunto il limite di tre dashboard."
        case .widgetLimit: return "Hai raggiunto il limite di cinque misure."
        case .noIdentifier: return "Non ci sono identificatori disponibili."
        case .revisionLimit: return "Il limite delle revisioni è stato raggiunto."
        }
    }
}

/// Local storage only. Writing must either succeed or throw without accepting a new snapshot.
public protocol ConfigurationStorage {
    func read() throws -> Data?
    func write(_ data: Data) throws
}

public enum GeometryField: String, CaseIterable, Sendable {
    case x, y, width, height
    public var titleIT: String {
        switch self { case .x: return "Posizione X"; case .y: return "Posizione Y"; case .width: return "Larghezza"; case .height: return "Altezza" }
    }
}

/// Integer editing grid from the development contract. Circular bounds are warnings only.
public struct EditingRectangle: Equatable, Sendable {
    public private(set) var x: Int
    public private(set) var y: Int
    public private(set) var width: Int
    public private(set) var height: Int
    public init(_ frame: WidgetFrame) throws {
        guard frame.isValid else { throw ConfigurationError.invalidFrame }
        x = Int((frame.x * 10000).rounded()); y = Int((frame.y * 10000).rounded())
        width = Int((frame.width * 10000).rounded()); height = Int((frame.height * 10000).rounded())
    }
    public var frame: WidgetFrame { .init(x: Double(x)/10000, y: Double(y)/10000, width: Double(width)/10000, height: Double(height)/10000) }
    public subscript(field: GeometryField) -> Int {
        switch field { case .x: return x; case .y: return y; case .width: return width; case .height: return height }
    }
    public func range(for field: GeometryField) -> ClosedRange<Int> {
        switch field { case .x: return 0...(10000-width); case .y: return 0...(10000-height); case .width: return 1...(10000-x); case .height: return 1...(10000-y) }
    }
    public mutating func set(_ field: GeometryField, to requested: Int) {
        let range = range(for: field)
        let value = min(max(requested, range.lowerBound), range.upperBound)
        switch field { case .x: x = value; case .y: y = value; case .width: width = value; case .height: height = value }
    }
    public var clipsCircularDisplay: Bool {
        [(x,y),(x+width,y),(x,y+height),(x+width,y+height)].contains { point in
            let dx = Int64(point.0-5000), dy = Int64(point.1-5000)
            return dx*dx + dy*dy > 25_000_000
        }
    }
}

public struct EditorSelection: Equatable, Sendable {
    public let dashboardID: Int
    public let widgetID: Int
    public init(dashboardID: Int, widgetID: Int) { self.dashboardID = dashboardID; self.widgetID = widgetID }
}

/// A working draft is never the saved snapshot until local persistence succeeds.
public struct ConfigurationDraft: Sendable {
    public private(set) var saved: GaugeConfiguration
    public var working: GaugeConfiguration
    public private(set) var selection: EditorSelection?
    public var hasChanges: Bool { working != saved }

    public init(configuration: GaugeConfiguration, catalog: [PIDDescriptor]) throws {
        try ConfigurationValidator.validate(configuration, catalog: catalog)
        saved = configuration; working = configuration
    }
    public mutating func select(dashboardID: Int, widgetID: Int) throws {
        let dashboard = try dashboardIndex(dashboardID)
        guard working.dashboards[dashboard].widgets.contains(where: { $0.id == widgetID }) else { throw EditorError.missingWidget }
        selection = .init(dashboardID: dashboardID, widgetID: widgetID)
    }
    public mutating func cancel() { working = saved; selection = nil }
    public mutating func restore(_ configuration: GaugeConfiguration, catalog: [PIDDescriptor]) throws {
        try ConfigurationValidator.validate(configuration, catalog: catalog)
        working = configuration
        working.revision = saved.revision
        selection = nil
    }
    public mutating func reload(from storage: ConfigurationStorage, catalog: [PIDDescriptor]) throws {
        guard let data = try storage.read() else { return }
        let candidate = try JSONDecoder().decode(GaugeConfiguration.self, from: data)
        try ConfigurationValidator.validate(candidate, catalog: catalog)
        saved = candidate; working = candidate; selection = nil
    }
    public mutating func save(to storage: ConfigurationStorage, catalog: [PIDDescriptor]) throws {
        guard saved.revision < 2_147_483_647 else { throw EditorError.revisionLimit }
        var candidate = working
        candidate.revision = saved.revision + 1
        try ConfigurationValidator.validate(candidate, catalog: catalog)
        let data = try JSONEncoder().encode(candidate)
        try storage.write(data)
        saved = candidate; working = candidate
    }
    @discardableResult
    public mutating func addDashboard(pid: PIDDescriptor) throws -> Int {
        guard working.dashboards.count < 3 else { throw EditorError.dashboardLimit }
        let id = try nextID(working.dashboards.map(\.id))
        working.dashboards.append(Dashboard(id: id, widgets: [.init(id: 1, pid: pid, frame: .init(x: 0.25, y: 0.3, width: 0.5, height: 0.3))]))
        return id
    }
    public mutating func removeDashboard(id: Int) throws {
        let index = try dashboardIndex(id)
        guard working.dashboards.count > 1 else { throw EditorError.lastDashboard }
        working.dashboards.remove(at: index)
        if selection?.dashboardID == id { selection = nil }
    }
    @discardableResult
    public mutating func addWidget(dashboardID: Int, pid: PIDDescriptor) throws -> Int {
        let index = try dashboardIndex(dashboardID)
        guard working.dashboards[index].widgets.count < 5 else { throw EditorError.widgetLimit }
        let id = try nextID(working.dashboards[index].widgets.map(\.id))
        working.dashboards[index].widgets.append(.init(id: id, pid: pid, frame: .init(x: 0.25, y: 0.35, width: 0.5, height: 0.2)))
        selection = .init(dashboardID: dashboardID, widgetID: id)
        return id
    }
    public mutating func removeWidget(dashboardID: Int, widgetID: Int) throws {
        let dashboard = try dashboardIndex(dashboardID)
        guard let widget = working.dashboards[dashboard].widgets.firstIndex(where: { $0.id == widgetID }) else { throw EditorError.missingWidget }
        guard working.dashboards[dashboard].widgets.count > 1 else { throw EditorError.lastWidget }
        working.dashboards[dashboard].widgets.remove(at: widget)
        if selection == .init(dashboardID: dashboardID, widgetID: widgetID) { selection = nil }
    }
    public mutating func setGeometry(dashboardID: Int, widgetID: Int, field: GeometryField, units: Int) throws {
        let dashboard = try dashboardIndex(dashboardID)
        guard let widget = working.dashboards[dashboard].widgets.firstIndex(where: { $0.id == widgetID }) else { throw EditorError.missingWidget }
        var rectangle = try EditingRectangle(working.dashboards[dashboard].widgets[widget].frame)
        rectangle.set(field, to: units)
        working.dashboards[dashboard].widgets[widget].frame = rectangle.frame
    }
    public func clippedWidgetIDs(dashboardID: Int) -> Set<Int> {
        guard let dashboard = working.dashboards.first(where: { $0.id == dashboardID }) else { return [] }
        return Set(dashboard.widgets.filter { (try? EditingRectangle($0.frame).clipsCircularDisplay) == true }.map(\.id))
    }
    private func dashboardIndex(_ id: Int) throws -> Int {
        guard let index = working.dashboards.firstIndex(where: { $0.id == id }) else { throw EditorError.missingDashboard }
        return index
    }
    private func nextID(_ existing: [Int]) throws -> Int {
        let used = Set(existing)
        let high = existing.max() ?? 0
        if high < 255 { return high + 1 }
        guard let id = (1...255).first(where: { !used.contains($0) }) else { throw EditorError.noIdentifier }
        return id
    }
}

/// Atomic file replacement for local drafts; hardware power-loss recovery is a separate concern.
public struct AtomicFileConfigurationStorage: ConfigurationStorage, Sendable {
    public let fileURL: URL
    public init(fileURL: URL) { self.fileURL = fileURL }
    public func read() throws -> Data? {
        guard FileManager.default.fileExists(atPath: fileURL.path) else { return nil }
        return try Data(contentsOf: fileURL)
    }
    public func write(_ data: Data) throws {
        try FileManager.default.createDirectory(at: fileURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try data.write(to: fileURL, options: .atomic)
    }
}
