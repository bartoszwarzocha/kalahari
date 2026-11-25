// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_publishing_card.cpp
/// @brief The card For Publishers: the tiles of the set, the versions of the part chosen and
///        the text of a version written in the card, saved at once

#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/publishing_card_panel.h"
#include "kalahari/gui/panels/publishing_text_box.h"
#include "kalahari/gui/widgets/length_meter.h"

#include <catch2/catch_test_macros.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QApplication>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QToolButton>

#include <cstdio>
#include <string>

namespace core = kalahari::core;
namespace editor = kalahari::editor;
namespace gui = kalahari::gui;
namespace test = kalahari::test;

using core::PublishingTexts;
using gui::PublishingCardPanel;

namespace {

/// Type @p text in @p field as the writer does: the field gets it and says it was edited
void type(QLineEdit* field, const QString& text) {
    REQUIRE(field != nullptr);
    field->setText(text);
    emit field->textEdited(text);
}

/// Press and release @p key with @p modifiers on @p widget
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    REQUIRE(widget != nullptr);
    QKeyEvent down(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &down);
    QKeyEvent up(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &up);
}

/// The text of the version shown on @p card
std::string textOf(const PublishingCardPanel& card) {
    return card.getBookEditor()->plainText().trimmed().toStdString();
}

/// The unit of the lengths in characters during a test, as it was after it
class CharactersUnit {
public:
    CharactersUnit()
        : m_unit(settings().get<std::string>(core::CoverMaterial::UNIT_SETTING, "characters")) {
        settings().set<std::string>(core::CoverMaterial::UNIT_SETTING, "characters");
    }
    ~CharactersUnit() {
        try {
            settings().set<std::string>(core::CoverMaterial::UNIT_SETTING, m_unit);
        } catch (...) {
            (void)std::fputs("CharactersUnit: the unit of the test stays\n", stderr);
        }
    }
    CharactersUnit(const CharactersUnit&) = delete;
    CharactersUnit& operator=(const CharactersUnit&) = delete;
    CharactersUnit(CharactersUnit&&) = delete;
    CharactersUnit& operator=(CharactersUnit&&) = delete;

private:
    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }
    std::string m_unit;
};

}  // namespace

TEST_CASE("For Publishers: a sentence is written in the card with the fields of its pattern",
          "[gui][publishing][publishingcard]") {
    gui::registerAllIcons();
    const CharactersUnit unit;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString id = pm.addPublishingText(QString::fromLatin1(PublishingTexts::PITCH),
                                            QStringLiteral("kalahari.novel:when_must"),
                                            QStringLiteral("Sentence – version 1"));
    REQUIRE_FALSE(id.isEmpty());
    test::Window window;
    // A text for publishers opened anywhere opens on the card
    window.navigator.onElementSelected(id, QStringLiteral("Sentence – version 1"));
    auto* card = qobject_cast<PublishingCardPanel*>(window.tabs->currentWidget());
    REQUIRE(card != nullptr);
    CHECK(window.tabs->tabText(window.tabs->indexOf(card)) == QStringLiteral("For Publishers"));
    CHECK(card->part() == PublishingTexts::PITCH);
    CHECK(card->versionId() == id);
    CHECK(card->property("elementId").toString() == id);
    CHECK(card->textBox()->templateBox()->currentData().toString() ==
          QStringLiteral("kalahari.novel:when_must"));
    // An empty sentence; the fields of the pattern
    CHECK(textOf(*card).empty());
    REQUIRE(card->field(QStringLiteral("event")) != nullptr);
    CHECK(card->field(QStringLiteral("event"))->accessibleName() == QStringLiteral("what happens"));
    CHECK(card->field(QStringLiteral("nothing")) == nullptr);

    // The fields compose the sentence at once; what is missing is "…"
    type(card->field(QStringLiteral("event")), QStringLiteral("the well runs dry"));
    CHECK(textOf(*card) == "When the well runs dry, … must … before ….");
    type(card->field(QStringLiteral("hero")), QStringLiteral("Leila"));
    type(card->field(QStringLiteral("goal")), QStringLiteral("find water"));
    type(card->field(QStringLiteral("stakes")), QStringLiteral("the caravan dies"));
    const std::string sentence =
        "When the well runs dry, Leila must find water before the caravan dies.";
    CHECK(textOf(*card) == sentence);
    CHECK(card->textBox()->meter()->targetLabel()->text() ==
          QStringLiteral("target: up to 200 characters"));
    // Saved at once: the fields and the sentence; the tab shows no "*"
    card->saveNow();
    CHECK(pm.findElement(id)->publishing.fields.value(QStringLiteral("hero")) ==
          QStringLiteral("Leila"));
    CHECK(pm.savedChapterText(id).contains(QStringLiteral("Leila must find water")));
    CHECK_FALSE(card->isModified());
    CHECK(window.tabs->tabText(window.tabs->indexOf(card)) == QStringLiteral("For Publishers"));
    CHECK(card->savedLabel()->text().contains(QStringLiteral("saved at")));

    // Changed by hand: the fields no longer change it, until it is composed again
    card->getBookEditor()->replaceWithKml(
        editor::ClipboardHandler::textToKml(QStringLiteral("Leila looks for the last well.")));
    CHECK(pm.findElement(id)->publishing.manual);
    CHECK(card->composeButton()->isVisibleTo(card));
    type(card->field(QStringLiteral("hero")), QStringLiteral("Young Leila"));
    CHECK(textOf(*card) == "Leila looks for the last well.");
    card->composeButton()->click();
    CHECK_FALSE(pm.findElement(id)->publishing.manual);
    CHECK(textOf(*card) ==
          "When the well runs dry, Young Leila must find water before the caravan dies.");

    // Another template beside the text: the fields of the same names keep their values
    QComboBox* templates = card->textBox()->templateBox();
    const int heroGoal = templates->findData(QStringLiteral("kalahari.novel:hero_goal"));
    REQUIRE(heroGoal >= 0);
    templates->setCurrentIndex(heroGoal);
    emit templates->activated(heroGoal);
    CHECK(pm.findElement(id)->publishing.templateId == QStringLiteral("kalahari.novel:hero_goal"));
    REQUIRE(card->field(QStringLiteral("who")) != nullptr);
    CHECK(card->field(QStringLiteral("hero"))->text() == QStringLiteral("Young Leila"));
    CHECK(card->field(QStringLiteral("event")) == nullptr);
}

TEST_CASE("For Publishers: the tiles of the set, the versions and a synopsis saved at once",
          "[gui][publishing][publishingcard]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString first = pm.addPublishingText(QString::fromLatin1(PublishingTexts::SYNOPSIS),
                                               QString(), QStringLiteral("Synopsis – short"));
    const QString second = pm.addPublishingText(QString::fromLatin1(PublishingTexts::SYNOPSIS),
                                                QString(), QStringLiteral("Synopsis – long"));
    REQUIRE_FALSE(second.isEmpty());
    REQUIRE(pm.setPublishingState(second, QString::fromLatin1(PublishingTexts::CHOSEN)));
    test::Window window;
    window.window.show();
    PublishingCardPanel* card = window.navigator.openPublishingCard(QString(), true);
    REQUIRE(card != nullptr);
    // One card: opened again, the same tab
    CHECK(window.navigator.openPublishingCard(QString(), false) == card);

    // A tile for each part of the set, in its order
    CHECK(card->tileParts() == pm.publishingSet().parts());
    const QString synopsis = QString::fromLatin1(PublishingTexts::SYNOPSIS);
    card->showPart(synopsis, true);
    QFrame* tile = card->tile(synopsis);
    REQUIRE(tile != nullptr);
    // The version in the set is shown, with a star
    CHECK(card->versionId() == second);
    QListWidget* versions = card->versionList();
    REQUIRE(versions->count() == 2);
    CHECK(versions->item(1)->text() == QStringLiteral("Synopsis – long ★"));
    CHECK(versions->item(0)->text() == QStringLiteral("Synopsis – short"));

    // Space on the tile leaves the part out of the set and puts it back
    const bool on = pm.publishingSet().includes(synopsis);
    press(tile, Qt::Key_Space);
    CHECK(pm.publishingSet().includes(synopsis) != on);
    CHECK(card->tileCheck(synopsis)->isChecked() != on);
    press(card->tile(synopsis), Qt::Key_Space);
    CHECK(pm.publishingSet().includes(synopsis) == on);
    // Alt+Left moves it earlier
    const qsizetype place = pm.publishingSet().parts().indexOf(synopsis);
    REQUIRE(place > 0);
    press(card->tile(synopsis), Qt::Key_Left, Qt::AltModifier);
    CHECK(pm.publishingSet().parts().indexOf(synopsis) == place - 1);
    CHECK(card->tileParts() == pm.publishingSet().parts());
    // The arrows choose another part
    press(card->tile(synopsis), Qt::Key_Right);
    CHECK(card->part() == card->tileParts().value(place));
    card->showPart(synopsis, false);

    // Another version: Space puts it in the set in place of the one there
    versions->setCurrentRow(0);
    CHECK(card->versionId() == first);
    press(versions, Qt::Key_Space);
    CHECK(pm.findElement(first)->publishing.state == PublishingTexts::CHOSEN);
    CHECK(pm.findElement(second)->publishing.state.isEmpty());
    CHECK(card->versionList()->item(0)->text() == QStringLiteral("Synopsis – short ★"));

    // The text is written in the card and saved at once
    card->getBookEditor()->replaceWithKml(
        editor::ClipboardHandler::textToKml(QStringLiteral("Leila crosses the desert.")));
    CHECK(card->isModified());
    card->saveNow();
    CHECK_FALSE(card->isModified());
    CHECK(pm.savedChapterText(first).contains(QStringLiteral("Leila crosses the desert.")));
    CHECK(window.tabs->tabText(window.tabs->indexOf(card)) == QStringLiteral("For Publishers"));

    // Insert adds a new version and shows it
    press(card->versionList(), Qt::Key_Insert);
    CHECK(card->versionList()->count() == 3);
    const QString added = card->versionId();
    CHECK_FALSE(added.isEmpty());
    CHECK(added != first);
    CHECK(pm.findElement(added)->publishing.part == PublishingTexts::SYNOPSIS);
    CHECK(textOf(*card).empty());
    // A copy of a version has its text
    card->showVersion(first, false);
    const QString copy = card->addVersion(true);
    REQUIRE_FALSE(copy.isEmpty());
    CHECK(card->versionId() == copy);
    CHECK(textOf(*card) == "Leila crosses the desert.");
    CHECK(pm.findElement(copy)->publishing.state.isEmpty());

    // The version shown deleted: the card stays and shows another one
    REQUIRE(pm.removeElement(copy));
    CHECK(window.tabs->indexOf(card) >= 0);
    CHECK(card->versionId() != copy);
    CHECK_FALSE(card->versionId().isEmpty());
}
