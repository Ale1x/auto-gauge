import XCTest

@MainActor
final class ConsumerExperienceTests: XCTestCase {
    private var app: XCUIApplication!
    override func setUp() async throws {
        continueAfterFailure = false
        app = XCUIApplication()
        app.launchArguments = ["--uitesting-reset"]
        app.launch()
        XCTAssertTrue(app.tabBars.buttons["Dispositivo"].waitForExistence(timeout: 15))
    }
    private func capture(_ name: String) {
        // Capture the final rendered frame after native navigation transitions.
        Thread.sleep(forTimeInterval: 0.7)
        let bannerExit = app.buttons["Esci dalla modalità demo"]
        if name != "07-device-mode" && name != "demo-exit-failure" {
            XCTAssertTrue(bannerExit.waitForExistence(timeout: 5))
            XCTAssertTrue(bannerExit.isHittable)
            XCTAssertGreaterThanOrEqual(bannerExit.frame.minY, 0)
        }
        let image = XCTAttachment(screenshot: XCUIScreen.main.screenshot())
        image.name = name; image.lifetime = .keepAlways; add(image)
    }
    private func button(_ identifier: String) -> XCUIElement {
        app.buttons[identifier]
    }
    func testDemoNavigationAndSingleBanner() {
        XCTAssertTrue(app.otherElements["demo.banner"].exists || app.staticTexts["Modalità demo"].exists)
        capture("01-device-light")
        app.tabBars.buttons["Schermate"].tap()
        XCTAssertTrue(app.navigationBars["Schermate"].waitForExistence(timeout: 5))
        capture("02-screens-light")
        app.tabBars.buttons["Viaggi"].tap()
        XCTAssertTrue(app.navigationBars["Viaggi"].waitForExistence(timeout: 5))
        XCTAssertTrue(app.cells.count > 0)
        capture("03-trips-light")
        app.tabBars.buttons["Impostazioni"].tap()
        XCTAssertTrue(app.navigationBars["Impostazioni"].waitForExistence(timeout: 5))
        XCTAssertEqual(app.buttons.matching(identifier: "Esci dalla modalità demo").count, 1)
        capture("04-settings-light")
        for word in ["Snapshot", "RAM volatile", "ACK", "Fixture", "Revisione locale", "PID", "ECU", "checkpoint"] {
            XCTAssertFalse(app.staticTexts.containing(NSPredicate(format: "label MATCHES[c] %@", ".*\\b" + NSRegularExpression.escapedPattern(for: word) + "\\b.*")).firstMatch.exists)
        }
    }
    func testAppearanceAndDeviceMode() {
        app.tabBars.buttons["Impostazioni"].tap()
        let appearance = app.buttons["settings.appearance"]
        XCTAssertTrue(appearance.waitForExistence(timeout: 5))
        appearance.tap()
        app.buttons["Scuro"].tap()
        capture("05-settings-dark")
        appearance.tap()
        app.buttons["Chiaro"].tap()
        capture("06-settings-light")
        let demo = app.switches["settings.demo"]
        if !demo.isHittable { app.swipeUp() }
        XCTAssertTrue(demo.waitForExistence(timeout: 5)); demo.switches.firstMatch.tap()
        XCTAssertTrue(app.tabBars.buttons["Dispositivo"].waitForExistence(timeout: 5))
        let exitedDemo = app.buttons["Esci dalla modalità demo"].waitForNonExistence(timeout: 10)
        if !exitedDemo { capture("demo-exit-failure"); print(app.debugDescription) }
        XCTAssertTrue(exitedDemo)
        XCTAssertTrue(app.buttons["Cerca dispositivo"].waitForExistence(timeout: 5))
        capture("07-device-mode")
        app.tabBars.buttons["Viaggi"].tap()
        XCTAssertFalse(app.staticTexts["Verso il lavoro"].exists)
        XCTAssertTrue(app.staticTexts["I tuoi viaggi saranno qui"].exists)
        app.tabBars.buttons["Schermate"].tap()
        XCTAssertTrue(app.staticTexts["Collega il dispositivo"].exists)
        app.tabBars.buttons["Impostazioni"].tap()
        app.switches["settings.demo"].switches.firstMatch.tap()
        XCTAssertTrue(app.staticTexts["Modalità demo"].waitForExistence(timeout: 10))
    }
    func testConfigurationSavePersists() {
        app.tabBars.buttons["Schermate"].tap()
        let create = app.buttons["Nuova schermata"]
        XCTAssertTrue(create.waitForExistence(timeout: 5))
        if !create.isHittable { app.swipeUp() }
        create.tap()
        let save = button("configuration.save")
        if !save.isHittable { app.swipeUp() }
        XCTAssertTrue(save.isEnabled); save.tap()
        XCTAssertTrue(app.staticTexts["Modifiche salvate"].waitForExistence(timeout: 10))
        capture("12-screens-saved")
        app.terminate(); app.launchArguments = ["--uitesting"]; app.launch()
        app.tabBars.buttons["Schermate"].tap()
        XCTAssertTrue(app.staticTexts["Schermata 2"].exists)
        XCTAssertFalse(button("configuration.save").isEnabled)
    }
    func testBackupAndTripDetails() {
        app.tabBars.buttons["Viaggi"].tap()
        let firstTrip = app.cells.containing(.staticText, identifier: "Verso il lavoro").firstMatch
        if firstTrip.exists { firstTrip.tap() } else { app.cells.element(boundBy: 1).tap() }
        XCTAssertTrue(app.navigationBars["Dettaglio viaggio"].waitForExistence(timeout: 5))
        XCTAssertTrue(app.navigationBars["Dettaglio viaggio"].buttons.firstMatch.isHittable)
        capture("09-trip-chart")
        app.tabBars.buttons["Impostazioni"].tap()
        let sync = button("backup.sync")
        XCTAssertTrue(sync.waitForExistence(timeout: 5)); sync.tap()
        XCTAssertTrue(app.staticTexts["Copia di sicurezza aggiornata"].waitForExistence(timeout: 5))
        button("backup.restore").tap()
        XCTAssertTrue(app.staticTexts["Viaggi recuperati"].waitForExistence(timeout: 5))
        capture("10-backup")
    }
    func testAppearancePersistsAfterRelaunch() {
        app.tabBars.buttons["Impostazioni"].tap()
        button("settings.appearance").tap(); app.buttons["Scuro"].tap()
        app.terminate(); app.launchArguments = ["--uitesting"]; app.launch()
        app.tabBars.buttons["Impostazioni"].tap()
        XCTAssertTrue(button("settings.appearance").label.contains("Scuro"))
        button("settings.appearance").tap(); app.buttons["Automatico"].tap()
        capture("11-appearance-system")
    }
    func testLargeTextRemainsNavigable() {
        app.terminate()
        app.launchArguments = ["--uitesting-reset", "-UIPreferredContentSizeCategoryName", "UICTContentSizeCategoryAccessibilityXXXL"]
        app.launch()
        XCTAssertTrue(app.tabBars.buttons["Dispositivo"].waitForExistence(timeout: 10))
        capture("13-device-large-text")
        app.swipeUp()
        capture("15-device-large-text-scrolled")
        app.tabBars.buttons["Impostazioni"].tap()
        XCTAssertTrue(app.buttons["settings.appearance"].waitForExistence(timeout: 5))
        capture("14-settings-large-text")
    }
    func testClusterGalleryAndEditor() {
        XCTAssertFalse(app.navigationBars.staticTexts["Auto Gauge"].exists)
        app.tabBars.buttons["Schermate"].tap()
        XCTAssertTrue(app.buttons["screen.edit.1"].waitForExistence(timeout: 5))
        capture("16-cluster-gallery")
        app.buttons["screen.edit.1"].tap()
        XCTAssertTrue(app.navigationBars["Modifica schermata"].waitForExistence(timeout: 5))
        capture("17-cluster-editor")
        app.navigationBars.buttons.firstMatch.tap()
        app.tabBars.buttons["Dispositivo"].tap()
        XCTAssertFalse(app.navigationBars.staticTexts["Auto Gauge"].exists)
    }
    func testDemoUpdateCompletes() {
        let updates = app.buttons["Aggiornamenti"]
        if !updates.isHittable { app.swipeUp() }
        if updates.exists { updates.tap() }
        else { app.staticTexts["Aggiornamenti"].tap() }
        XCTAssertTrue(app.navigationBars["Aggiornamenti"].buttons.firstMatch.waitForExistence(timeout: 5))
        XCTAssertTrue(app.navigationBars["Aggiornamenti"].buttons.firstMatch.isHittable)
        let check = button("firmware.check")
        XCTAssertTrue(check.waitForExistence(timeout: 5)); check.tap()
        let download = button("firmware.download")
        XCTAssertTrue(download.waitForExistence(timeout: 10)); download.tap()
        let install = button("firmware.install")
        XCTAssertTrue(install.waitForExistence(timeout: 15)); install.tap()
        app.buttons["Installa aggiornamento"].tap()
        XCTAssertTrue(check.waitForExistence(timeout: 20))
        capture("08-update-completed")
    }
}
