// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file changed_setting.h
/// @brief A setting changed for one test, and set back when the test ends

#pragma once

#include "kalahari/core/settings_manager.h"

#include <cstdio>
#include <string>
#include <utility>

namespace kalahari::test {

/// @brief A setting with a value of the test; the value it had comes back when the test ends
template <typename T>
class ChangedSetting {
public:
    /// @brief Give setting @p key the value @p value
    ChangedSetting(std::string key, const T& value)
        : m_key(std::move(key)),
          m_saved(core::SettingsManager::getInstance().get<T>(m_key, value)) {
        core::SettingsManager::getInstance().set<T>(m_key, value);
    }
    ~ChangedSetting() {
        try {
            core::SettingsManager::getInstance().set<T>(m_key, m_saved);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            static_cast<void>(
                std::fputs("ChangedSetting: the setting of the test stays\n", stderr));
        }
    }
    ChangedSetting(const ChangedSetting&) = delete;
    ChangedSetting& operator=(const ChangedSetting&) = delete;
    ChangedSetting(ChangedSetting&&) = delete;
    ChangedSetting& operator=(ChangedSetting&&) = delete;

private:
    std::string m_key;
    T m_saved;
};

}  // namespace kalahari::test
