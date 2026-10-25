import Foundation
import XCTest
import GaugeCore
@testable import AppModelHarness

private actor QualificationSpy: GaugeNativeQualificationProvider {
    var calls = 0
    func qualification(for connection: GaugePipeConnection) async throws -> GaugeNativeQualification { calls += 1; throw GaugeLinkError.unqualified }
}
private actor InstallationSpy: OTAInstallationSessionProvider {
    var calls = 0
    func openSession(for candidate: VerifiedOTACandidate, deadline: ContinuousClock.Instant) async throws -> any OTAInstallationSession { calls += 1; throw OTATransferError.unqualified }
}
@MainActor final class AppModelExperienceTests: XCTestCase {
    private func context() throws -> (URL, UserDefaults) {
        NativeBluetoothController.nextPipe = nil
        let id = UUID().uuidString
        let directory = FileManager.default.temporaryDirectory.appending(path: "ag-app-experience-\(id)")
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let suite = "ag-app-experience-\(id)"
        let preferences = try XCTUnwrap(UserDefaults(suiteName: suite))
        addTeardownBlock { try? FileManager.default.removeItem(at: directory); UserDefaults.standard.removePersistentDomain(forName: suite) }
        return (directory, preferences)
    }
    func testAllDemoActionsAvoidNativeAndCloudBoundaries() async throws {
        let (directory, preferences) = try context(); let qualification = QualificationSpy(); let installation = InstallationSpy()
        NativeBluetoothController.packetRequests = 0
        let marker = Data("native untouched".utf8); try marker.write(to: directory.appending(path: "native-histories-v1.json"))
        let model = AppModel(nativeQualificationProvider: qualification, otaInstallationProvider: installation, applicationSupportDirectory: directory, preferences: preferences, demoStepDelay: .zero)
        await model.bootstrap()
        XCTAssertTrue(model.isDemoMode); XCTAssertTrue(model.gaugeConnected)
        XCTAssertEqual(model.logSessions.count, 2); XCTAssertEqual(model.logSessions.reduce(0) { $0 + $1.records.count }, 960)
        XCTAssertFalse(model.catalog.contains { $0.nameIT.hasPrefix("PID") })
        await model.synchronizeBackup(); XCTAssertNotNil(model.lastBackupDate)
        await model.restoreBackup(); XCTAssertEqual(model.logSessions.count, 2)
        await model.checkFirmwareUpdate(); await model.downloadFirmwareUpdate(); await model.installFirmwareUpdate()
        XCTAssertEqual(model.firmwareCurrentVersion, "0.14.0"); XCTAssertEqual(model.demoFirmwareState, .completed)
        XCTAssertEqual(model.cloud.calls, 0); XCTAssertEqual(NativeBluetoothController.packetRequests, 0)
        let qualificationCalls = await qualification.calls; let installationCalls = await installation.calls
        XCTAssertEqual(qualificationCalls, 0); XCTAssertEqual(installationCalls, 0)
        XCTAssertEqual(try Data(contentsOf: directory.appending(path: "native-histories-v1.json")), marker)
    }
    func testSingleSaveAndRestartPreservePhoneAndDemoDevice() async throws {
        let (directory, preferences) = try context()
        let model = AppModel(applicationSupportDirectory: directory, preferences: preferences, demoStepDelay: .zero)
        await model.bootstrap(); model.configuration.brightness.manualPercent = 31
        XCTAssertTrue(model.hasPendingChanges); await model.saveChanges()
        XCTAssertFalse(model.hasPendingChanges); XCTAssertEqual(model.gaugeSnapshot?.brightness.manualPercent, 31)
        let restarted = AppModel(applicationSupportDirectory: directory, preferences: preferences, demoStepDelay: .zero)
        await restarted.bootstrap()
        XCTAssertEqual(restarted.configuration.brightness.manualPercent, 31)
        XCTAssertEqual(restarted.gaugeSnapshot?.brightness.manualPercent, 31)
        XCTAssertFalse(restarted.hasPendingChanges)
    }
    func testModeSwitchInvalidatesFirmwareProgressAndNeverGrantsSimulatorAsReal() async throws {
        let (directory, preferences) = try context()
        let model = AppModel(applicationSupportDirectory: directory, preferences: preferences, demoStepDelay: .milliseconds(5))
        await model.bootstrap()
        let download = Task { await model.downloadFirmwareUpdate() }
        let deadline = ContinuousClock().now.advanced(by: .seconds(2))
        while model.demoFirmwareProgress == 0, ContinuousClock().now < deadline { await Task.yield() }
        XCTAssertEqual(model.demoFirmwareState, .downloading)
        await model.setDemoMode(false); await download.value
        XCTAssertFalse(model.isDemoMode); XCTAssertFalse(model.gaugeConnected)
        await model.connectGauge(); XCTAssertFalse(model.gaugeConnected)
        await model.installFirmwareUpdate(); XCTAssertEqual(model.firmwareCurrentVersion, "0.13.0")
        await model.setDemoMode(true)
        XCTAssertTrue(model.gaugeConnected); XCTAssertEqual(model.firmwareCurrentVersion, "0.13.0")
        await model.downloadFirmwareUpdate(); XCTAssertEqual(model.demoFirmwareState, .downloaded)
    }
    func testAppearancePreferencesAndSimpleErrorsSurviveRestart() async throws {
        let (directory, preferences) = try context()
        let model = AppModel(applicationSupportDirectory: directory, preferences: preferences)
        model.appearance = .dark; model.backupEnabled = false
        model.edit { _ in throw NSError(domain: "PRIVATE_TECHNICAL_DETAIL", code: 9) }
        XCTAssertFalse(model.message?.contains("PRIVATE_TECHNICAL_DETAIL") ?? true)
        XCTAssertTrue(model.diagnosticDetail?.contains("PRIVATE_TECHNICAL_DETAIL") ?? false)
        let restarted = AppModel(applicationSupportDirectory: directory, preferences: preferences)
        XCTAssertEqual(restarted.appearance, .dark); XCTAssertFalse(restarted.backupEnabled)
    }
    func testNativeOfflineSavePersistsAndReappearsAsPendingAfterRelaunch() async throws {
        let (directory, preferences) = try context(); let provider = QualifiedSessionTestProvider(); let pipe = QualifiedSessionTestPipe()
        let model = AppModel(nativeQualificationProvider: provider, applicationSupportDirectory: directory, preferences: preferences, demoStepDelay: .zero)
        await model.bootstrap(); await model.setDemoMode(false)
        NativeBluetoothController.nextPipe = pipe
        await model.activateNativeGauge(); XCTAssertTrue(model.isNativeSession); XCTAssertTrue(model.gaugeConnected)
        await model.disconnectGauge(); let before = await pipe.opcodes
        model.configuration.brightness.manualPercent = 31; await model.saveChanges()
        XCTAssertFalse(model.editor.hasChanges); XCTAssertTrue(model.hasPendingChanges); XCTAssertNil(model.message)
        XCTAssertEqual(model.editor.saved.brightness.manualPercent, 31)
        let after = await pipe.opcodes; XCTAssertEqual(before, after)
        await pipe.reconnect()
        let restarted = AppModel(nativeQualificationProvider: provider, applicationSupportDirectory: directory, preferences: preferences, demoStepDelay: .zero)
        await restarted.bootstrap(); await restarted.activateNativeGauge()
        XCTAssertTrue(restarted.hasPendingChanges); XCTAssertEqual(restarted.editor.saved.brightness.manualPercent, 31)
        await restarted.saveChanges()
        XCTAssertFalse(restarted.hasPendingChanges); XCTAssertEqual(restarted.gaugeSnapshot?.brightness.manualPercent, 31)
    }
    func testUnknownSaveWithNewEditingNeverClaimsOldSnapshotSavesNewValues() async throws {
        let (directory, preferences) = try context(); let provider = QualifiedSessionTestProvider(); let pipe = QualifiedSessionTestPipe()
        let model = AppModel(nativeQualificationProvider: provider, applicationSupportDirectory: directory, preferences: preferences, demoStepDelay: .zero)
        await model.bootstrap(); await model.setDemoMode(false); NativeBluetoothController.nextPipe = pipe
        await model.activateNativeGauge(); await pipe.setDropApplied(true)
        model.configuration.brightness.manualPercent = 31; await model.saveChanges()
        XCTAssertEqual(model.gaugeCommitState, .unknown)
        model.configuration.brightness.manualPercent = 45; await model.saveChanges()
        XCTAssertEqual(model.editor.saved.brightness.manualPercent, 45)
        XCTAssertTrue(model.hasPendingChanges); XCTAssertNotEqual(model.saveStatusText, "Modifiche salvate")
        let operations = await pipe.opcodes; XCTAssertEqual(operations.filter { $0 == "config.apply" }.count, 1)
    }
    func testConfiguredNativeBackupVerifiesThenCallsSelectedAction() async throws {
        let (directory, preferences) = try context()
        let model = AppModel(applicationSupportDirectory: directory, preferences: preferences, demoStepDelay: .zero)
        await model.bootstrap(); await model.setDemoMode(false); model.backupEnabled = true
        model.cloud.configured = true; model.cloud.account = .available(context: "LOCAL-TEST")
        let before = model.cloud.calls
        await model.synchronizeBackup(); XCTAssertEqual(model.cloud.calls, before + 2); XCTAssertNotNil(model.lastBackupDate)
        await model.restoreBackup(); XCTAssertEqual(model.cloud.calls, before + 4)
        XCTAssertEqual(model.backupStatusText, "Viaggi recuperati")
    }
    func testDefaultsUseOnlyAvailableMeasuresAndUnconfiguredCloudDoesNotPretendSuccess() async throws {
        let (directory, preferences) = try context()
        let model = AppModel(applicationSupportDirectory: directory, preferences: preferences, demoStepDelay: .zero)
        await model.bootstrap(); model.catalog = [.init(pid: 13, nameIT: "Velocità", unit: "km/h")]
        model.restoreDefaults(); XCTAssertEqual(model.configuration.dashboards.first?.widgets.map(\.pid.pid), [13])
        await model.setDemoMode(false); model.backupEnabled = true
        let callsBefore = model.cloud.calls
        await model.synchronizeBackup(); XCTAssertNil(model.lastBackupDate)
        XCTAssertEqual(model.backupStatusText, "Il backup iCloud non è disponibile in questa versione")
        await model.restoreBackup(); XCTAssertEqual(model.cloud.calls, callsBefore)
    }
}


extension AppModelExperienceTests {
    func testThemeDemoSelectionPersistsAndNeverGrantsNativeRights() async throws {
        let (_, preferences) = try context()
        let first = ThemeLibrary(preferences: preferences)
        let bmw = try XCTUnwrap(GaugeThemeCatalog.find("bmw_m"))
        await first.start(demo: true)
        XCTAssertTrue(first.store.products.isEmpty)
        XCTAssertFalse(first.owns(bmw, demo: false))
        first.acquireDemo(bmw); first.choose(bmw, demo: true)
        let reopened = ThemeLibrary(preferences: preferences)
        XCTAssertEqual(reopened.selected(demo: true).id, bmw.id)
        XCTAssertEqual(reopened.selected(demo: false).id, "original")
        XCTAssertFalse(reopened.owns(bmw, demo: false))
        await reopened.restore(demo: true)
        XCTAssertTrue(reopened.owns(bmw, demo: true))
        XCTAssertTrue(reopened.store.owned.isEmpty)
        reopened.saveCustom(accent: 0xabcdef, secondary: 0x123456, demo: true)
        let customized = ThemeLibrary(preferences: preferences)
        XCTAssertEqual(customized.selected(demo: true).accent, 0xabcdef)
        XCTAssertEqual(customized.selected(demo: false).id, "original")
        XCTAssertTrue(customized.selected(demo: true).isFree)
    }
    func testThemeUnknownStoredGrantsCannotEnableProducts() throws {
        let (_, preferences) = try context()
        preferences.set(["invented"], forKey: "demo.themes.owned")
        preferences.set("bmw_m", forKey: "device.themes.selected")
        let library = ThemeLibrary(preferences: preferences)
        XCTAssertEqual(library.demoOwned, ["original"])
        XCTAssertEqual(library.selected(demo: false).id, "original")
    }
}
