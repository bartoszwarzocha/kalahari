/// @file test_trusted_keys.cpp
/// @brief Unit tests for TrustedKeys singleton (Sub-Project C WS4.3)
///
/// TrustedKeys manages Ed25519 publisher keys for plugin verification. These
/// tests exercise the public API (add/remove user keys, lookups, built-in
/// immutability, base64 validation) using clear() for isolation.

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/trusted_keys.h>

#include <QByteArray>

#include <string>
#include <vector>

using namespace kalahari::core;

namespace {

/// @brief Produce a valid base64-encoded 32-byte key from a seed byte.
///
/// Ed25519 public keys are exactly 32 bytes; addUserKey requires the decoded
/// length to be 32. We build a deterministic 32-byte buffer and base64-encode
/// it with Qt so the test does not depend on any external fixture.
std::string makeValidKeyBase64(char seed = 0x01)
{
    QByteArray raw(32, seed);
    return raw.toBase64().toStdString();
}

}  // namespace

TEST_CASE("TrustedKeys singleton identity", "[core][security][trusted_keys]") {
    TrustedKeys::getInstance().clear();
    TrustedKeys& a = TrustedKeys::getInstance();
    TrustedKeys& b = TrustedKeys::getInstance();
    REQUIRE(&a == &b);
}

TEST_CASE("TrustedKeys clear empties the registry", "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();
    keys.addUserKey("some-id", "Some Name", makeValidKeyBase64());
    REQUIRE_FALSE(keys.getAllPublishers().empty());

    keys.clear();
    REQUIRE(keys.getAllPublishers().empty());
    REQUIRE_FALSE(keys.isTrusted("some-id"));
}

TEST_CASE("TrustedKeys addUserKey with a valid key", "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();

    REQUIRE(keys.addUserKey("pub-1", "Publisher One", makeValidKeyBase64(0x2A)));
    REQUIRE(keys.isTrusted("pub-1"));

    auto pubKey = keys.getPublicKey("pub-1");
    REQUIRE(pubKey.has_value());
    REQUIRE(pubKey->size() == 32);

    auto publisher = keys.getPublisher("pub-1");
    REQUIRE(publisher.has_value());
    REQUIRE(publisher->id == "pub-1");
    REQUIRE(publisher->name == "Publisher One");
    REQUIRE(publisher->trustLevel == TrustLevel::User);
}

TEST_CASE("TrustedKeys rejects invalid key material", "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();

    SECTION("Invalid base64 is rejected") {
        REQUIRE_FALSE(keys.addUserKey("bad", "Bad", "!!!not-valid-base64!!!"));
        REQUIRE_FALSE(keys.isTrusted("bad"));
    }

    SECTION("Wrong decoded length (not 32 bytes) is rejected") {
        // 16 bytes -> valid base64 but wrong length for an Ed25519 key
        std::string shortKey = QByteArray(16, 0x05).toBase64().toStdString();
        REQUIRE_FALSE(keys.addUserKey("short", "Short", shortKey));
        REQUIRE_FALSE(keys.isTrusted("short"));
    }

    SECTION("Empty base64 is rejected") {
        REQUIRE_FALSE(keys.addUserKey("empty", "Empty", ""));
        REQUIRE_FALSE(keys.isTrusted("empty"));
    }
}

TEST_CASE("TrustedKeys addUserKey duplicate id replaces (actual behavior)", "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();

    REQUIRE(keys.addUserKey("dup", "First", makeValidKeyBase64(0x11)));
    // A duplicate user id is allowed and REPLACES the existing user key
    // (built-in guard only blocks non-User trust levels).
    REQUIRE(keys.addUserKey("dup", "Second", makeValidKeyBase64(0x22)));

    auto publisher = keys.getPublisher("dup");
    REQUIRE(publisher.has_value());
    REQUIRE(publisher->name == "Second");
    REQUIRE(keys.getAllPublishers().size() == 1);
}

TEST_CASE("TrustedKeys removeUserKey", "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();

    SECTION("Removing an existing user key succeeds") {
        keys.addUserKey("removable", "Removable", makeValidKeyBase64());
        REQUIRE(keys.removeUserKey("removable"));
        REQUIRE_FALSE(keys.isTrusted("removable"));
    }

    SECTION("Removing an unknown key fails") {
        REQUIRE_FALSE(keys.removeUserKey("does-not-exist"));
    }
}

TEST_CASE("TrustedKeys lookups for unknown publishers", "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();

    REQUIRE_FALSE(keys.getPublicKey("nobody").has_value());
    REQUIRE_FALSE(keys.getPublisher("nobody").has_value());
    REQUIRE_FALSE(keys.isTrusted("nobody"));
}

TEST_CASE("TrustedKeys getAllPublishers reflects the number added", "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();

    keys.addUserKey("a", "A", makeValidKeyBase64(0x01));
    keys.addUserKey("b", "B", makeValidKeyBase64(0x02));
    keys.addUserKey("c", "C", makeValidKeyBase64(0x03));

    REQUIRE(keys.getAllPublishers().size() == 3);
}

TEST_CASE("trustLevelToString produces distinct, non-empty strings", "[core][security][trusted_keys]") {
    const std::string full = trustLevelToString(TrustLevel::Full);
    const std::string verified = trustLevelToString(TrustLevel::Verified);
    const std::string user = trustLevelToString(TrustLevel::User);

    REQUIRE_FALSE(full.empty());
    REQUIRE_FALSE(verified.empty());
    REQUIRE_FALSE(user.empty());

    REQUIRE(full != verified);
    REQUIRE(verified != user);
    REQUIRE(full != user);
}

TEST_CASE("TrustedKeys loadBuiltinKeys and built-in immutability", "[core][security][trusted_keys]") {
    auto& keys = TrustedKeys::getInstance();
    keys.clear();

    // Resource-path dependency: loadBuiltinKeys() resolves
    // resources/keys/trusted_publishers.json relative to the test CWD. It may
    // not be resolvable on every runner, so guard the assertions.
    const bool loaded = keys.loadBuiltinKeys();

    if (!loaded) {
        WARN("trusted_publishers.json not resolvable from test CWD; "
             "skipping built-in key assertions");
        SUCCEED("Built-in keys not available in this environment");
        return;
    }

    auto publishers = keys.getAllPublishers();
    REQUIRE_FALSE(publishers.empty());

    // Built-in (non-User) keys cannot be removed. Find one to verify immutability.
    bool checkedImmutability = false;
    for (const auto& pub : publishers) {
        if (pub.trustLevel != TrustLevel::User) {
            REQUIRE_FALSE(keys.removeUserKey(pub.id));
            REQUIRE(keys.isTrusted(pub.id));  // still present
            checkedImmutability = true;
            break;
        }
    }

    if (!checkedImmutability) {
        WARN("No non-User built-in keys present to verify immutability");
    }
}

TEST_CASE("TrustedKeys save/load round-trip preserves key bytes", "[core][security][trusted_keys]") {
    // Regression guard for the base64EncodeInternal() buffer-size fix: addUserKey()
    // persists via saveUserKeys() -> base64EncodeInternal() (the path that used to
    // under-allocate its output buffer for a 32-byte key). Reloading from disk and
    // comparing the exact bytes locks in encoder+decoder correctness even in Debug
    // builds without ASan, where a 1-byte overflow would not reliably crash.
    auto& keys = TrustedKeys::getInstance();
    keys.clear();

    QByteArray raw(32, 0x3C);
    const std::string keyB64 = raw.toBase64().toStdString();

    REQUIRE(keys.addUserKey("roundtrip", "Round Trip", keyB64));

    // Decoded key held in memory (before any save/reload cycle).
    auto original = keys.getPublicKey("roundtrip");
    REQUIRE(original.has_value());
    REQUIRE(original->size() == 32);

    // Drop the in-memory copy and reload from the persisted user-keys file.
    keys.clear();
    REQUIRE_FALSE(keys.isTrusted("roundtrip"));
    keys.loadUserKeys();

    auto reloaded = keys.getPublicKey("roundtrip");
    REQUIRE(reloaded.has_value());
    REQUIRE(reloaded->size() == 32);

    // Byte-for-byte identity: the 32 bytes survived encode -> persist -> decode.
    REQUIRE(*reloaded == *original);
}
