#if DEBUG
import SwiftUI
import ImageIO
import UniformTypeIdentifiers

/// Debug-only static rendering of the actual DeviceView. No UI automation or network.
@MainActor
enum DesignSnapshotRenderer {
    static func render(model: AppModel) throws {
        let folder = URL.documentsDirectory.appending(path: "DesignSnapshots")
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let cases: [(String, Double, DynamicTypeSize, ColorScheme)] = [
            ("narrow-light", 320, .large, .light),
            ("narrow-dark", 320, .large, .dark),
            ("narrow-accessibility", 320, .accessibility3, .light),
            ("wide-light", 640, .large, .light)
        ]
        for (name, width, type, scheme) in cases {
            let view = VStack(spacing: 0) { SimulationNotice(); DeviceContentView() }
                .environment(model).environment(\.locale, Locale(identifier: "it_IT"))
                .environment(\.dynamicTypeSize, type).environment(\.colorScheme, scheme)
                .frame(width: width).fixedSize(horizontal: false, vertical: true).background(AppTheme.canvas).tint(AppTheme.accent).environment(\.colorScheme, scheme)
            let renderer = ImageRenderer(content: view)
            renderer.scale = 1
            renderer.proposedSize = ProposedViewSize(width: width, height: nil)
            guard let image = renderer.cgImage,
                  let destination = CGImageDestinationCreateWithURL(folder.appending(path: name+".png") as CFURL, UTType.png.identifier as CFString, 1, nil) else { throw CocoaError(.fileWriteUnknown) }
            CGImageDestinationAddImage(destination, image, nil)
            guard CGImageDestinationFinalize(destination) else { throw CocoaError(.fileWriteUnknown) }
        }
        try Data("Static SwiftUI ImageRenderer output; full DeviceContentView reused by the screen, without ScrollView/navigation chrome. No visual accessibility conformance claim.\n".utf8)
            .write(to: folder.appending(path: "README.txt"), options: .atomic)
    }
}
#endif
