import Foundation
import XCTest
@testable import GaugeCore

final class ConfigurationTests: XCTestCase {
    func testDemoRoundTripAndValidation() throws {
        let data = try JSONEncoder().encode(DemoData.configuration)
        let config = try JSONDecoder().decode(GaugeConfiguration.self, from: data)
        XCTAssertEqual(config, DemoData.configuration)
        XCTAssertNoThrow(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
    }
    func testDashboardAndWidgetBounds() throws {
        var config = DemoData.configuration
        config.dashboards = []
        XCTAssertThrowsError(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
        config = DemoData.configuration
        config.dashboards[0].widgets = []
        XCTAssertThrowsError(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
        config = DemoData.configuration
        let widget = config.dashboards[0].widgets[0]
        config.dashboards[0].widgets = (0..<6).map { .init(id: $0 + 1, pid: widget.pid, frame: widget.frame) }
        XCTAssertThrowsError(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
    }
    func testInvalidGeometryAndNonFiniteCoordinates() {
        for frame in [WidgetFrame(x: -0.1, y: 0, width: 0.5, height: 0.5), .init(x: 0.8, y: 0, width: 0.5, height: 0.5), .init(x: .nan, y: 0, width: 0.5, height: 0.5), .init(x: 0, y: 0, width: 0, height: 0.5)] {
            var config = DemoData.configuration
            config.dashboards[0].widgets[0].frame = frame
            XCTAssertThrowsError(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
        }
    }
    func testUnsupportedAndNonService01Rejected() {
        var config = DemoData.configuration
        config.dashboards[0].widgets[0].pid.service = 4
        XCTAssertThrowsError(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
        config = DemoData.configuration
        config.dashboards[0].widgets[0].pid.pid = 255
        XCTAssertThrowsError(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
    }
    func testDuplicateIdentifiersAndBrightnessRejected() {
        var config = DemoData.configuration
        config.dashboards[0].widgets[1].id = config.dashboards[0].widgets[0].id
        XCTAssertThrowsError(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
        config = DemoData.configuration
        config.brightness.manualPercent = 101
        XCTAssertThrowsError(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
    }
    func testInvalidSimulatedUploadPreservesPreviousConfig() async throws {
        let transport = SimulatedGaugeTransport()
        let before = await transport.configuration()
        var invalid = before
        invalid.dashboards = []
        do { try await transport.apply(invalid); XCTFail("La configurazione non valida è stata accettata") }
        catch { }
        let after = await transport.configuration()
        XCTAssertEqual(before, after)
    }
    func testQuantizationCannotCreateInvalidRectangle() {
        for frame in [WidgetFrame(x: 0, y: 0, width: 0.00001, height: 0.5),
                      WidgetFrame(x: 0.00005, y: 0, width: 0.99995, height: 0.5)] {
            var config = DemoData.configuration
            config.dashboards[0].widgets[0].frame = frame
            XCTAssertThrowsError(try ConfigurationValidator.validate(config, catalog: DemoData.catalog))
            XCTAssertThrowsError(try JSONEncoder().encode(config))
        }
    }
    func testDecodedPIDUsesActualCapabilitiesInsteadOfDemoCatalog() throws {
        var config = DemoData.configuration
        let realPID = PIDDescriptor(pid: 0x10, nameIT: "Portata aria", unit: "g/s")
        config.dashboards[0].widgets[0].pid = realPID
        let decoded = try JSONDecoder().decode(GaugeConfiguration.self, from: JSONEncoder().encode(config))
        XCTAssertNoThrow(try ConfigurationValidator.validate(decoded, catalog: DemoData.catalog + [realPID]))
        XCTAssertThrowsError(try ConfigurationValidator.validate(decoded, catalog: DemoData.catalog))
    }
    func testRealTransportFailsClosed() async {
        do { try await UnconfiguredGaugeTransport().apply(DemoData.configuration); XCTFail("Accesso reale autorizzato senza autenticazione") }
        catch { XCTAssertTrue(error is GaugeTransportError) }
    }
}

final class SharedContractTests: XCTestCase {
    private var fixtureRoot: URL {
        URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().appendingPathComponent("shared/fixtures")
    }
    func testSharedGoldenConfigurationRoundTripsWithoutExtraWireFields() throws {
        let original = try Data(contentsOf: fixtureRoot.appendingPathComponent("config.valid.json"))
        let config = try JSONDecoder().decode(GaugeConfiguration.self, from: original)
        try ConfigurationValidator.validate(config, catalog: DemoData.catalog)
        let encoded = try JSONEncoder().encode(config)
        XCTAssertEqual(try JSONSerialization.jsonObject(with: original) as? NSDictionary,
                       try JSONSerialization.jsonObject(with: encoded) as? NSDictionary)
    }
    func testSharedContractCases() throws {
        let data = try Data(contentsOf: fixtureRoot.appendingPathComponent("config.cases.json"))
        let cases = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [[String: Any]])
        for item in cases {
            let name = try XCTUnwrap(item["name"] as? String)
            let expected = try XCTUnwrap(item["valid"] as? Bool)
            let encoded = try JSONSerialization.data(withJSONObject: XCTUnwrap(item["configuration"]))
            let accepted: Bool
            do {
                let config = try JSONDecoder().decode(GaugeConfiguration.self, from: encoded)
                try ConfigurationValidator.validate(config, catalog: DemoData.catalog)
                accepted = true
            } catch { accepted = false }
            XCTAssertEqual(accepted, expected, name)
        }
    }
}
