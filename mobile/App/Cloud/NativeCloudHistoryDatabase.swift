import Foundation
import CloudKit
import GaugeCore

/// Created only after explicit Development configuration/profile gating.
actor NativeCloudHistoryDatabase: CloudHistoryPullDatabase {
    private let container: CKContainer
    init(containerIdentifier: String) { container = CKContainer(identifier: containerIdentifier) }
    func account() async throws -> CloudAccount {
        switch try await container.accountStatus() {
        case .available:
            let id = try await container.userRecordID()
            return .available(context: CloudHistoryItem.digest(Data(id.recordName.utf8)))
        case .noAccount: return .noAccount
        case .restricted: return .restricted
        case .temporarilyUnavailable, .couldNotDetermine: return .temporarilyUnavailable
        @unknown default: return .temporarilyUnavailable
        }
    }
    func putImmutable(_ item: CloudHistoryItem, context: String) async throws {
        try await requireAccount(context)
        let database = container.privateCloudDatabase
        let id = CKRecord.ID(recordName: item.recordID)
        let payload = try item.payload()
        do {
            let existing = try await database.record(for: id)
            try await requireAccount(context)
            try check(existing, payload: payload)
            return
        } catch let error as CKError where error.code == .unknownItem { /* Create below. */ }
        try await requireAccount(context)
        try Task.checkCancellation()
        let record = CKRecord(recordType: "AGHistoryV1", recordID: id)
        record["schemaVersion"] = 1 as NSNumber
        record["payload"] = payload as NSData
        record["digest"] = CloudHistoryItem.digest(payload) as NSString
        do {
            let value = try await CloudRecordSave.perform(record: record, database: database)
            try await requireAccount(context)
            try check(value, payload: payload)
        } catch let error as CKError where error.code == .serverRecordChanged {
            try await requireAccount(context)
            guard let server = error.userInfo[CKRecordChangedErrorServerRecordKey] as? CKRecord else { throw CloudSyncError.conflict }
            try check(server, payload: payload)
        }
    }
    func readHistory(context: String, cursor: Data?, limit: Int) async throws -> CloudHistoryPullPage {
        try await requireAccount(context)
        do {
            let page = try await CloudRecordQuery.perform(database: container.privateCloudDatabase, cursor: cursor, limit: limit)
            try await requireAccount(context)
            return page
        } catch let error as CKError where cursor != nil && (error.code == .invalidArguments || error.code == .changeTokenExpired) {
            throw CloudSyncError.cursorExpired
        }
    }
    private func requireAccount(_ context: String) async throws {
        try Task.checkCancellation()
        guard try await account() == .available(context: context) else { throw CloudSyncError.accountChanged }
        try Task.checkCancellation()
    }
    private func check(_ record: CKRecord, payload: Data) throws {
        guard record.recordType == "AGHistoryV1", record["schemaVersion"] as? Int == 1,
              record["payload"] as? Data == payload,
              record["digest"] as? String == CloudHistoryItem.digest(payload) else { throw CloudSyncError.conflict }
    }
}
