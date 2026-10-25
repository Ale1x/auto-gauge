import Foundation
import XCTest
@testable import GaugeCore

final class ThemeCatalogTests: XCTestCase {
    func testAllRequestedSportsLinesAreAvailableAndOriginalIsFree() throws {
        let catalog = try GaugeThemeCatalog.load()
        XCTAssertTrue(Set(["bmw_m", "mercedes_amg", "audi_rs", "porsche_gt3"]).isSubset(of: Set(catalog.map(\.id))))
        XCTAssertEqual(catalog.filter(\.isFree).map(\.id), ["original"])
        XCTAssertGreaterThanOrEqual(catalog.count, 60)
        XCTAssertEqual(Set(catalog.map(\.visualStyle)), Set(GaugeThemeStyle.allCases))
        XCTAssertGreaterThanOrEqual(catalog.filter { $0.visualStyle == .pixel || $0.visualStyle == .arcade }.count, 8)
    }
    func testProductIdentitiesAreUniqueStableAndBoundToTheApp() throws {
        let ids = try GaugeThemeCatalog.load().compactMap(\.productID)
        XCTAssertEqual(Set(ids).count, ids.count)
        XCTAssertTrue(ids.allSatisfy { $0.hasPrefix("it.ale1x.autogauge.theme.") })
        XCTAssertNil(GaugeThemeCatalog.original.productID)
        XCTAssertNil(GaugeThemeCatalog.find("unknown"))
    }
    func testCatalogSurvivesRoundTripWithoutChangingItsAppearance() throws {
        let catalog = try GaugeThemeCatalog.load()
        XCTAssertEqual(try JSONDecoder().decode([GaugeTheme].self, from: JSONEncoder().encode(catalog)), catalog)
        XCTAssertNotEqual(GaugeThemeCatalog.find("audi_rs")?.accent, GaugeThemeCatalog.find("bmw_m")?.accent)
    }
}
