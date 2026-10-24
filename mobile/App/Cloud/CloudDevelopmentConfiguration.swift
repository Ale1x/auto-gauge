import Foundation

/// Fail closed before CKContainer construction. Live cloud is device-only in this prototype.
enum CloudDevelopmentConfiguration {
    static let container = "iCloud.it.ale1x.autogauge"
    static var enabled: Bool {
        #if targetEnvironment(simulator)
        return false
        #else
        let flag = Bundle.main.object(forInfoDictionaryKey: "AutoGaugeCloudDevelopmentEnabled")
        guard (flag as? Bool == true || flag as? String == "YES"),
              Bundle.main.bundleIdentifier == "it.ale1x.autogauge",
              let url = Bundle.main.url(forResource: "embedded", withExtension: "mobileprovision"),
              let profile = try? Data(contentsOf: url), profile.count <= 1024 * 1024,
              let start = profile.range(of: Data("<?xml".utf8)),
              let end = profile.range(of: Data("</plist>".utf8), in: start.lowerBound..<profile.endIndex),
              let plist = try? PropertyListSerialization.propertyList(from: profile.subdata(in: start.lowerBound..<end.upperBound), format: nil) as? [String: Any],
              let expiry = plist["ExpirationDate"] as? Date, expiry > .now,
              let teams = plist["TeamIdentifier"] as? [String], teams == ["ZDQFU74347"],
              let entitlements = plist["Entitlements"] as? [String: Any],
              entitlements["application-identifier"] as? String == "ZDQFU74347.it.ale1x.autogauge",
              entitlements["com.apple.developer.icloud-container-environment"] as? String == "Development",
              let containers = entitlements["com.apple.developer.icloud-container-identifiers"] as? [String], containers.contains(container),
              let services = entitlements["com.apple.developer.icloud-services"] as? [String], services.contains("CloudKit") else { return false }
        return true
        #endif
    }
}
