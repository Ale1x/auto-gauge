public enum GaugeRPCOperation: String, Sendable {
    case hello, configuration = "config.get", validate = "config.validate", apply = "config.apply", logs = "logs.read", acknowledge = "logs.ack"
    public var mutates: Bool { self == .apply || self == .acknowledge }
}
