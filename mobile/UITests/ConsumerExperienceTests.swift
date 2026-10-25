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
            if name != "44-remove-confirmation" { XCTAssertTrue(bannerExit.isHittable) }
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
        XCTAssertTrue(app.staticTexts["screens.heading"].waitForExistence(timeout: 5))
        capture("02-screens-light")
        app.tabBars.buttons["Temi"].tap()
        XCTAssertTrue(app.buttons["themes.search"].waitForExistence(timeout: 5))
        XCTAssertTrue(app.buttons["theme.card.bmw_m"].waitForExistence(timeout: 5))
        XCTAssertFalse(app.tabBars.buttons["Viaggi"].exists)
        capture("03-themes-light")
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
        app.tabBars.buttons["Temi"].tap()
        let bmwPreview = app.buttons["theme.card.bmw_m"]
        XCTAssertTrue(bmwPreview.waitForExistence(timeout: 8)); bmwPreview.tap()
        XCTAssertTrue(app.buttons["Acquisti in arrivo"].waitForExistence(timeout: 5))
        XCTAssertFalse(app.buttons["Acquisti in arrivo"].isEnabled)
        app.navigationBars.buttons.firstMatch.tap()
        app.tabBars.buttons["Schermate"].tap()
        XCTAssertTrue(app.staticTexts["Collega il dispositivo"].exists)
        app.tabBars.buttons["Impostazioni"].tap()
        app.switches["settings.demo"].switches.firstMatch.tap()
        XCTAssertTrue(app.staticTexts["Modalità demo"].waitForExistence(timeout: 10))
    }
    func testConfigurationSavePersists() {
        app.tabBars.buttons["Schermate"].tap()
        let create = app.buttons["screens.add"]
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
        XCTAssertTrue(app.buttons["screen.edit.2"].exists)
        XCTAssertFalse(button("configuration.save").exists)
    }
    func testThemePurchaseSelectionAndRestore() {
        app.tabBars.buttons["Temi"].tap()
        let bmw = app.buttons["theme.card.bmw_m"]
        XCTAssertTrue(bmw.waitForExistence(timeout: 5)); bmw.tap()
        capture("19-bmw-theme-detail")
        let buy = app.buttons["theme.buy"]
        XCTAssertTrue(buy.waitForExistence(timeout: 5)); buy.tap()
        app.buttons["Conferma acquisto"].tap()
        let use = app.buttons["theme.use"]
        XCTAssertTrue(use.waitForExistence(timeout: 5)); use.tap()
        XCTAssertTrue(app.buttons["Tema in uso"].waitForExistence(timeout: 5))
        app.terminate(); app.launchArguments = ["--uitesting"]; app.launch()
        app.tabBars.buttons["Impostazioni"].tap()
        XCTAssertTrue(app.descendants(matching: .any)["settings.currentTheme"].waitForExistence(timeout: 5))
        XCTAssertEqual(app.descendants(matching: .any)["settings.currentTheme"].value as? String, "BMW M Sport")
        app.buttons["themes.restore"].tap()
        XCTAssertTrue(app.staticTexts["La tua raccolta è aggiornata"].waitForExistence(timeout: 5))
        capture("20-theme-owned")
    }
    func testMarketplaceAuditScreens() {
        app.tabBars.buttons["Temi"].tap()
        let bmw = app.buttons.matching(NSPredicate(format: "label BEGINSWITH %@", "BMW M Sport,")).firstMatch
        XCTAssertTrue(bmw.waitForExistence(timeout: 8))
        capture("30-marketplace-after")
        bmw.tap()
        XCTAssertTrue(app.navigationBars["Anteprima"].waitForExistence(timeout: 5))
        XCTAssertFalse(app.tabBars.buttons["Temi"].exists)
        capture("31-purchase-after")
        app.navigationBars.buttons.firstMatch.tap()
        app.tabBars.buttons["Impostazioni"].tap()
        button("settings.appearance").tap(); app.buttons["Scuro"].tap()
        app.tabBars.buttons["Temi"].tap()
        XCTAssertTrue(bmw.waitForExistence(timeout: 8))
        capture("32-marketplace-dark")
        bmw.tap()
        XCTAssertTrue(app.navigationBars["Anteprima"].waitForExistence(timeout: 5))
        capture("33-purchase-dark")
        app.navigationBars.buttons.firstMatch.tap()
        app.buttons["themes.category.Pixel"].tap()
        let pixel = app.buttons["theme.card.pixel_turbo"]
        XCTAssertTrue(pixel.waitForExistence(timeout: 5))
        capture("34-pixel-collection")
        pixel.tap()
        capture("35-pixel-detail")
    }
    func testCreateFreePersonalTheme() {
        app.tabBars.buttons["Temi"].tap()
        app.segmentedControls.buttons["La tua raccolta"].tap()
        let create = app.buttons["themes.create"]
        XCTAssertTrue(create.waitForExistence(timeout: 8)); create.tap()
        XCTAssertTrue(app.navigationBars["Crea il tuo tema"].waitForExistence(timeout: 5))
        let save = app.buttons["theme.custom.save"]
        if !save.isHittable { app.swipeUp() }
        XCTAssertTrue(save.waitForExistence(timeout: 5)); save.tap()
        XCTAssertTrue(app.staticTexts["Il tuo tema è in uso"].waitForExistence(timeout: 5))
        capture("22-custom-theme")
        app.tabBars.buttons["Impostazioni"].tap()
        XCTAssertEqual(app.descendants(matching: .any)["settings.currentTheme"].value as? String, "Il mio tema")
    }
    func testThemeSearchAndAudiPreview() {
        app.tabBars.buttons["Temi"].tap()
        app.buttons["themes.search"].tap()
        let search = app.searchFields.firstMatch
        XCTAssertTrue(search.waitForExistence(timeout: 5)); search.tap(); search.typeText("Audi\n")
        let audi = app.buttons["theme.card.audi_rs"]
        XCTAssertTrue(audi.waitForExistence(timeout: 5)); audi.tap()
        XCTAssertTrue(app.navigationBars["Anteprima"].waitForExistence(timeout: 5))
        capture("21-audi-theme-detail")
        XCTAssertTrue(app.buttons["theme.buy"].exists)
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
    func testScreenOverviewManagement() {
        app.tabBars.buttons["Schermate"].tap()
        capture("40-screens-overview-light")
        let add = app.buttons["screens.add"]
        XCTAssertTrue(add.waitForExistence(timeout: 5)); add.tap()
        XCTAssertTrue(app.buttons["screen.edit.2"].waitForExistence(timeout: 5))
        if !add.isHittable { app.swipeUp() }; add.tap()
        XCTAssertTrue(app.buttons["screen.edit.3"].waitForExistence(timeout: 5))
        XCTAssertFalse(add.exists)
        app.swipeUp()
        capture("41-three-screens")
        let options = app.buttons["screen.options.3"]
        if !options.isHittable { app.swipeUp() }; options.tap()
        app.buttons["Elimina schermata"].tap()
        capture("44-remove-confirmation")
        XCTAssertTrue(app.buttons["Annulla"].waitForExistence(timeout: 5)); app.buttons["Annulla"].tap()
        XCTAssertTrue(app.buttons["screen.edit.3"].exists)
        options.tap(); app.buttons["Elimina schermata"].tap()
        app.buttons["Elimina schermata"].tap()
        XCTAssertTrue(app.buttons["screen.edit.3"].waitForNonExistence(timeout: 5))
    }
    func testScreenOverviewAppearance() {
        app.tabBars.buttons["Impostazioni"].tap()
        button("settings.appearance").tap(); app.buttons["Scuro"].tap()
        app.tabBars.buttons["Schermate"].tap()
        capture("42-screens-overview-dark")
        app.terminate()
        app.launchArguments = ["--uitesting-reset", "-UIPreferredContentSizeCategoryName", "UICTContentSizeCategoryAccessibilityXXXL"]
        app.launch()
        app.tabBars.buttons["Schermate"].tap()
        XCTAssertTrue(app.buttons["screen.edit.1"].waitForExistence(timeout: 5))
        capture("43-screens-large-text")
        app.buttons["screen.edit.1"].tap()
        XCTAssertTrue(app.navigationBars["Modifica schermata"].waitForExistence(timeout: 5))
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
