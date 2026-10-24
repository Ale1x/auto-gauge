import Foundation
import XCTest
@testable import GaugeCore

private final class MemoryConfigurationStorage: ConfigurationStorage {
    var data: Data?
    var failsWrite = false
    enum Failure: Error { case unavailable }
    func read() -> Data? { data }
    func write(_ data: Data) throws {
        if failsWrite { throw Failure.unavailable }
        self.data = data
    }
}

final class EditingTests: XCTestCase {
    func testCancelRestoresSavedConfigurationAndClearsSelection() throws {
        var draft = try ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
        try draft.select(dashboardID: 1, widgetID: 1)
        try draft.setGeometry(dashboardID: 1, widgetID: 1, field: .x, units: 1000)
        draft.working.brightness.manualPercent = 12
        XCTAssertTrue(draft.hasChanges)
        draft.cancel()
        XCTAssertEqual(draft.working, DemoData.configuration)
        XCTAssertFalse(draft.hasChanges)
        XCTAssertNil(draft.selection)
    }
    func testSaveReloadAndRestoreAreSeparateOperations() throws {
        let storage = MemoryConfigurationStorage()
        var draft = try ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
        draft.working.brightness.manualPercent = 23
        try draft.save(to: storage, catalog: DemoData.catalog)
        XCTAssertEqual(draft.saved.revision, 2)
        XCTAssertFalse(draft.hasChanges)
        try draft.restore(DemoData.configuration, catalog: DemoData.catalog)
        XCTAssertEqual(draft.working.revision, 2)
        XCTAssertTrue(draft.hasChanges)
        XCTAssertEqual(draft.saved.brightness.manualPercent, 23)
        try draft.reload(from: storage, catalog: DemoData.catalog)
        XCTAssertEqual(draft.working.brightness.manualPercent, 23)
        XCTAssertFalse(draft.hasChanges)
        var second = try ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
        try second.reload(from: storage, catalog: DemoData.catalog)
        XCTAssertEqual(second.saved, draft.saved)
    }
    func testWriteFailureOrInvalidReloadDoesNotReplaceSavedOrWorkingSnapshot() throws {
        let storage = MemoryConfigurationStorage()
        var draft = try ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
        draft.working.brightness.manualPercent = 35
        let saved = draft.saved, working = draft.working
        storage.failsWrite = true
        XCTAssertThrowsError(try draft.save(to: storage, catalog: DemoData.catalog))
        XCTAssertEqual(draft.saved, saved)
        XCTAssertEqual(draft.working, working)
        storage.data = Data("{broken".utf8)
        XCTAssertThrowsError(try draft.reload(from: storage, catalog: DemoData.catalog))
        XCTAssertEqual(draft.saved, saved)
        XCTAssertEqual(draft.working, working)
        var invalid = DemoData.configuration
        invalid.dashboards = []
        storage.data = try JSONEncoder().encode(invalid)
        XCTAssertThrowsError(try draft.reload(from: storage, catalog: DemoData.catalog))
        XCTAssertEqual(draft.working, working)
    }
    func testIDsStayStableAfterRemovalAndSelectionClearsOnlyWhenRemoved() throws {
        var draft = try ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
        let secondID = try draft.addDashboard(pid: DemoData.catalog[0])
        let thirdID = try draft.addDashboard(pid: DemoData.catalog[1])
        try draft.select(dashboardID: thirdID, widgetID: 1)
        try draft.removeDashboard(id: secondID)
        XCTAssertEqual(draft.working.dashboards.map(\.id), [1, thirdID])
        XCTAssertEqual(draft.selection?.dashboardID, thirdID)
        let newID = try draft.addDashboard(pid: DemoData.catalog[2])
        XCTAssertEqual(newID, 4)
        try draft.select(dashboardID: 1, widgetID: 3)
        try draft.removeWidget(dashboardID: 1, widgetID: 2)
        XCTAssertEqual(draft.working.dashboards[0].widgets.map(\.id), [1, 3])
        XCTAssertEqual(draft.selection?.widgetID, 3)
        let newWidget = try draft.addWidget(dashboardID: 1, pid: DemoData.catalog[3])
        XCTAssertEqual(newWidget, 4)
        try draft.removeWidget(dashboardID: 1, widgetID: newWidget)
        XCTAssertNil(draft.selection)
    }
    func testLimitsAndLastElementAreEnforcedWithoutChangingState() throws {
        var draft = try ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
        XCTAssertThrowsError(try draft.removeDashboard(id: 1))
        try draft.removeWidget(dashboardID: 1, widgetID: 3)
        try draft.removeWidget(dashboardID: 1, widgetID: 2)
        XCTAssertThrowsError(try draft.removeWidget(dashboardID: 1, widgetID: 1))
        for _ in 0..<4 { try draft.addWidget(dashboardID: 1, pid: DemoData.catalog[0]) }
        let before = draft.working
        XCTAssertThrowsError(try draft.addWidget(dashboardID: 1, pid: DemoData.catalog[0]))
        XCTAssertEqual(draft.working, before)
        try draft.addDashboard(pid: DemoData.catalog[0]); try draft.addDashboard(pid: DemoData.catalog[0])
        XCTAssertThrowsError(try draft.addDashboard(pid: DemoData.catalog[0]))
        XCTAssertNoThrow(try ConfigurationValidator.validate(draft.working, catalog: DemoData.catalog))
    }
    func testIntegerGridMovementAndResizeClampWithoutOverflow() throws {
        var draft = try ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
        try draft.setGeometry(dashboardID: 1, widgetID: 1, field: .x, units: Int.max)
        var rect = try EditingRectangle(draft.working.dashboards[0].widgets[0].frame)
        XCTAssertEqual(rect.x, 5000)
        try draft.setGeometry(dashboardID: 1, widgetID: 1, field: .width, units: Int.min)
        rect = try EditingRectangle(draft.working.dashboards[0].widgets[0].frame)
        XCTAssertEqual(rect.width, 1)
        try draft.setGeometry(dashboardID: 1, widgetID: 1, field: .x, units: Int.max)
        rect = try EditingRectangle(draft.working.dashboards[0].widgets[0].frame)
        XCTAssertEqual(rect.x, 9999)
        XCTAssertNoThrow(try ConfigurationValidator.validate(draft.working, catalog: DemoData.catalog))
        let data = try JSONEncoder().encode(draft.working)
        XCTAssertEqual(try JSONDecoder().decode(GaugeConfiguration.self, from: data), draft.working)
    }
    func testAtomicFileStorageRoundTripAndMissingFile() throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: directory) }
        let storage = AtomicFileConfigurationStorage(fileURL: directory.appendingPathComponent("draft/config.json"))
        XCTAssertNil(try storage.read())
        var draft = try ConfigurationDraft(configuration: DemoData.configuration, catalog: DemoData.catalog)
        draft.working.brightness.manualPercent = 17
        try draft.save(to: storage, catalog: DemoData.catalog)
        let data = try XCTUnwrap(storage.read())
        XCTAssertEqual(try JSONDecoder().decode(GaugeConfiguration.self, from: data), draft.saved)
        draft.working.brightness.manualPercent = 99
        try draft.reload(from: storage, catalog: DemoData.catalog)
        XCTAssertEqual(draft.working.brightness.manualPercent, 17)
    }
    func testCircleClippingIsWarningAndDoesNotChangeSharedRectangleValidation() throws {
        let centered = WidgetFrame(x: 0.3, y: 0.3, width: 0.4, height: 0.4)
        XCTAssertFalse(try EditingRectangle(centered).clipsCircularDisplay)
        let full = WidgetFrame(x: 0, y: 0, width: 1, height: 1)
        XCTAssertTrue(try EditingRectangle(full).clipsCircularDisplay)
        var config = DemoData.configuration
        config.dashboards[0].widgets[0].frame = full
        let draft = try ConfigurationDraft(configuration: config, catalog: DemoData.catalog)
        XCTAssertTrue(draft.clippedWidgetIDs(dashboardID: 1).contains(1))
        XCTAssertNoThrow(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
    }
}
