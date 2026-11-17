// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_pitch_panel.cpp
/// @brief The card of a sentence about the book: the fields of its pattern compose the
///        sentence until it is changed by hand, its template, length, target and state

#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/pitch_panel.h"
#include "kalahari/gui/widgets/length_meter.h"

#include <catch2/catch_test_macros.hpp>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

#include <string>

using namespace kalahari;
using core::PublishingTexts;
using gui::PitchPanel;

namespace {

/// Type @p text in @p field as the writer does: the field gets it and says it was edited
void type(QLineEdit* field, const QString& text) {
    REQUIRE(field != nullptr);
    field->setText(text);
    emit field->textEdited(text);
}

/// The sentence of @p panel
std::string sentenceOf(const PitchPanel& panel) {
    return panel.getBookEditor()->plainText().trimmed().toStdString();
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
            std::fputs("CharactersUnit: the unit of the test stays\n", stderr);
        }
    }
    CharactersUnit(const CharactersUnit&) = delete;
    CharactersUnit& operator=(const CharactersUnit&) = delete;

private:
    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }
    std::string m_unit;
};

}  // namespace

TEST_CASE("Sentence about the book: its card composes it of the fields of its pattern",
          "[gui][publishing][pitch]") {
    gui::registerAllIcons();
    CharactersUnit unit;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString id = pm.addPublishingText(QString::fromLatin1(PublishingTexts::PITCH),
                                            QStringLiteral("kalahari.novel:when_must"),
                                            QStringLiteral("Sentence – version 1"));
    REQUIRE_FALSE(id.isEmpty());
    test::Window window;
    window.navigator.onElementSelected(id, QStringLiteral("Sentence – version 1"));
    auto* panel = qobject_cast<PitchPanel*>(window.tabs->currentWidget());
    REQUIRE(panel != nullptr);
    CHECK(panel->elementId() == id);
    CHECK(panel->titleEdit()->text() == QStringLiteral("Sentence – version 1"));
    CHECK(panel->templateBox()->currentData().toString() ==
          QStringLiteral("kalahari.novel:when_must"));
    CHECK(panel->templateBox()->currentText() == QStringLiteral("When… must… before…"));
    // An empty sentence; the fields of the pattern, in its order
    CHECK(sentenceOf(*panel).empty());
    REQUIRE(panel->field(QStringLiteral("event")) != nullptr);
    CHECK(panel->field(QStringLiteral("event"))->accessibleName() ==
          QStringLiteral("what happens"));
    CHECK(panel->field(QStringLiteral("stakes")) != nullptr);
    CHECK(panel->field(QStringLiteral("nothing")) == nullptr);
    CHECK_FALSE(panel->manualNote()->isVisibleTo(panel));

    // The fields compose the sentence at once; what is missing is "…"
    type(panel->field(QStringLiteral("event")), QStringLiteral("the well runs dry"));
    CHECK(sentenceOf(*panel) == "When the well runs dry, … must … before ….");
    type(panel->field(QStringLiteral("hero")), QStringLiteral("Leila"));
    type(panel->field(QStringLiteral("goal")), QStringLiteral("find water"));
    type(panel->field(QStringLiteral("stakes")), QStringLiteral("the caravan dies"));
    const std::string sentence =
        "When the well runs dry, Leila must find water before the caravan dies.";
    CHECK(sentenceOf(*panel) == sentence);
    // Its length against the target of the template
    CHECK(panel->meter()->targetLabel()->text() == QStringLiteral("target: up to 200 characters"));
    CHECK(panel->meter()->countsLabel()->text().startsWith(
        QStringLiteral("70 characters with spaces")));
    // The fields are saved with the book
    panel->saveFields();
    CHECK(pm.findElement(id)->publishing.fields.value(QStringLiteral("hero")) ==
          QStringLiteral("Leila"));
    CHECK_FALSE(pm.findElement(id)->publishing.manual);

    // Changed by hand: the fields no longer change it, until it is composed again
    panel->getBookEditor()->replaceWithKml(editor::ClipboardHandler::textToKml(
        QStringLiteral("Leila looks for the last well.")));
    CHECK(pm.findElement(id)->publishing.manual);
    CHECK(panel->manualNote()->isVisibleTo(panel));
    type(panel->field(QStringLiteral("hero")), QStringLiteral("Young Leila"));
    CHECK(sentenceOf(*panel) == "Leila looks for the last well.");
    panel->composeButton()->click();
    CHECK_FALSE(pm.findElement(id)->publishing.manual);
    CHECK_FALSE(panel->manualNote()->isVisibleTo(panel));
    CHECK(sentenceOf(*panel) ==
          "When the well runs dry, Young Leila must find water before the caravan dies.");

    // Another template: the fields of the same names keep their values
    const int heroGoal = panel->templateBox()->findData(QStringLiteral("kalahari.novel:hero_goal"));
    REQUIRE(heroGoal >= 0);
    panel->templateBox()->setCurrentIndex(heroGoal);
    emit panel->templateBox()->activated(heroGoal);
    CHECK(pm.findElement(id)->publishing.templateId == QStringLiteral("kalahari.novel:hero_goal"));
    REQUIRE(panel->field(QStringLiteral("who")) != nullptr);
    CHECK(panel->field(QStringLiteral("hero"))->text() == QStringLiteral("Young Leila"));
    CHECK(sentenceOf(*panel) == "Young Leila, …, wants find water, but ….");
    CHECK(panel->field(QStringLiteral("event")) == nullptr);
    // Without a template the sentence is written as it is
    const int none = panel->templateBox()->findData(QString());
    panel->templateBox()->setCurrentIndex(none);
    emit panel->templateBox()->activated(none);
    CHECK(pm.findElement(id)->publishing.templateId.isEmpty());
    CHECK(panel->field(QStringLiteral("hero")) == nullptr);
    CHECK(panel->meter()->targetLabel()->text() == QStringLiteral("target: up to 200 characters"));

    // Its own target, its state and whether the set has it
    panel->targetSpin()->setValue(90);
    CHECK(pm.findElement(id)->publishing.targetCharacters == 90);
    CHECK(panel->meter()->targetLabel()->text() == QStringLiteral("target: up to 90 characters"));
    CHECK(panel->inSetLabel()->text() == QStringLiteral("In the set: no"));
    const int chosen = panel->stateBox()->findData(QString::fromLatin1(PublishingTexts::CHOSEN));
    panel->stateBox()->setCurrentIndex(chosen);
    emit panel->stateBox()->activated(chosen);
    CHECK(pm.findElement(id)->publishing.state == PublishingTexts::CHOSEN);
    CHECK(panel->inSetLabel()->text() == QStringLiteral("In the set: yes – this version"));
    // Rejected elsewhere: the card shows it
    REQUIRE(pm.setPublishingState(id, QString::fromLatin1(PublishingTexts::REJECTED)));
    CHECK(panel->stateBox()->currentData().toString() == PublishingTexts::REJECTED);

    // A new title: the tab shows it
    panel->titleEdit()->setText(QStringLiteral("For the contest"));
    emit panel->titleEdit()->editingFinished();
    CHECK(pm.findElement(id)->title == QStringLiteral("For the contest"));
    // The sentence is not saved yet: the tab shows "*" before the title
    CHECK(window.tabs->tabText(window.tabs->indexOf(panel)) == QStringLiteral("*For the contest"));
}

TEST_CASE("Sentence about the book: a synopsis opens in the editor, a sentence on its card",
          "[gui][publishing][pitch]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString synopsis = pm.addPublishingText(QString::fromLatin1(PublishingTexts::SYNOPSIS),
                                                  QString(), QStringLiteral("Synopsis"));
    REQUIRE_FALSE(synopsis.isEmpty());
    test::Window window;
    window.navigator.onElementSelected(synopsis, QStringLiteral("Synopsis"));
    QWidget* tab = window.tabs->currentWidget();
    REQUIRE(tab != nullptr);
    CHECK(qobject_cast<PitchPanel*>(tab) == nullptr);
    CHECK(tab->property("elementId").toString() == synopsis);
}
