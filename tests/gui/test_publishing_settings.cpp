// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_publishing_settings.cpp
/// @brief The texts for publishers in the program: the names of their parts and states, the
/// length in standard pages and their group in Settings > Navigator

#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/settings/settings_pages.h"

#include <catch2/catch_test_macros.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QLocale>
#include <QSpinBox>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
using core::PublishingTexts;
using gui::PublishingNames;

namespace {

/// Numbers are written as in English (0.6) until the test ends; then as before
class EnglishNumbers {
public:
    EnglishNumbers() { QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); }
    ~EnglishNumbers() { QLocale::setDefault(m_saved); }
    EnglishNumbers(const EnglishNumbers&) = delete;
    EnglishNumbers& operator=(const EnglishNumbers&) = delete;
    EnglishNumbers(EnglishNumbers&&) = delete;
    EnglishNumbers& operator=(EnglishNumbers&&) = delete;

private:
    QLocale m_saved;
};

/// The settings of the texts for publishers as they were before a test, back after it; in the
/// test, the defaults and the cover's unit in characters
class KeptSettings {
public:
    KeptSettings() {
        for (const char* key : KEYS) {
            m_values.push_back(settings().get<nlohmann::json>(key, nlohmann::json()));
        }
        settings().set<std::string>(core::CoverMaterial::UNIT_SETTING, "characters");
        settings().set<nlohmann::json>(PublishingTexts::TARGETS_SETTING, nlohmann::json::object());
        settings().set<bool>(PublishingTexts::PAGES_SETTING, true);
        settings().set<nlohmann::json>(PublishingTexts::SET_SETTING,
                                       nlohmann::json{"pitch", "tagline", "blurb", "synopsis",
                                                      "aboutAuthor"});
    }
    ~KeptSettings() {
        try {
            for (std::size_t i = 0; i < KEYS.size(); ++i) {
                settings().set<nlohmann::json>(KEYS.at(i), m_values.at(i));
            }
        } catch (...) {
            static_cast<void>(std::fputs("KeptSettings: the settings of the test stay\n", stderr));
        }
    }
    KeptSettings(const KeptSettings&) = delete;
    KeptSettings& operator=(const KeptSettings&) = delete;
    KeptSettings(KeptSettings&&) = delete;
    KeptSettings& operator=(KeptSettings&&) = delete;

private:
    static constexpr std::array<const char*, 4> KEYS{
        core::CoverMaterial::UNIT_SETTING, PublishingTexts::TARGETS_SETTING,
        PublishingTexts::PAGES_SETTING, PublishingTexts::SET_SETTING};
    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }
    std::vector<nlohmann::json> m_values;
};

/// The child of @p page named @p name; the test stops without it
template <typename Widget>
Widget* childOf(const QWidget& page, const QString& name) {
    auto* child = page.findChild<Widget*>(name);
    REQUIRE(child != nullptr);
    return child;
}

}  // namespace

TEST_CASE("Texts for publishers: the names of their parts and states and the pages",
          "[gui][publishing]") {
    const EnglishNumbers numbers;
    CHECK(PublishingNames::partName(QStringLiteral("pitch")) ==
          QStringLiteral("Sentence about the book"));
    CHECK(PublishingNames::partName(QStringLiteral("synopsis")) == QStringLiteral("Synopsis"));
    CHECK(PublishingNames::partName(QStringLiteral("tagline")) == QStringLiteral("Tagline"));
    CHECK(PublishingNames::partName(QStringLiteral("blurb")) ==
          QStringLiteral("Cover description"));
    CHECK(PublishingNames::partName(QStringLiteral("aboutAuthor")) ==
          QStringLiteral("About the author"));
    // A part of the cover that is not a text for publishers, and an unknown one
    CHECK(PublishingNames::partName(QStringLiteral("opinion")).isEmpty());
    CHECK(PublishingNames::partName(QStringLiteral("letter")).isEmpty());
    // Every part of a text and of a set has a name
    for (const QString& part : PublishingTexts::setParts()) {
        CHECK_FALSE(PublishingNames::partName(part).isEmpty());
    }

    CHECK(PublishingNames::stateName(QString()) == QStringLiteral("draft"));
    CHECK(PublishingNames::stateName(QStringLiteral("chosen")) ==
          QStringLiteral("chosen for the set"));
    CHECK(PublishingNames::stateName(QStringLiteral("rejected")) == QStringLiteral("rejected"));
    // A state of a newer version, as it is written
    CHECK(PublishingNames::stateName(QStringLiteral("sent")) == QStringLiteral("sent"));

    CHECK(PublishingNames::pagesText(1080) == QStringLiteral("about 0.6 of a standard page"));
    CHECK(PublishingNames::pagesText(1800) == QStringLiteral("about 1.0 standard pages"));
    CHECK(PublishingNames::pagesText(4500) == QStringLiteral("about 2.5 standard pages"));
}

TEST_CASE("Texts for publishers: the targets and the pages in Settings > Navigator",
          "[gui][publishing][settings]") {
    const KeptSettings kept;
    gui::NavigatorPage page(kalahari::test::book(), gui::NavigatorPage::Part::Publishing);
    page.load();
    CHECK_FALSE(page.isChanged());

    auto* pitchCharacters =
        childOf<QSpinBox>(page, QStringLiteral("navigatorPublishingTarget_pitch_characters"));
    auto* pitchWords =
        childOf<QSpinBox>(page, QStringLiteral("navigatorPublishingTarget_pitch_words"));
    auto* synopsisCharacters =
        childOf<QSpinBox>(page, QStringLiteral("navigatorPublishingTarget_synopsis_characters"));
    auto* synopsisWords =
        childOf<QSpinBox>(page, QStringLiteral("navigatorPublishingTarget_synopsis_words"));
    auto* pages = childOf<QCheckBox>(page, QStringLiteral("navigatorPublishingPages"));

    // The defaults, both units side by side: the unit counted is chosen on the page Cover
    CHECK(pitchCharacters->value() == 200);
    CHECK(synopsisCharacters->value() == 1800);
    CHECK(pitchWords->value() == 35);
    CHECK(synopsisWords->value() == 300);
    CHECK(pitchCharacters->isVisibleTo(&page));
    CHECK(pitchWords->isVisibleTo(&page));
    CHECK(pitchCharacters->accessibleName() ==
          QStringLiteral("Sentence about the book, characters"));
    CHECK(pitchWords->accessibleName() == QStringLiteral("Sentence about the book, words"));
    CHECK(pages->isChecked());

    synopsisWords->setValue(450);
    pitchCharacters->setValue(250);
    pages->setChecked(false);
    CHECK(page.isChanged());
    page.apply();
    CHECK(PublishingTexts::defaultTargetOf(QStringLiteral("synopsis"), true) == 450);
    CHECK(PublishingTexts::defaultTargetOf(QStringLiteral("pitch"), false) == 250);
    CHECK(PublishingTexts::defaultTargetOf(QStringLiteral("pitch"), true) == 35);
    CHECK_FALSE(PublishingTexts::showsPages());

    // Loaded again: as applied
    gui::NavigatorPage again(kalahari::test::book(), gui::NavigatorPage::Part::Publishing);
    again.load();
    CHECK_FALSE(again.isChanged());
    CHECK(childOf<QSpinBox>(again, QStringLiteral("navigatorPublishingTarget_synopsis_words"))
              ->value() == 450);
    CHECK_FALSE(childOf<QCheckBox>(again, QStringLiteral("navigatorPublishingPages"))->isChecked());
}

TEST_CASE("Texts for publishers: the set of a new book in Settings > Navigator",
          "[gui][publishing][settings]") {
    const KeptSettings kept;
    auto& settings = core::SettingsManager::getInstance();
    // A set kept in another order than that of the boxes is not a change
    settings.set<nlohmann::json>(PublishingTexts::SET_SETTING,
                                 nlohmann::json{"synopsis", "pitch"});
    gui::NavigatorPage page(kalahari::test::book(), gui::NavigatorPage::Part::Publishing);
    page.load();
    CHECK_FALSE(page.isChanged());


    QList<QCheckBox*> boxes;
    for (const QString& part : PublishingTexts::setParts()) {
        boxes << childOf<QCheckBox>(page, QStringLiteral("navigatorPublishingSet_") + part);
        CHECK(boxes.last()->text() == PublishingNames::partName(part));
    }
    const auto boxOf = [&page](const char* part) {
        return childOf<QCheckBox>(page, QStringLiteral("navigatorPublishingSet_") +
                                            QLatin1String(part));
    };
    CHECK(boxOf("pitch")->isChecked());
    CHECK(boxOf("synopsis")->isChecked());
    CHECK_FALSE(boxOf("tagline")->isChecked());
    CHECK_FALSE(boxOf("blurb")->isChecked());
    CHECK_FALSE(boxOf("aboutAuthor")->isChecked());

    // The set with the tagline, without the sentence
    boxOf("tagline")->setChecked(true);
    boxOf("pitch")->setChecked(false);
    CHECK(page.isChanged());
    page.apply();
    const auto stored = settings.get<nlohmann::json>(PublishingTexts::SET_SETTING);
    CHECK(stored == (nlohmann::json{"tagline", "synopsis"}));
    const core::PublishingSet defaults = core::PublishingSet::defaults();
    CHECK(defaults.includes(QStringLiteral("tagline")));
    CHECK(defaults.includes(QStringLiteral("synopsis")));
    CHECK_FALSE(defaults.includes(QStringLiteral("pitch")));
}
