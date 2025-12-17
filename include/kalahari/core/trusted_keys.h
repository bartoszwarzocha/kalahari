/// @file trusted_keys.h
/// @brief Manages trusted publisher keys for plugin verification
///
/// TrustedKeys is a singleton that manages Ed25519 public keys for plugin
/// signature verification. It loads built-in keys from resources and
/// user-configured keys from settings.
///
/// @example
/// @code
/// auto& keys = TrustedKeys::getInstance();
/// keys.loadBuiltinKeys();
///
/// if (keys.isTrusted("kalahari-dev")) {
///     auto pubKey = keys.getPublicKey("kalahari-dev");
///     // Use for verification...
/// }
/// @endcode

#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <mutex>

namespace kalahari {
namespace core {

/// @brief Trust level for plugin publishers.
enum class TrustLevel {
    Full,       ///< Official Kalahari publisher (highest trust)
    Verified,   ///< Community verified publisher
    User        ///< User-added publisher (requires confirmation)
};

/// @brief Convert TrustLevel to human-readable string
/// @param level Trust level
/// @return String representation
std::string trustLevelToString(TrustLevel level);

/// @brief Information about a trusted publisher.
struct TrustedPublisher {
    std::string id;                      ///< Publisher identifier (e.g., "kalahari-dev")
    std::string name;                    ///< Human-readable name
    std::vector<uint8_t> publicKey;      ///< 32-byte Ed25519 public key
    TrustLevel trustLevel;               ///< Trust level
};

/// @brief Manages trusted publisher keys for plugin verification.
///
/// Singleton that loads built-in keys from resources and user-configured keys
/// from settings. Thread-safe for concurrent access.
class TrustedKeys {
public:
    /// @brief Get singleton instance
    /// @return Reference to TrustedKeys instance
    static TrustedKeys& getInstance();

    // Non-copyable
    TrustedKeys(const TrustedKeys&) = delete;
    TrustedKeys& operator=(const TrustedKeys&) = delete;

    /// @brief Load built-in keys from resources.
    ///
    /// Loads keys from resources/keys/trusted_publishers.json.
    /// Called during application startup.
    /// @return true if keys loaded successfully
    bool loadBuiltinKeys();

    /// @brief Load user-configured keys from settings.
    ///
    /// Loads keys stored in user settings (added via Settings dialog).
    void loadUserKeys();

    /// @brief Get public key for a publisher.
    ///
    /// @param publisherId Publisher identifier
    /// @return Public key bytes (32 bytes), or nullopt if not found
    [[nodiscard]] std::optional<std::vector<uint8_t>> getPublicKey(
        const std::string& publisherId
    ) const;

    /// @brief Check if publisher is trusted.
    ///
    /// @param publisherId Publisher identifier
    /// @return true if publisher exists in trusted keys
    [[nodiscard]] bool isTrusted(const std::string& publisherId) const;

    /// @brief Get publisher info.
    ///
    /// @param publisherId Publisher identifier
    /// @return Publisher info, or nullopt if not found
    [[nodiscard]] std::optional<TrustedPublisher> getPublisher(
        const std::string& publisherId
    ) const;

    /// @brief Get all trusted publishers.
    ///
    /// @return Vector of all trusted publishers
    [[nodiscard]] std::vector<TrustedPublisher> getAllPublishers() const;

    /// @brief Add a user key.
    ///
    /// @param publisherId Publisher identifier (unique)
    /// @param name Display name
    /// @param publicKeyBase64 Base64-encoded 32-byte public key
    /// @return true if key was added successfully
    bool addUserKey(
        const std::string& publisherId,
        const std::string& name,
        const std::string& publicKeyBase64
    );

    /// @brief Remove a user key.
    ///
    /// Only user-added keys can be removed. Built-in keys cannot be removed.
    /// @param publisherId Publisher identifier
    /// @return true if key was removed
    bool removeUserKey(const std::string& publisherId);

    /// @brief Save user keys to settings.
    ///
    /// Persists user-added keys to the settings file.
    void saveUserKeys();

    /// @brief Clear all keys (for testing).
    void clear();

private:
    TrustedKeys() = default;

    /// @brief Decode base64 string to bytes.
    ///
    /// @param base64 Base64-encoded string
    /// @return Decoded bytes, or empty vector on error
    std::vector<uint8_t> base64Decode(const std::string& base64) const;

    std::map<std::string, TrustedPublisher> m_publishers;
    mutable std::mutex m_mutex;
};

} // namespace core
} // namespace kalahari
