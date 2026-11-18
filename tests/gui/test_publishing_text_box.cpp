// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_publishing_text_box.cpp
/// @brief A text for publishers in the Properties panel: its length with the target and the
///        standard pages, its template and its points ticked off, its state and own target;
///        Template Points gives the keys to the points, Esc gives them back to the text

#include "kalahari_tests/editor/editor_test_utils.h"
#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/publishing_text_box.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/widgets/length_meter.h"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QSpinBox>

#include <string>

using namespace kalahari;
using core::PublishingTexts;
using gui::PublishingTextBox;

namespace {

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

/// The texts of the points of @p box, joined by " | "
std::string pointsOf(const PublishingTextBox& box) {
    QStringList texts;
    for (const QCheckBox* point : box.points()) {
        texts << point->text();
    }
    return texts.join(QStringLiteral(" | ")).toStdString();
}

/// The points of text @p id ticked off, as the book keeps them, joined by " | "
std::string doneOf(const core::ProjectManager& pm, const QString& id) {
    const core::ProjectElement* element = pm.findElement(id);
    REQUIRE(element != nullptr);
    return element->publishing.done.join(QStringLiteral(" | ")).toStdString();
}

/// Press and let go of @p key in @p widget
void press(QWidget* widget, Qt::Key key) {
    QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &down);
    QKeyEvent up(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &up);
}

/// Choose @p data in @p box as the writer does
void choose(QComboBox* box, const QString& data) {
    const int index = box->findData(data);
    REQUIRE(index >= 0);
    box->setCurrentIndex(index);
    emit box->activated(index);
}

/// The tests of the keys need an active window; the offscreen platform has one without a
/// window on screen
bool canActivateWindows() {
    return QGuiApplication::platformName() == QStringLiteral("offscreen");
}

}  // namespace

TEST_CASE("Texts for publishers: a synopsis in the Properties panel, its template and points",
          "[gui][publishing][synopsisbox]") {
    gui::registerAllIcons();
    CharactersUnit unit;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString id = pm.addPublishingText(QString::fromLatin1(PublishingTexts::SYNOPSIS),
                                            QStringLiteral("kalahari.novel:one_page"),
                                            QStringLiteral("Synopsis – version 1"));
    REQUIRE_FALSE(id.isEmpty());
    gui::PropertiesPanel properties(pm);
    properties.showChapterProperties(id);
    REQUIRE(properties.currentPage() == gui::PropertiesPanel::Page::Chapter);
    PublishingTextBox* box = properties.publishingBox();
    REQUIRE(box != nullptr);
    CHECK(box->elementId() == id);
    CHECK_FALSE(box->isHidden());
    CHECK(box->title() == QStringLiteral("Synopsis"));

    // The length against the target of the template, with the standard pages
    CHECK(box->meter()->targetLabel()->text() == gui::CoverNames::targetText(1800, false));
    CHECK(box->meter()->countsLabel()->text().startsWith(
        QStringLiteral("0 characters with spaces")));
    CHECK(box->meter()->countsLabel()->text().endsWith(gui::PublishingNames::pagesText(0)));
    CHECK(box->targetSourceLabel()->text().startsWith(
        QStringLiteral("The target length of the template \"One page – five steps\"")));

    // The points of the template, ticked off with a click or Space, saved at once
    CHECK(box->templateBox()->currentData().toString() ==
          QStringLiteral("kalahari.novel:one_page"));
    CHECK(pointsOf(*box) == "Hero and world | The event that changes everything | Goal and "
                            "obstacles | Climax | Ending");
    REQUIRE(box->points().size() == 5);
    CHECK(box->points().at(0)->accessibleDescription() ==
          QStringLiteral("Who the hero is, where and when the story takes place."));
    CHECK(box->pointsHint()->text().contains(QStringLiteral("Template Points")));
    box->points().at(1)->click();
    CHECK(doneOf(pm, id) == "event");
    press(box->points().at(0), Qt::Key_Space);
    CHECK(doneOf(pm, id) == "event | hero");
    box->points().at(1)->click();
    CHECK(doneOf(pm, id) == "hero");
    // The panel shown again keeps them ticked off
    properties.showChapterProperties(novel.chapterId);
    CHECK(box->isHidden());  // a chapter has none
    CHECK(box->elementId().isEmpty());
    properties.showChapterProperties(id);
    REQUIRE(box->points().size() == 5);
    CHECK(box->points().at(0)->isChecked());
    CHECK_FALSE(box->points().at(1)->isChecked());

    // A point for each chapter of the book, named with its title: the first of a new novel
    // and the one of the test
    choose(box->templateBox(), QStringLiteral("kalahari.novel:each_chapter"));
    CHECK(pm.findElement(id)->publishing.templateId ==
          QStringLiteral("kalahari.novel:each_chapter"));
    CHECK(pointsOf(*box) == "Chapter 1 | The Storm");
    // Without a template: no points
    choose(box->templateBox(), QString());
    CHECK(pm.findElement(id)->publishing.templateId.isEmpty());
    CHECK(box->points().isEmpty());
    CHECK(box->pointsHint()->text().startsWith(QStringLiteral("A text without a template.")));
    CHECK(box->targetSourceLabel()->text().startsWith(
        QStringLiteral("The target length from the Settings")));

    // Its own target, its state and whether the set has it
    box->targetSpin()->setValue(900);
    CHECK(pm.findElement(id)->publishing.targetCharacters == 900);
    CHECK(box->meter()->targetLabel()->text() == gui::CoverNames::targetText(900, false));
    CHECK(box->targetSourceLabel()->text() == QStringLiteral("The target length of this text."));
    CHECK(box->inSetLabel()->text() == QStringLiteral("In the set: no"));
    choose(box->stateBox(), QString::fromLatin1(PublishingTexts::CHOSEN));
    CHECK(pm.findElement(id)->publishing.state == PublishingTexts::CHOSEN);
    CHECK(box->inSetLabel()->text() == QStringLiteral("In the set: yes – this version"));
    // Rejected elsewhere: the panel shows it
    REQUIRE(pm.setPublishingState(id, QString::fromLatin1(PublishingTexts::REJECTED)));
    CHECK(box->stateBox()->currentData().toString() == PublishingTexts::REJECTED);
    CHECK(box->inSetLabel()->text() == QStringLiteral("In the set: no"));
}

TEST_CASE("Texts for publishers: a synopsis in the editor, its length as typed; Template "
          "Points and Esc",
          "[gui][publishing][synopsisbox]") {
    gui::registerAllIcons();
    CharactersUnit unit;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString id = pm.addPublishingText(QString::fromLatin1(PublishingTexts::SYNOPSIS),
                                            QStringLiteral("kalahari.novel:one_page"),
                                            QStringLiteral("Synopsis – version 1"));
    REQUIRE_FALSE(id.isEmpty());
    QWidget window;
    auto* layout = new QHBoxLayout(&window);
    auto* editorPanel = new gui::EditorPanel(pm, test::commands(), &window);
    editorPanel->setProperty("elementId", id);
    auto* properties = new gui::PropertiesPanel(pm, &window);
    layout->addWidget(editorPanel);
    layout->addWidget(properties);
    properties->setActiveEditor(editorPanel);
    REQUIRE(properties->currentPage() == gui::PropertiesPanel::Page::Editor);
    PublishingTextBox* box = properties->publishingBox();
    CHECK(box->elementId() == id);

    // The words typed count at once, before they are saved
    editorPanel->getBookEditor()->replaceWithKml(
        editor::ClipboardHandler::textToKml(QStringLiteral("Leila goes to the last well.")));
    CHECK(test::waitUntil([box]() {
        return box->meter()->countsLabel()->text().startsWith(
            QStringLiteral("28 characters with spaces"));
    }));
    // On the page of the element too
    properties->showChapterProperties(id);
    CHECK(box->meter()->countsLabel()->text().startsWith(
        QStringLiteral("28 characters with spaces")));

    if (!canActivateWindows()) {
        SKIP("needs an active window without a window on screen: run with "
             "QT_QPA_PLATFORM=offscreen");
    }
    window.resize(1200, 800);
    window.show();
    window.activateWindow();
    REQUIRE(test::waitUntil([&window] { return QApplication::activeWindow() == &window; }));
    editorPanel->getBookEditor()->setFocus();
    // Template Points: the keys in the first point; Esc: back in the text
    REQUIRE(properties->focusTemplatePoints());
    CHECK(box->points().constFirst()->hasFocus());
    press(box->points().constFirst(), Qt::Key_Escape);
    CHECK(editorPanel->getBookEditor()->hasFocus());
    // From another page of the panel, the page of the text in front
    properties->showChapterProperties(novel.chapterId);
    REQUIRE(properties->focusTemplatePoints());
    CHECK(box->elementId() == id);
    CHECK(box->points().constFirst()->hasFocus());
    // A text without points: the keys stay where they are
    REQUIRE(pm.setPublishing(id, [&pm, &id]() {
        core::PublishingData data = pm.findElement(id)->publishing;
        data.templateId.clear();
        return data;
    }()));
    CHECK_FALSE(properties->focusTemplatePoints());
}
