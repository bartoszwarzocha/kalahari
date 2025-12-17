/// @file trusted_keys.cpp
/// @brief Implementation of TrustedKeys singleton

#include <kalahari/core/trusted_keys.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/logger.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <fstream>

namespace kalahari {
namespace core {

// ============================================================================
// Helper Functions
// ============================================================================

std::string trustLevelToString(TrustLevel level) {
    switch (level) {
        case TrustLevel::Full:
            return "Full";
        case TrustLevel::Verified:
            return "Verified";
        case TrustLevel::User:
            return "User";
        default:
            return "Unknown";
    }
}

static TrustLevel trustLevelFromString(const std::string& str) {
    if (str == "full" || str == "Full") {
        return TrustLevel::Full;
    } else if (str == "verified" || str == "Verified") {
        return TrustLevel::Verified;
    } else {
        return TrustLevel::User;
    }
}

// Forward declaration for base64 encoding helper
static std::string base64EncodeInternal(const std::vector<uint8_t>& data);

// ============================================================================
// Singleton Instance
// ============================================================================

TrustedKeys& TrustedKeys::getInstance() {
    static TrustedKeys instance;
    return instance;
}

// ============================================================================
// Public API
// ============================================================================

bool TrustedKeys::loadBuiltinKeys() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find resources directory (same logic as ResourcePaths but simplified)
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList searchPaths;

    // Priority order for finding resources
    searchPaths << QDir::cleanPath(appDir + "/resources");
    searchPaths << QDir::cleanPath(appDir + "/../../resources");
    searchPaths << QDir::cleanPath(appDir + "/../../../resources");
    searchPaths << QDir::cleanPath(appDir + "/../resources");

    QString resourcesDir;
    for (const QString& path : searchPaths) {
        QString keysFile = path + "/keys/trusted_publishers.json";
        if (QFileInfo::exists(keysFile)) {
            resourcesDir = path;
            break;
        }
    }

    if (resourcesDir.isEmpty()) {
        Logger::getInstance().warn("TrustedKeys: Resources directory not found");
        return false;
    }

    QString keysPath = resourcesDir + "/keys/trusted_publishers.json";
    std::filesystem::path filePath(keysPath.toStdString());

    if (!std::filesystem::exists(filePath)) {
        Logger::getInstance().warn("TrustedKeys: Built-in keys file not found: {}",
                                   filePath.string());
        return false;
    }

    try {
        std::ifstream file(filePath);
        if (!file) {
            Logger::getInstance().error("TrustedKeys: Cannot open keys file: {}",
                                        filePath.string());
            return false;
        }

        nlohmann::json keysJson;
        file >> keysJson;

        // Validate version
        int version = keysJson.value("version", 0);
        if (version != 1) {
            Logger::getInstance().error("TrustedKeys: Unsupported keys file version: {}",
                                        version);
            return false;
        }

        // Parse publishers
        auto publishers = keysJson.value("publishers", nlohmann::json::object());
        int loadedCount = 0;

        for (auto& [publisherId, publisherData] : publishers.items()) {
            TrustedPublisher publisher;
            publisher.id = publisherId;
            publisher.name = publisherData.value("name", publisherId);

            // Decode base64 public key
            std::string publicKeyBase64 = publisherData.value("public_key", "");
            publisher.publicKey = base64Decode(publicKeyBase64);

            if (publisher.publicKey.size() != 32) {
                Logger::getInstance().warn(
                    "TrustedKeys: Invalid public key size for {}: {} (expected 32)",
                    publisherId, publisher.publicKey.size()
                );
                continue;
            }

            // Parse trust level
            std::string trustLevelStr = publisherData.value("trust_level", "user");
            publisher.trustLevel = trustLevelFromString(trustLevelStr);

            m_publishers[publisherId] = publisher;
            loadedCount++;

            Logger::getInstance().debug("TrustedKeys: Loaded publisher '{}' ({})",
                                        publisher.name,
                                        trustLevelToString(publisher.trustLevel));
        }

        Logger::getInstance().info("TrustedKeys: Loaded {} built-in publisher keys",
                                   loadedCount);
        return loadedCount > 0;
    }
    catch (const nlohmann::json::exception& e) {
        Logger::getInstance().error("TrustedKeys: JSON parsing error: {}", e.what());
        return false;
    }
    catch (const std::exception& e) {
        Logger::getInstance().error("TrustedKeys: Error loading keys: {}", e.what());
        return false;
    }
}

void TrustedKeys::loadUserKeys() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& settings = SettingsManager::getInstance();

    // User keys are stored as JSON array in settings
    // Format: plugins.trustedKeys = [{"id": "...", "name": "...", "publicKey": "base64..."}]
    try {
        std::string keysJson = settings.get<std::string>("plugins.trustedKeys", "[]");
        auto userKeys = nlohmann::json::parse(keysJson);

        int loadedCount = 0;
        for (const auto& keyData : userKeys) {
            std::string publisherId = keyData.value("id", "");
            if (publisherId.empty()) {
                continue;
            }

            // Don't override built-in keys
            if (m_publishers.count(publisherId) > 0 &&
                m_publishers[publisherId].trustLevel != TrustLevel::User) {
                Logger::getInstance().warn(
                    "TrustedKeys: Ignoring user key that conflicts with built-in: {}",
                    publisherId
                );
                continue;
            }

            TrustedPublisher publisher;
            publisher.id = publisherId;
            publisher.name = keyData.value("name", publisherId);
            publisher.trustLevel = TrustLevel::User;

            std::string publicKeyBase64 = keyData.value("publicKey", "");
            publisher.publicKey = base64Decode(publicKeyBase64);

            if (publisher.publicKey.size() != 32) {
                Logger::getInstance().warn(
                    "TrustedKeys: Invalid user key size for {}: {}",
                    publisherId, publisher.publicKey.size()
                );
                continue;
            }

            m_publishers[publisherId] = publisher;
            loadedCount++;

            Logger::getInstance().debug("TrustedKeys: Loaded user key '{}'", publisher.name);
        }

        if (loadedCount > 0) {
            Logger::getInstance().info("TrustedKeys: Loaded {} user-added keys", loadedCount);
        }
    }
    catch (const std::exception& e) {
        Logger::getInstance().warn("TrustedKeys: Error loading user keys: {}", e.what());
    }
}

std::optional<std::vector<uint8_t>> TrustedKeys::getPublicKey(
    const std::string& publisherId
) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_publishers.find(publisherId);
    if (it != m_publishers.end()) {
        return it->second.publicKey;
    }
    return std::nullopt;
}

bool TrustedKeys::isTrusted(const std::string& publisherId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_publishers.count(publisherId) > 0;
}

std::optional<TrustedPublisher> TrustedKeys::getPublisher(
    const std::string& publisherId
) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_publishers.find(publisherId);
    if (it != m_publishers.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<TrustedPublisher> TrustedKeys::getAllPublishers() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<TrustedPublisher> result;
    result.reserve(m_publishers.size());

    for (const auto& [id, publisher] : m_publishers) {
        result.push_back(publisher);
    }

    return result;
}

bool TrustedKeys::addUserKey(
    const std::string& publisherId,
    const std::string& name,
    const std::string& publicKeyBase64
) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Don't override built-in keys
    auto it = m_publishers.find(publisherId);
    if (it != m_publishers.end() && it->second.trustLevel != TrustLevel::User) {
        Logger::getInstance().error(
            "TrustedKeys: Cannot override built-in key: {}", publisherId
        );
        return false;
    }

    // Decode and validate public key
    std::vector<uint8_t> publicKey = base64Decode(publicKeyBase64);
    if (publicKey.size() != 32) {
        Logger::getInstance().error(
            "TrustedKeys: Invalid public key size: {} (expected 32)",
            publicKey.size()
        );
        return false;
    }

    TrustedPublisher publisher;
    publisher.id = publisherId;
    publisher.name = name;
    publisher.publicKey = publicKey;
    publisher.trustLevel = TrustLevel::User;

    m_publishers[publisherId] = publisher;

    Logger::getInstance().info("TrustedKeys: Added user key '{}' ({})",
                               name, publisherId);

    // Save to settings
    saveUserKeys();

    return true;
}

bool TrustedKeys::removeUserKey(const std::string& publisherId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_publishers.find(publisherId);
    if (it == m_publishers.end()) {
        Logger::getInstance().warn("TrustedKeys: Key not found: {}", publisherId);
        return false;
    }

    // Only user keys can be removed
    if (it->second.trustLevel != TrustLevel::User) {
        Logger::getInstance().error(
            "TrustedKeys: Cannot remove built-in key: {}", publisherId
        );
        return false;
    }

    m_publishers.erase(it);

    Logger::getInstance().info("TrustedKeys: Removed user key '{}'", publisherId);

    // Save to settings
    saveUserKeys();

    return true;
}

void TrustedKeys::saveUserKeys() {
    // Note: Must be called with m_mutex already held

    nlohmann::json userKeys = nlohmann::json::array();

    for (const auto& [id, publisher] : m_publishers) {
        if (publisher.trustLevel == TrustLevel::User) {
            // Encode public key to base64
            std::string publicKeyBase64 = base64EncodeInternal(publisher.publicKey);

            userKeys.push_back({
                {"id", publisher.id},
                {"name", publisher.name},
                {"publicKey", publicKeyBase64}
            });
        }
    }

    auto& settings = SettingsManager::getInstance();
    settings.set("plugins.trustedKeys", userKeys.dump());
    settings.save();
}

void TrustedKeys::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_publishers.clear();
}

// ============================================================================
// Private Implementation
// ============================================================================

std::vector<uint8_t> TrustedKeys::base64Decode(const std::string& base64) const {
    if (base64.empty()) {
        return {};
    }

    // Calculate decoded length
    size_t inputLen = base64.size();

    // Count padding
    size_t padding = 0;
    if (inputLen >= 1 && base64[inputLen - 1] == '=') padding++;
    if (inputLen >= 2 && base64[inputLen - 2] == '=') padding++;

    // Calculate output length
    size_t outputLen = (inputLen / 4) * 3 - padding;

    std::vector<uint8_t> decoded(outputLen + 1);  // +1 for safety

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

// Helper function for base64 encoding (internal use)
static std::string base64EncodeInternal(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return "";
    }

    // Calculate output length
    size_t outputLen = ((data.size() + 2) / 3) * 4 + 1;

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

    // Remove trailing newlines
    std::string result(reinterpret_cast<char*>(encoded.data()), outLen + finalLen);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}

} // namespace core
} // namespace kalahari
