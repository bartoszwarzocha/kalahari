// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file trusted_keys.cpp
/// @brief The keys of the publishers whose plugins the program loads: Kalahari's own

#include <kalahari/core/trusted_keys.h>
#include <kalahari/core/logger.h>

#include <openssl/evp.h>

#include <array>

namespace kalahari {
namespace core {

namespace {

/// @brief A key built into the program
struct BuiltinKey {
    const char* id;
    const char* name;
    const char* publicKeyBase64;  ///< Empty: the key is not made yet
};

/// @brief Kalahari's own keys: the plugins signed with them are the only ones the program loads
///
/// The key of the add-ons is not made yet: its private half stays with the publisher, its
/// public half goes here (base64 of the 32 bytes). Until then no plugin is trusted. A
/// placeholder of zero bytes would be worse than none: a key of a small order lets anyone
/// make a signature that passes.
constexpr std::array<BuiltinKey, 1> BUILTIN_KEYS{{
    {"kalahari", "Kalahari", ""},
}};

/// @brief The 32 bytes of a base64 key; empty when it is not one
std::vector<uint8_t> decodeKey(const std::string& base64) {
    constexpr size_t KEY_SIZE = 32;
    constexpr size_t KEY_BASE64_SIZE = 44;  // 32 bytes: 43 characters and one '='
    if (base64.size() != KEY_BASE64_SIZE) {
        return {};
    }
    std::vector<uint8_t> decoded(KEY_BASE64_SIZE / 4 * 3);
    const int written =
        EVP_DecodeBlock(decoded.data(), reinterpret_cast<const unsigned char*>(base64.data()),
                        static_cast<int>(base64.size()));
    // EVP_DecodeBlock counts the padding as zero bytes: one '=' is 32 bytes, two are 31
    if (written != static_cast<int>(decoded.size()) || base64[KEY_BASE64_SIZE - 1] != '=' ||
        base64[KEY_BASE64_SIZE - 2] == '=') {
        return {};
    }
    decoded.resize(KEY_SIZE);
    return decoded;
}

}  // namespace

TrustedKeys& TrustedKeys::getInstance() {
    static TrustedKeys instance;
    return instance;
}

size_t TrustedKeys::loadBuiltinKeys() {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t loaded = 0;
    for (const BuiltinKey& key : BUILTIN_KEYS) {
        std::vector<uint8_t> publicKey = decodeKey(key.publicKeyBase64);
        if (publicKey.empty()) {
            Logger::getInstance().warn("TrustedKeys: The key '{}' is not made yet", key.id);
            continue;
        }
        m_publishers[key.id] = TrustedPublisher{key.id, key.name, std::move(publicKey)};
        ++loaded;
    }
    Logger::getInstance().info("TrustedKeys: {} built-in publisher keys", loaded);
    return loaded;
}

std::optional<std::vector<uint8_t>> TrustedKeys::getPublicKey(
    const std::string& publisherId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (const auto it = m_publishers.find(publisherId); it != m_publishers.end()) {
        return it->second.publicKey;
    }
    return std::nullopt;
}

bool TrustedKeys::isTrusted(const std::string& publisherId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_publishers.count(publisherId) > 0;
}

std::optional<TrustedPublisher> TrustedKeys::getPublisher(const std::string& publisherId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (const auto it = m_publishers.find(publisherId); it != m_publishers.end()) {
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

void TrustedKeys::addTestKey(const std::string& publisherId,
                             const std::vector<uint8_t>& publicKey) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_publishers[publisherId] = TrustedPublisher{publisherId, publisherId, publicKey};
}

void TrustedKeys::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_publishers.clear();
}

} // namespace core
} // namespace kalahari
