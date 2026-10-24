import Foundation
import Security

struct OTARSAPublicKey {
    let key: SecKey
    init(pkcs1DER:Data) throws {
        guard (1...8192).contains(pkcs1DER.count) else { throw OTAPackageError.publicKey }
        let bytes = Array(pkcs1DER)
        var index = 0
        let sequence = try Self.element(0x30,bytes:bytes,index:&index)
        guard index == bytes.count else { throw OTAPackageError.publicKey }
        let contents = Array(sequence); var inner = 0
        let modulus = try Self.element(0x02,bytes:contents,index:&inner)
        let exponent = try Self.element(0x02,bytes:contents,index:&inner)
        guard inner == contents.count, modulus.count == 385, modulus.first == 0,
              let high = modulus.dropFirst().first, high >= 128, Self.positiveInteger(exponent) else { throw OTAPackageError.publicKey }
        let attributes: [CFString:Any] = [kSecAttrKeyType:kSecAttrKeyTypeRSA,kSecAttrKeyClass:kSecAttrKeyClassPublic]
        guard let key = SecKeyCreateWithData(pkcs1DER as CFData,attributes as CFDictionary,nil),
              let attributes = SecKeyCopyAttributes(key) as? [CFString:Any],
              attributes[kSecAttrKeyType] as? String == kSecAttrKeyTypeRSA as String,
              attributes[kSecAttrKeyClass] as? String == kSecAttrKeyClassPublic as String,
              attributes[kSecAttrKeySizeInBits] as? Int == 3072,
              SecKeyGetBlockSize(key) == 384,
              SecKeyIsAlgorithmSupported(key,.verify,.rsaSignatureMessagePSSSHA256) else { throw OTAPackageError.publicKey }
        self.key = key
    }
    private static func positiveInteger(_ bytes:ArraySlice<UInt8>) -> Bool {
        guard let first = bytes.first, first < 128 else { return false }
        if first == 0 { return bytes.count > 1 && bytes.dropFirst().first.map { $0 >= 128 } == true }
        return true
    }
    private static func element(_ tag:UInt8,bytes:[UInt8],index:inout Int) throws -> ArraySlice<UInt8> {
        guard index + 2 <= bytes.count, bytes[index] == tag else { throw OTAPackageError.publicKey }
        index += 1; let byte = bytes[index]; index += 1
        var count = Int(byte)
        if byte >= 128 {
            let lengthBytes = Int(byte & 0x7f)
            guard (1...2).contains(lengthBytes), index + lengthBytes <= bytes.count, bytes[index] != 0 else { throw OTAPackageError.publicKey }
            count = 0
            for _ in 0..<lengthBytes { count = count * 256 + Int(bytes[index]); index += 1 }
            guard count >= 128, lengthBytes == 1 || count >= 256 else { throw OTAPackageError.publicKey }
        }
        guard count > 0, count <= bytes.count - index else { throw OTAPackageError.publicKey }
        defer { index += count }
        return bytes[index..<(index+count)]
    }
}
