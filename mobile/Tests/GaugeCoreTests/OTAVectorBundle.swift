#if os(macOS)
import Foundation

struct OTAVectorBundle: Decodable, Sendable {
    let schema_version: Int
    let hardware_id: String
    let inactive_capacity: Int
    let accepted_counter: String
    let key_id: String
    let public_key_der_base64: String
    let image_base64: String
    let cases: [OTAVectorCase]
    static let generated: Result<Self,Error> = Result {
        let root = URL(fileURLWithPath:#filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let temporary = FileManager.default.temporaryDirectory.appendingPathComponent("ag-swift-ota-"+UUID().uuidString,isDirectory:true)
        try FileManager.default.createDirectory(at:temporary,withIntermediateDirectories:true)
        defer { try? FileManager.default.removeItem(at:temporary) }
        let output = temporary.appendingPathComponent("vectors.json")
        let log = temporary.appendingPathComponent("generator.log")
        _ = FileManager.default.createFile(atPath:log.path,contents:nil)
        let stream = try FileHandle(forWritingTo:log); defer { try? stream.close() }
        let process = Process(); process.executableURL = URL(fileURLWithPath:"/usr/bin/env")
        process.arguments = ["python3",root.appendingPathComponent("scripts/generate_ota_vectors.py").path,"--output",output.path]
        var environment = ProcessInfo.processInfo.environment; environment["TMPDIR"] = temporary.path
        process.environment = environment
        process.standardOutput = stream; process.standardError = stream
        try process.run()
        let deadline = Date().addingTimeInterval(120)
        while process.isRunning && Date() < deadline { Thread.sleep(forTimeInterval:0.01) }
        if process.isRunning { process.terminate(); throw NSError(domain:"OTAFixture",code:1,userInfo:[NSLocalizedDescriptionKey:"Generator deadline exceeded"]) }
        guard process.terminationStatus == 0 else {
            throw NSError(domain:"OTAFixture",code:2,userInfo:[NSLocalizedDescriptionKey:String(decoding:try Data(contentsOf:log),as:UTF8.self)])
        }
        return try JSONDecoder().decode(Self.self,from:Data(contentsOf:output))
    }
}
#endif
