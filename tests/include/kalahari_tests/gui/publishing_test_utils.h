// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file publishing_test_utils.h
/// @brief Texts for publishers and materials of the cover of the open book, and the settings
///        of the lists kept around a test

#ifndef KALAHARI_TESTS_GUI_PUBLISHING_TEST_UTILS_H
#define KALAHARI_TESTS_GUI_PUBLISHING_TEST_UTILS_H

#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/material_list.h"

#include <catch2/catch_test_macros.hpp>

#include <QString>

#include <nlohmann/json.hpp>

#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace kalahari::test {

/// A text for publishers of the open book: @p part with @p title and @p text
inline QString addPublishingText(const char* part, const QString& title, const QString& text,
                                 const QString& templateReference = QString()) {
    auto& pm = book();
    const QString id =
        pm.addPublishingText(QString::fromLatin1(part), templateReference, title);
    REQUIRE_FALSE(id.isEmpty());
    pm.setChapterContent(id, QStringLiteral("<kml><p>%1</p></kml>").arg(text));
    REQUIRE(pm.saveChapterContent(id));
    return id;
}

/// A material of the cover of the open book: @p part with @p text
inline QString addCoverText(const QString& title, const char* part, const QString& text) {
    auto& pm = book();
    core::NewMaterial material;
    material.title = title;
    material.cover.part = QString::fromLatin1(part);
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("cover"));
    const QString id = pm.addMaterial(material, kind);
    REQUIRE_FALSE(id.isEmpty());
    REQUIRE(pm.setDescription(id, text));
    return id;
}

/// The settings of the lists, of the unit of the lengths, of their targets, of the set and of
/// the writer's templates as they were before a test, back after it; during it no kind is
/// hidden, the lists keep nothing, the lengths are in characters and the writer has no
/// templates
class PublishingSettings {
public:
    PublishingSettings() {
        for (const char* key : KEYS) {
            m_values.push_back(settings().get<nlohmann::json>(key, nlohmann::json()));
        }
        settings().set<nlohmann::json>(gui::MaterialList::HIDDEN_SETTING,
                                       nlohmann::json::array());
        settings().set<nlohmann::json>(gui::MaterialList::LISTS_SETTING,
                                       nlohmann::json::object());
        settings().set<std::string>(core::CoverMaterial::UNIT_SETTING, "characters");
        settings().set<nlohmann::json>(core::PublishingTexts::TEMPLATES_SETTING,
                                       nlohmann::json::array());
    }
    ~PublishingSettings() {
        try {
            for (std::size_t i = 0; i < KEYS.size(); ++i) {
                settings().set<nlohmann::json>(KEYS.at(i), m_values.at(i));
            }
        } catch (...) {
            (void)std::fputs("PublishingSettings: the settings of the test stay\n", stderr);
        }
    }
    PublishingSettings(const PublishingSettings&) = delete;
    PublishingSettings& operator=(const PublishingSettings&) = delete;
    PublishingSettings(PublishingSettings&&) = delete;
    PublishingSettings& operator=(PublishingSettings&&) = delete;

private:
    static constexpr std::array<const char*, 6> KEYS{
        gui::MaterialList::HIDDEN_SETTING, core::CoverMaterial::UNIT_SETTING,
        gui::MaterialList::LISTS_SETTING, core::PublishingTexts::TARGETS_SETTING,
        core::PublishingTexts::SET_SETTING, core::PublishingTexts::TEMPLATES_SETTING};
    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }
    std::vector<nlohmann::json> m_values;
};

}  // namespace kalahari::test

#endif  // KALAHARI_TESTS_GUI_PUBLISHING_TEST_UTILS_H
