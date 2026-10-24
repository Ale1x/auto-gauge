import Foundation

public struct DemoLogImport: Sendable {
    public let deviceID: String
    public let pages: [Data]
    public static func load() throws -> Self {
        struct BundleContents: Decodable { let device_id: String; let pages: [HistoricalLogPage] }
        guard let url = Bundle.module.url(forResource: "log-demo-pages", withExtension: "json") else { throw LogImportError.invalidPage }
        let data = try Data(contentsOf: url)
        guard data.count <= 16 * 1024 else { throw LogImportError.capacity }
        let contents = try JSONDecoder().decode(BundleContents.self, from: data)
        guard contents.device_id == "SIMULATO-001", contents.pages.count <= 16 else { throw LogImportError.invalidPage }
        let pages = try contents.pages.map { page in
            let data = try JSONEncoder().encode(page)
            _ = try HistoricalLogPage.decode(data)
            return data
        }
        return Self(deviceID: contents.device_id, pages: pages)
    }
}
