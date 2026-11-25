// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file plugin_signing.cpp
/// @brief The tests' own publisher key: the program loads only signed plugins

#include "plugin_signing.h"

#include <kalahari/core/plugin_signature.h>
#include <kalahari/core/trusted_keys.h>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace kalahari::test {

namespace {

using KeyPointer = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;

KeyPointer makeKey() {
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> context(
        EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr), &EVP_PKEY_CTX_free);
    EVP_PKEY* key = nullptr;
    if (!context || EVP_PKEY_keygen_init(context.get()) <= 0 ||
        EVP_PKEY_keygen(context.get(), &key) <= 0) {
        throw std::runtime_error("The tests' Ed25519 key was not made");
    }
    return {key, &EVP_PKEY_free};
}

/// The key the program trusts in the tests, made once
EVP_PKEY* testKey() {
    static const KeyPointer key = makeKey();
    return key.get();
}

/// A key the program does not trust, made once
EVP_PKEY* strangerKey() {
    static const KeyPointer key = makeKey();
    return key.get();
}

std::vector<uint8_t> publicKeyOf(EVP_PKEY* key) {
    std::vector<uint8_t> bytes(32);
    size_t size = bytes.size();
    if (EVP_PKEY_get_raw_public_key(key, bytes.data(), &size) <= 0) {
        throw std::runtime_error("The public half of the tests' key was not read");
    }
    bytes.resize(size);
    return bytes;
}

std::string base64Of(const std::vector<uint8_t>& bytes) {
    std::string text(((bytes.size() + 2) / 3) * 4 + 1, '\0');
    const int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(text.data()),
                                        bytes.data(), static_cast<int>(bytes.size()));
    text.resize(static_cast<size_t>(written));
    return text;
}

void signWith(EVP_PKEY* key, const std::string& publisher, const std::filesystem::path& kplugin) {
    const std::string hash = core::PluginSignature::computeArchiveHash(kplugin);
    if (hash.empty()) {
        throw std::runtime_error("The plugin to sign has no hash: " + kplugin.string());
    }
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(),
                                                                    &EVP_MD_CTX_free);
    std::vector<uint8_t> signature(64);
    size_t size = signature.size();
    if (!context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key) <= 0 ||
        EVP_DigestSign(context.get(), signature.data(), &size,
                       reinterpret_cast<const unsigned char*>(hash.data()), hash.size()) <= 0) {
        throw std::runtime_error("The plugin was not signed: " + kplugin.string());
    }
    signature.resize(size);

    const nlohmann::json file = {{"version", 1},
                                 {"algorithm", "ed25519"},
                                 {"signed_by", publisher},
                                 {"archive_hash", hash},
                                 {"signature", base64Of(signature)}};
    std::filesystem::path sigPath = kplugin;
    sigPath += ".sig";
    std::ofstream(sigPath, std::ios::binary | std::ios::trunc) << file.dump(2);
}

}  // namespace

void trustTestKey() {
    core::TrustedKeys::getInstance().addTestKey(TEST_PUBLISHER, publicKeyOf(testKey()));
}

void signPlugin(const std::filesystem::path& kplugin) {
    trustTestKey();
    signWith(testKey(), TEST_PUBLISHER, kplugin);
}

void signPluginWithStrangerKey(const std::filesystem::path& kplugin) {
    signWith(strangerKey(), "stranger", kplugin);
}

}  // namespace kalahari::test
