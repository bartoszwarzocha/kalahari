/// @file plugin_signature.cpp
/// @brief Implementation of plugin signature verification

#include <kalahari/core/plugin_signature.h>
#include <kalahari/core/trusted_keys.h>
#include <kalahari/core/logger.h>
#include <nlohmann/json.hpp>
#include <zip.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace kalahari {
namespace core {

// ============================================================================
// Public API
// ============================================================================

std::string PluginSignature::resultToString(VerifyResult result) {
    switch (result) {
        case VerifyResult::Valid:
            return "Valid";
        case VerifyResult::Invalid:
            return "Invalid";
        case VerifyResult::NotFound:
            return "NotFound";
        case VerifyResult::KeyNotTrusted:
            return "KeyNotTrusted";
        case VerifyResult::FormatError:
            return "FormatError";
        default:
            return "Unknown";
    }
}

PluginSignature::VerifyResult PluginSignature::verify(
    const std::filesystem::path& kpluginPath,
    std::string& signedBy,
    std::string& errorMessage
) {
    signedBy.clear();
    errorMessage.clear();

    // Check if plugin file exists
    if (!std::filesystem::exists(kpluginPath)) {
        errorMessage = "Plugin file does not exist: " + kpluginPath.string();
        Logger::getInstance().error("PluginSignature: {}", errorMessage);
        return VerifyResult::FormatError;
    }

    // Check for signature file
    std::filesystem::path sigPath = kpluginPath;
    sigPath += ".sig";

    if (!std::filesystem::exists(sigPath)) {
        errorMessage = "Signature file not found: " + sigPath.string();
        Logger::getInstance().debug("PluginSignature: {}", errorMessage);
        return VerifyResult::NotFound;
    }

    // Parse signature file
    std::string algorithm;
    std::string expectedHash;
    std::vector<uint8_t> signature;
    std::string publisherId;

    if (!parseSignatureFile(sigPath, algorithm, expectedHash, signature, publisherId)) {
        errorMessage = "Failed to parse signature file";
        Logger::getInstance().error("PluginSignature: {}", errorMessage);
        return VerifyResult::FormatError;
    }

    // Verify algorithm
    if (algorithm != "ed25519") {
        errorMessage = "Unsupported signature algorithm: " + algorithm;
        Logger::getInstance().error("PluginSignature: {}", errorMessage);
        return VerifyResult::FormatError;
    }

    // Check if publisher is trusted
    auto& trustedKeys = TrustedKeys::getInstance();
    if (!trustedKeys.isTrusted(publisherId)) {
        errorMessage = "Publisher not trusted: " + publisherId;
        Logger::getInstance().warn("PluginSignature: {}", errorMessage);
        return VerifyResult::KeyNotTrusted;
    }

    // Get publisher's public key
    auto publicKeyOpt = trustedKeys.getPublicKey(publisherId);
    if (!publicKeyOpt.has_value()) {
        errorMessage = "Failed to retrieve public key for: " + publisherId;
        Logger::getInstance().error("PluginSignature: {}", errorMessage);
        return VerifyResult::KeyNotTrusted;
    }

    const std::vector<uint8_t>& publicKey = publicKeyOpt.value();

    // Compute archive hash
    std::string actualHash = computeArchiveHash(kpluginPath);
    if (actualHash.empty()) {
        errorMessage = "Failed to compute archive hash";
        Logger::getInstance().error("PluginSignature: {}", errorMessage);
        return VerifyResult::FormatError;
    }

    // Verify hash matches
    if (actualHash != expectedHash) {
        errorMessage = "Archive hash mismatch. Expected: " + expectedHash +
                       ", Actual: " + actualHash;
        Logger::getInstance().error("PluginSignature: {}", errorMessage);
        return VerifyResult::Invalid;
    }

    // Prepare message for verification (the hash bytes)
    std::vector<uint8_t> messageBytes;
    messageBytes.reserve(actualHash.size());
    for (char c : actualHash) {
        messageBytes.push_back(static_cast<uint8_t>(c));
    }

    // Verify Ed25519 signature
    if (!verifyEd25519(publicKey, messageBytes, signature)) {
        errorMessage = "Signature verification failed";
        Logger::getInstance().error("PluginSignature: {}", errorMessage);
        return VerifyResult::Invalid;
    }

    // Success
    signedBy = publisherId;
    Logger::getInstance().info("PluginSignature: Verified {} signed by {}",
                               kpluginPath.filename().string(), publisherId);
    return VerifyResult::Valid;
}

std::string PluginSignature::computeArchiveHash(
    const std::filesystem::path& kpluginPath
) {
    int zipError = 0;
    zip_t* archive = zip_open(kpluginPath.string().c_str(), ZIP_RDONLY, &zipError);

    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, zipError);
        Logger::getInstance().error("PluginSignature: Failed to open archive {}: {}",
                                    kpluginPath.filename().string(),
                                    zip_error_strerror(&error));
        zip_error_fini(&error);
        return "";
    }

    // Get number of entries
    zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    if (numEntries < 0) {
        Logger::getInstance().error("PluginSignature: Failed to get entry count");
        zip_close(archive);
        return "";
    }

    // Collect file names and content hashes
    struct FileEntry {
        std::string name;
        std::vector<uint8_t> contentHash;
    };
    std::vector<FileEntry> entries;

    for (zip_int64_t i = 0; i < numEntries; ++i) {
        const char* entryName = zip_get_name(archive, i, 0);
        if (!entryName) {
            continue;
        }

        std::string nameStr(entryName);

        // Skip directories (end with /)
        if (!nameStr.empty() && nameStr.back() == '/') {
            continue;
        }

        // Open file in archive
        zip_file_t* zipFile = zip_fopen_index(archive, i, 0);
        if (!zipFile) {
            Logger::getInstance().warn("PluginSignature: Failed to open entry: {}", nameStr);
            continue;
        }

        // Read file content and compute SHA256
        EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
        if (!mdCtx) {
            zip_fclose(zipFile);
            continue;
        }

        if (EVP_DigestInit_ex(mdCtx, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(mdCtx);
            zip_fclose(zipFile);
            continue;
        }

        char buffer[8192];
        zip_int64_t bytesRead;
        while ((bytesRead = zip_fread(zipFile, buffer, sizeof(buffer))) > 0) {
            EVP_DigestUpdate(mdCtx, buffer, static_cast<size_t>(bytesRead));
        }

        zip_fclose(zipFile);

        std::vector<uint8_t> hash(EVP_MD_size(EVP_sha256()));
        unsigned int hashLen = 0;
        EVP_DigestFinal_ex(mdCtx, hash.data(), &hashLen);
        EVP_MD_CTX_free(mdCtx);

        hash.resize(hashLen);
        entries.push_back({nameStr, hash});
    }

    zip_close(archive);

    // Sort entries by name (deterministic order)
    std::sort(entries.begin(), entries.end(),
              [](const FileEntry& a, const FileEntry& b) {
                  return a.name < b.name;
              });

    // Compute final hash: SHA256(name1 || hash1 || name2 || hash2 || ...)
    EVP_MD_CTX* finalCtx = EVP_MD_CTX_new();
    if (!finalCtx) {
        return "";
    }

    if (EVP_DigestInit_ex(finalCtx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(finalCtx);
        return "";
    }

    for (const auto& entry : entries) {
        EVP_DigestUpdate(finalCtx, entry.name.data(), entry.name.size());
        EVP_DigestUpdate(finalCtx, entry.contentHash.data(), entry.contentHash.size());
    }

    std::vector<uint8_t> finalHash(EVP_MD_size(EVP_sha256()));
    unsigned int finalHashLen = 0;
    EVP_DigestFinal_ex(finalCtx, finalHash.data(), &finalHashLen);
    EVP_MD_CTX_free(finalCtx);

    finalHash.resize(finalHashLen);
    return bytesToHex(finalHash);
}

// ============================================================================
// Private Implementation
// ============================================================================

bool PluginSignature::verifyEd25519(
    const std::vector<uint8_t>& publicKey,
    const std::vector<uint8_t>& message,
    const std::vector<uint8_t>& signature
) {
    // Ed25519 public key must be 32 bytes
    if (publicKey.size() != 32) {
        Logger::getInstance().error("PluginSignature: Invalid public key size: {}",
                                    publicKey.size());
        return false;
    }

    // Ed25519 signature must be 64 bytes
    if (signature.size() != 64) {
        Logger::getInstance().error("PluginSignature: Invalid signature size: {}",
                                    signature.size());
        return false;
    }

    // Create EVP_PKEY from raw public key
    EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519,
        nullptr,
        publicKey.data(),
        publicKey.size()
    );

    if (!pkey) {
        Logger::getInstance().error("PluginSignature: Failed to create EVP_PKEY");
        return false;
    }

    // Create verification context
    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    if (!mdCtx) {
        EVP_PKEY_free(pkey);
        Logger::getInstance().error("PluginSignature: Failed to create EVP_MD_CTX");
        return false;
    }

    // Initialize verification (Ed25519 uses DigestVerifyInit with NULL digest)
    if (EVP_DigestVerifyInit(mdCtx, nullptr, nullptr, nullptr, pkey) != 1) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(pkey);
        Logger::getInstance().error("PluginSignature: EVP_DigestVerifyInit failed");
        return false;
    }

    // Verify signature (Ed25519 does signing and verification in one step)
    int result = EVP_DigestVerify(
        mdCtx,
        signature.data(),
        signature.size(),
        message.data(),
        message.size()
    );

    EVP_MD_CTX_free(mdCtx);
    EVP_PKEY_free(pkey);

    return result == 1;
}

bool PluginSignature::parseSignatureFile(
    const std::filesystem::path& sigPath,
    std::string& algorithm,
    std::string& archiveHash,
    std::vector<uint8_t>& signature,
    std::string& signedBy
) {
    try {
        // Read signature file
        std::ifstream file(sigPath);
        if (!file) {
            Logger::getInstance().error("PluginSignature: Cannot open signature file: {}",
                                        sigPath.string());
            return false;
        }

        nlohmann::json sigJson;
        file >> sigJson;

        // Validate version
        int version = sigJson.value("version", 0);
        if (version != 1) {
            Logger::getInstance().error("PluginSignature: Unsupported signature version: {}",
                                        version);
            return false;
        }

        // Extract fields
        algorithm = sigJson.at("algorithm").get<std::string>();
        archiveHash = sigJson.at("archive_hash").get<std::string>();
        signedBy = sigJson.at("signed_by").get<std::string>();

        // Decode base64 signature
        std::string signatureBase64 = sigJson.at("signature").get<std::string>();
        signature = base64Decode(signatureBase64);

        if (signature.empty()) {
            Logger::getInstance().error("PluginSignature: Failed to decode signature");
            return false;
        }

        return true;
    }
    catch (const nlohmann::json::exception& e) {
        Logger::getInstance().error("PluginSignature: JSON parsing error: {}", e.what());
        return false;
    }
    catch (const std::exception& e) {
        Logger::getInstance().error("PluginSignature: Error parsing signature: {}", e.what());
        return false;
    }
}

std::vector<uint8_t> PluginSignature::base64Decode(const std::string& base64) {
    // Calculate decoded length
    size_t inputLen = base64.size();
    if (inputLen == 0) {
        return {};
    }

    // Count padding
    size_t padding = 0;
    if (inputLen >= 1 && base64[inputLen - 1] == '=') padding++;
    if (inputLen >= 2 && base64[inputLen - 2] == '=') padding++;

    // Calculate output length
    size_t outputLen = (inputLen / 4) * 3 - padding;

    std::vector<uint8_t> decoded(outputLen);

    // Use OpenSSL's EVP_DecodeBlock
    EVP_ENCODE_CTX* ctx = EVP_ENCODE_CTX_new();
    if (!ctx) {
        return {};
    }

    EVP_DecodeInit(ctx);

    int outLen = 0;
    int tmpLen = 0;

    if (EVP_DecodeUpdate(ctx,
                         decoded.data(),
                         &outLen,
                         reinterpret_cast<const unsigned char*>(base64.data()),
                         static_cast<int>(inputLen)) < 0) {
        EVP_ENCODE_CTX_free(ctx);
        return {};
    }

    if (EVP_DecodeFinal(ctx, decoded.data() + outLen, &tmpLen) < 0) {
        EVP_ENCODE_CTX_free(ctx);
        return {};
    }

    outLen += tmpLen;
    EVP_ENCODE_CTX_free(ctx);

    decoded.resize(static_cast<size_t>(outLen));
    return decoded;
}

std::string PluginSignature::base64Encode(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return "";
    }

    // Calculate output length (4 bytes for every 3 input bytes, rounded up)
    size_t outputLen = ((data.size() + 2) / 3) * 4 + 1;  // +1 for null terminator

    std::vector<unsigned char> encoded(outputLen);

    EVP_ENCODE_CTX* ctx = EVP_ENCODE_CTX_new();
    if (!ctx) {
        return "";
    }

    EVP_EncodeInit(ctx);

    int outLen = 0;
    EVP_EncodeUpdate(ctx,
                     encoded.data(),
                     &outLen,
                     data.data(),
                     static_cast<int>(data.size()));

    int finalLen = 0;
    EVP_EncodeFinal(ctx, encoded.data() + outLen, &finalLen);

    EVP_ENCODE_CTX_free(ctx);

    // Remove any trailing newlines added by OpenSSL
    std::string result(reinterpret_cast<char*>(encoded.data()), outLen + finalLen);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}

std::string PluginSignature::bytesToHex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

} // namespace core
} // namespace kalahari
