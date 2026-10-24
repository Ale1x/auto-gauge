import Foundation
import CryptoKit
import Security

public enum OTAPackageVerifier {
    public static let maximumEnvelopeBytes = 4096
    public static let maximumImageBytes = 16 * 1024 * 1024
    /// Trust, actual inactive capacity and durable replay floor must be supplied
    /// by the caller. No state, boot slot or floor is advanced by verification.
    /// Key bytes must be DER PKCS#1 RSAPublicKey, never an envelope-provided key.
    public static func verify(envelope:Data,image:Data,trustedKeys:[String:Data],hardwareID:String,
                              inactiveCapacity:Int,acceptedCounter:UInt64) throws -> VerifiedOTACandidate {
        guard (1...maximumEnvelopeBytes).contains(envelope.count) else { throw OTAPackageError.envelope }
        guard inactiveCapacity > 0 else { throw OTAPackageError.capacity }
        let envelopeSnapshot = copy(envelope)
        guard let fields = try OTAStrictJSON.parse(envelopeSnapshot).object,
              Set(fields.keys) == ["manifest","signature_algorithm","signature"],
              fields["signature_algorithm"]?.string == "RSA-PSS-SHA256", let manifestJSON = fields["manifest"] else { throw OTAPackageError.schema }
        let manifest = try OTAManifest(json:manifestJSON)
        try manifest.validateESPProfile()
        guard manifest.hardwareID == hardwareID else { throw OTAPackageError.hardware }
        guard manifest.releaseCounter > acceptedCounter else { throw OTAPackageError.replay }
        guard manifest.imageBytes <= inactiveCapacity else { throw OTAPackageError.capacity }
        guard let keyData = trustedKeys[manifest.signingKeyID] else { throw OTAPackageError.untrustedKey }
        guard let signatureText = fields["signature"]?.string, signatureText.utf8.count == 512,
              let signature = Data(base64Encoded:signatureText), signature.count == 384,
              signature.base64EncodedString() == signatureText else { throw OTAPackageError.signature }
        guard (1...8192).contains(keyData.count) else { throw OTAPackageError.publicKey }
        let key = try OTARSAPublicKey(pkcs1DER:copy(keyData)).key
        // SDK SecKey.h specifies MGF1-SHA256 and a32-byte salt for this algorithm.
        guard SecKeyVerifySignature(key,.rsaSignatureMessagePSSSHA256,manifest.signingBytes as CFData,signature as CFData,nil) else { throw OTAPackageError.signature }
        guard image.count == manifest.imageBytes, image.count <= maximumImageBytes else { throw OTAPackageError.image }
        let imageSnapshot = copy(image)
        let digest = SHA256.hash(data:imageSnapshot).map { String(format:"%02x",$0) }.joined()
        guard digest == manifest.imageSHA256 else { throw OTAPackageError.image }
        return VerifiedOTACandidate(manifest:manifest,envelope:envelopeSnapshot,image:imageSnapshot)
    }
    private static func copy(_ data:Data) -> Data {
        data.withUnsafeBytes { bytes in
            guard let base = bytes.baseAddress else { return Data() }
            return Data(bytes:base,count:bytes.count)
        }
    }
}
