public enum GaugeEndpointFailureKind: String, Sendable {
    case configurationNotOperational = "configuration_not_operational"
    case historyNotOperational = "history_not_operational"
    case outcomeUnknown = "outcome_unknown"
}
