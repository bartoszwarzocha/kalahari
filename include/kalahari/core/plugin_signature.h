/// @file plugin_signature.h
/// @brief Plugin signature verification using Ed25519
///
/// Provides Ed25519-based signature verification for .kplugin archives.
/// Signature files are stored as <plugin>.kplugin.sig alongside the archive.
///
/// Signature file format (JSON):
/// @code{.json}
/// {
///   "version": 1,
///   "algorithm": "ed25519",
///   "signed_by": "kalahari-dev",
///   "archive_hash": "<hex SHA256 of archive contents>",
///   "signature": "<base64 encoded Ed25519 signature>"
/// }
/// @endcode

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace kalahari {
namespace core {

/// @brief Plugin signature verification using Ed25519.
///
/// Verifies .kplugin archives have valid signatures from trusted publishers.
/// Signature files are stored as <plugin>.kplugin.sig
class PluginSignature {
public:
    /// @brief Verification result status.
    enum class VerifyResult {
        Valid,           ///< Signature is valid and trusted
        Invalid,         ///< Signature verification failed
        NotFound,        ///< No .sig file exists
        KeyNotTrusted,   ///< Signed by unknown/untrusted key
        FormatError      ///< Malformed signature file
    };

    /// @brief Convert VerifyResult to human-readable string
    /// @param result Verification result
    /// @return String representation of the result
    static std::string resultToString(VerifyResult result);

    /// @brief Verify plugin signature.
    ///
    /// @param kpluginPath Path to .kplugin file
    /// @param[out] signedBy Publisher ID if verification succeeds
    /// @param[out] errorMessage Error description if verification fails
    /// @return Verification result
    static VerifyResult verify(
        const std::filesystem::path& kpluginPath,
        std::string& signedBy,
        std::string& errorMessage
    );

    /// @brief Compute deterministic hash of archive contents.
    ///
    /// Hashes all files in archive in sorted order (alphabetically).
    /// Format: SHA256(sorted_file1_name || sorted_file1_content || ...)
    ///
    /// @param kpluginPath Path to .kplugin file
    /// @return Hex-encoded SHA256 hash, or empty on error
    static std::string computeArchiveHash(
        const std::filesystem::path& kpluginPath
    );

private:
    /// @brief Verify Ed25519 signature using OpenSSL.
    ///
    /// @param publicKey 32-byte Ed25519 public key
    /// @param message Message that was signed
    /// @param signature 64-byte Ed25519 signature
    /// @return true if signature is valid
    static bool verifyEd25519(
        const std::vector<uint8_t>& publicKey,
        const std::vector<uint8_t>& message,
        const std::vector<uint8_t>& signature
    );

    /// @brief Read and parse signature file.
    ///
    /// @param sigPath Path to .sig file
    /// @param[out] algorithm Signature algorithm (should be "ed25519")
    /// @param[out] archiveHash Expected archive hash (hex)
    /// @param[out] signature Base64-decoded signature bytes
    /// @param[out] signedBy Publisher ID
    /// @return true if parsing succeeded
    static bool parseSignatureFile(
        const std::filesystem::path& sigPath,
        std::string& algorithm,
        std::string& archiveHash,
        std::vector<uint8_t>& signature,
        std::string& signedBy
    );

    /// @brief Decode base64 string to bytes.
    ///
    /// @param base64 Base64-encoded string
    /// @return Decoded bytes, or empty vector on error
    static std::vector<uint8_t> base64Decode(const std::string& base64);

    /// @brief Encode bytes to base64 string.
    ///
    /// @param data Bytes to encode
    /// @return Base64-encoded string
    static std::string base64Encode(const std::vector<uint8_t>& data);

    /// @brief Convert bytes to hex string.
    ///
    /// @param data Bytes to convert
    /// @return Lowercase hex string
    static std::string bytesToHex(const std::vector<uint8_t>& data);
};

} // namespace core
} // namespace kalahari
