// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_trusted_keys.cpp
/// @brief Tests of TrustedKeys: the program trusts only the keys built into it

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/trusted_keys.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace kalahari::core;

namespace {

std::vector<uint8_t> keyOf(uint8_t seed) {
    return std::vector<uint8_t>(32, seed);
}

}  // namespace

TEST_CASE("TrustedKeys singleton identity", "[core][security][trusted_keys]") {
    TrustedKeys& a = TrustedKeys::getInstance();
    TrustedKeys& b = TrustedKeys::getInstance();
    REQUIRE(&a == &b);
}

TEST_CASE("TrustedKeys: a key of the tests is trusted until it is cleared",
          "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();
    keys.addTestKey("pub-1", keyOf(0x2A));

    REQUIRE(keys.isTrusted("pub-1"));
    const auto publicKey = keys.getPublicKey("pub-1");
    REQUIRE(publicKey.has_value());
    CHECK(*publicKey == keyOf(0x2A));
    const auto publisher = keys.getPublisher("pub-1");
    REQUIRE(publisher.has_value());
    CHECK(publisher->id == "pub-1");
    CHECK(keys.getAllPublishers().size() == 1);

    // The same id again replaces the key
    keys.addTestKey("pub-1", keyOf(0x11));
    CHECK(keys.getPublicKey("pub-1") == keyOf(0x11));
    CHECK(keys.getAllPublishers().size() == 1);

    keys.clear();
    CHECK(keys.getAllPublishers().empty());
    CHECK_FALSE(keys.isTrusted("pub-1"));
}

TEST_CASE("TrustedKeys lookups for unknown publishers", "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();

    CHECK_FALSE(keys.getPublicKey("nobody").has_value());
    CHECK_FALSE(keys.getPublisher("nobody").has_value());
    CHECK_FALSE(keys.isTrusted("nobody"));
}

TEST_CASE("TrustedKeys: only the keys built into the program are loaded",
          "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();
    // The settings of the earlier versions do not add a publisher any more
    SettingsManager::getInstance().set<std::string>(
        "plugins.trustedKeys",
        nlohmann::json::array({{{"id", "someone"},
                                {"name", "Someone"},
                                {"publicKey", "PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw="}}})
            .dump());

    const size_t loaded = keys.loadBuiltinKeys();

    CHECK_FALSE(keys.isTrusted("someone"));
    CHECK(keys.getAllPublishers().size() == loaded);
    for (const TrustedPublisher& publisher : keys.getAllPublishers()) {
        CHECK(publisher.publicKey.size() == 32);
        // A key of zero bytes would let anyone make a signature that passes
        CHECK(publisher.publicKey != keyOf(0));
    }
}
