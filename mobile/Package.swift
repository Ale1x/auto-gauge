// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "AutoGaugeMobile",
    platforms: [.iOS(.v17), .macOS(.v13)],
    products: [.library(name: "GaugeCore", targets: ["GaugeCore"])],
    targets: [
        .target(name: "GaugeCore"),
        .testTarget(name: "GaugeCoreTests", dependencies: ["GaugeCore"])
    ]
)
