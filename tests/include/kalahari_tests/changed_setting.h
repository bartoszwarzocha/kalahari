// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file changed_setting.h
/// @brief A setting kept or changed for one test, and set back when the test ends

#ifndef KALAHARI_TESTS_CHANGED_SETTING_H
#define KALAHARI_TESTS_CHANGED_SETTING_H

#include "kalahari/core/settings_manager.h"

#include <cstdio>
#include <string>
#include <utility>

namespace kalahari::test {

/// @brief A setting the test may change; the value it had comes back when the test ends
template <typename T>
class KeptSetting {
public:
    /// @brief Keep the value of setting @p key (@p fallback when it has none)
    explicit KeptSetting(std::string key, const T& fallback = T())
        : m_key(std::move(key)),
          m_saved(core::SettingsManager::getInstance().get<T>(m_key, fallback)) {}
    ~KeptSetting() {
        try {
            core::SettingsManager::getInstance().set<T>(m_key, m_saved);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            static_cast<void>(
                std::fputs("KeptSetting: the setting of the test stays\n", stderr));
        }
    }
    KeptSetting(const KeptSetting&) = delete;
    KeptSetting& operator=(const KeptSetting&) = delete;
    KeptSetting(KeptSetting&&) = delete;
    KeptSetting& operator=(KeptSetting&&) = delete;

    /// @brief The key of the setting
    [[nodiscard]] const std::string& key() const { return m_key; }

private:
    std::string m_key;
    T m_saved;
};

/// @brief A setting with a value of the test; the value it had comes back when the test ends
template <typename T>
class ChangedSetting : public KeptSetting<T> {
public:
    /// @brief Give setting @p key the value @p value
    ChangedSetting(std::string key, const T& value) : KeptSetting<T>(std::move(key), value) {
        core::SettingsManager::getInstance().set<T>(this->key(), value);
    }
};

}  // namespace kalahari::test

#endif  // KALAHARI_TESTS_CHANGED_SETTING_H
