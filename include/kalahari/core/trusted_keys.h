// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file trusted_keys.h
/// @brief The keys of the publishers whose plugins the program loads: Kalahari's own
///
/// The program loads only Kalahari's own add-ons, signed with its key. The keys are built
/// into the program (trusted_keys.cpp): no file, setting or window adds another publisher.
///
/// @code
/// auto& keys = TrustedKeys::getInstance();
/// keys.loadBuiltinKeys();
/// if (auto key = keys.getPublicKey(signedBy)) {
///     // Verify the signature with *key
/// }
/// @endcode

#ifndef KALAHARI_CORE_TRUSTED_KEYS_H
#define KALAHARI_CORE_TRUSTED_KEYS_H

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace kalahari::core {

/// @brief A publisher whose plugins the program loads
struct TrustedPublisher {
    std::string id;                  ///< Its id in the signature files ("signed_by")
    std::string name;                ///< Its name for the writer
    std::vector<uint8_t> publicKey;  ///< 32-byte Ed25519 public key
};

/// @brief The keys of Kalahari's own publishers (singleton, thread-safe)
class TrustedKeys {
public:
    static TrustedKeys& getInstance();

    TrustedKeys(const TrustedKeys&) = delete;
    TrustedKeys& operator=(const TrustedKeys&) = delete;

    /// @brief Load the keys built into the program (at the start, before the plugins)
    /// @return The number of keys loaded; none while the program has no key of its own
    size_t loadBuiltinKeys();

    /// @brief The public key of the publisher, when the program trusts it
    [[nodiscard]] std::optional<std::vector<uint8_t>> getPublicKey(
        const std::string& publisherId) const;

    /// @brief Whether the program trusts the publisher
    [[nodiscard]] bool isTrusted(const std::string& publisherId) const;

    /// @brief The publisher, when the program trusts it
    [[nodiscard]] std::optional<TrustedPublisher> getPublisher(
        const std::string& publisherId) const;

    /// @brief Every publisher the program trusts
    [[nodiscard]] std::vector<TrustedPublisher> getAllPublishers() const;

    /// @brief Trust a key of the tests' own (FOR TESTING ONLY: the tests sign their plugins)
    void addTestKey(const std::string& publisherId, const std::vector<uint8_t>& publicKey);

    /// @brief Forget every key (FOR TESTING ONLY)
    void clear();

private:
    TrustedKeys() = default;

    std::map<std::string, TrustedPublisher> m_publishers;
    mutable std::mutex m_mutex;
};

} // namespace kalahari::core

#endif  // KALAHARI_CORE_TRUSTED_KEYS_H
