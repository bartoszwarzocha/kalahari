// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plugin_signing.h
/// @brief The tests' own publisher key: the program loads only signed plugins
///
/// The tests make an Ed25519 key once per run, trust it (core::TrustedKeys::addTestKey())
/// and sign the plugin files they load with it, as Kalahari signs its add-ons.

#pragma once

#include <filesystem>
#include <string>

namespace kalahari::test {

/// @brief The publisher id of the tests' key ("signed_by" of their signatures)
inline constexpr const char* TEST_PUBLISHER = "kalahari-test";

/// @brief Trust the tests' key (again after resetSingletons() forgot it)
void trustTestKey();

/// @brief Sign the plugin file @p kplugin with the tests' key: writes <file>.sig, and trusts
///        the key
void signPlugin(const std::filesystem::path& kplugin);

/// @brief Sign the plugin file @p kplugin with a key the program does not trust
void signPluginWithStrangerKey(const std::filesystem::path& kplugin);

}  // namespace kalahari::test
