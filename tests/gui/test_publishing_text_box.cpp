// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_publishing_text_box.cpp
/// @brief A text for publishers in the Properties panel: its length with the target and the
///        standard pages, its template and its points ticked off, its state and own target;
///        Template Points gives the keys to the points, Esc gives them back to the text

#include "kalahari_tests/editor/editor_test_utils.h"
#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/text_skeleton.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/main_window.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/publishing_text_box.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/publishing_set.h"
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
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolButton>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>

namespace core = kalahari::core;
namespace editor = kalahari::editor;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
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

/// Whether a label of @p box gives the keys to @p field with Alt and its letter
bool hasLabel(const PublishingTextBox& box, const QWidget* field) {
    const QList<QLabel*> labels = box.findChildren<QLabel*>();
    return std::any_of(labels.cbegin(), labels.cend(),
                       [field](const QLabel* label) { return label->buddy() == field; });
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
    const CharactersUnit unit;
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

    // Its own target, its state and whether the set has it; Alt and the letter of each label
    // give the keys to its field
    CHECK(hasLabel(*box, box->templateBox()));
    CHECK(hasLabel(*box, box->stateBox()));
    CHECK(hasLabel(*box, box->targetSpin()));
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
    const CharactersUnit unit;
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

TEST_CASE("Texts for publishers: the skeleton of a synopsis in the editor, its length without "
          "the headings",
          "[gui][publishing][synopsisbox]") {
    gui::registerAllIcons();
    const CharactersUnit unit;
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
    PublishingTextBox* box = properties->frontPublishingBox();
    REQUIRE(box == properties->publishingBox());
    CHECK(box->editor() == editorPanel->getBookEditor());

    // The points of its template, in the language of the book
    const QList<editor::SkeletonPoint> points = box->skeletonPoints();
    REQUIRE(points.size() == 5);
    CHECK(points.first().name == QStringLiteral("Hero and world"));
    CHECK_FALSE(points.first().hint.isEmpty());

    // The headings do not count; the words typed under them do
    editor::BookEditor* text = editorPanel->getBookEditor();
    REQUIRE(text->insertTemplateSkeleton(points));
    CHECK(editor::hasSkeleton(*text->textDocument()));
    text->insertText(QStringLiteral("Leila goes."));
    CHECK(test::waitUntil([box]() {
        return box->meter()->countsLabel()->text().startsWith(
            QStringLiteral("11 characters with spaces"));
    }));
    CHECK(text->removeTemplateSkeleton() == 5);
    CHECK(text->plainText() == QStringLiteral("Leila goes."));
}

TEST_CASE("Texts for publishers: fragments of a synopsis linked to its points, their states, "
          "keys and marks",
          "[gui][publishing][synopsisbox][points]") {
    gui::registerAllIcons();
    const CharactersUnit unit;
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
    PublishingTextBox* box = properties->frontPublishingBox();
    REQUIRE(box != nullptr);
    REQUIRE(box->points().size() == 5);
    editor::BookEditor* text = editorPanel->getBookEditor();

    // No fragment yet: every point is not in the text; the editor knows the points
    text->replaceWithKml(
        editor::ClipboardHandler::textToKml(QStringLiteral("Leila goes to the last well.")));
    REQUIRE(test::waitUntil([box]() {
        return box->pointsSummary()->text() == QStringLiteral("In the text: 0 of 5 · Done: 0 of 5");
    }));
    CHECK(box->pointStates().constFirst()->text() == QStringLiteral("Not in the text"));
    CHECK(text->templatePointLook().order ==
          (QStringList{QStringLiteral("hero"), QStringLiteral("event"), QStringLiteral("goal"),
                       QStringLiteral("climax"), QStringLiteral("ending")}));
    CHECK(text->templatePointLook().colors.size() == 8);

    // "Leila" linked to the hero (Insert in the point does the same as its button)
    text->setSelection({{0, 0}, {0, 5}});
    CHECK(box->linkSelection(QStringLiteral("hero")));
    CHECK(text->toKml().contains(QStringLiteral("<points ids=\"hero\">Leila</points>")));
    CHECK(box->pointsSummary()->text() == QStringLiteral("In the text: 1 of 5 · Done: 0 of 5"));
    CHECK(box->pointStates().constFirst()->text() ==
          QStringLiteral("In the text: 1 fragment, 5 characters"));
    // The bar beside the fragment on the margin; no fill until Show in the Text
    REQUIRE(text->templatePointLook().bars);
    CHECK_FALSE(text->templatePointLook().shown);
    box->showInTextButton()->toggle();
    CHECK(text->templatePointLook().shown);

    // A point ticked off without a fragment: the warning under it
    box->points().at(1)->setChecked(true);
    CHECK(box->pointStates().at(1)->text() ==
          QStringLiteral("Ticked off, but the text has no fragment of it"));
    CHECK(box->pointStates().at(1)->font().bold());
    CHECK(box->pointsSummary()->text() == QStringLiteral("In the text: 1 of 5 · Done: 1 of 5"));

    // Enter in the point selects its fragment; undo takes the link back
    text->clearSelection();
    text->setCursorPosition({0, 20});
    CHECK(box->goToNextFragment(QStringLiteral("hero")));
    CHECK(text->selection().normalized().start == editor::CursorPosition{0, 0});
    CHECK(text->selection().normalized().end == editor::CursorPosition{0, 5});
    text->undo();
    CHECK_FALSE(text->toKml().contains(QStringLiteral("<points")));
    CHECK(test::waitUntil([box]() {
        return box->pointStates().constFirst()->text() == QStringLiteral("Not in the text");
    }));
    text->redo();
    CHECK(test::waitUntil([box]() {
        return box->pointStates().constFirst()->text() != QStringLiteral("Not in the text");
    }));

    // Delete in the point takes its fragments away, the text stays
    CHECK(box->unlinkPoint(QStringLiteral("hero")));
    CHECK(text->plainText() == QStringLiteral("Leila goes to the last well."));
    CHECK(text->templatePointFragments().isEmpty());

    // The set warns of the points without fragments while the text is not saved too
    text->setSelection({{0, 6}, {0, 10}});
    REQUIRE(text->linkToTemplatePoint(QStringLiteral("event"), true));
    pm.setEditorText(id, text->toKml());
    QStringList unlinked;
    for (const core::PublishingTemplatePoint& point : pm.unlinkedPublishingPointsOf(id)) {
        unlinked << point.id;
    }
    CHECK(unlinked == (QStringList{QStringLiteral("hero"), QStringLiteral("goal"),
                                   QStringLiteral("climax"), QStringLiteral("ending")}));
    CHECK(PublishingTexts::linkedPointsOf(text->toKml()) ==
          QSet<QString>{QStringLiteral("event")});

    if (!canActivateWindows()) {
        SKIP("needs an active window without a window on screen: run with "
             "QT_QPA_PLATFORM=offscreen");
    }
    window.resize(1200, 800);
    window.show();
    window.activateWindow();
    REQUIRE(test::waitUntil([&window] { return QApplication::activeWindow() == &window; }));
    // The keys of the points: Down to the next one, which is then the point chosen; Insert
    // links the selection to it; Delete takes it away; End to the last one
    REQUIRE(properties->focusTemplatePoints());
    press(box->points().constFirst(), Qt::Key_Down);
    CHECK(box->points().at(1)->hasFocus());
    CHECK(text->templatePointLook().current == QStringLiteral("event"));
    text->setSelection({{0, 14}, {0, 18}});
    press(box->points().at(1), Qt::Key_Insert);
    CHECK(text->templatePointFragments().value(QStringLiteral("event")).size() == 2);
    press(box->points().at(1), Qt::Key_Delete);
    CHECK_FALSE(text->templatePointFragments().contains(QStringLiteral("event")));
    press(box->points().at(1), Qt::Key_End);
    CHECK(box->points().constLast()->hasFocus());
    press(box->points().constLast(), Qt::Key_Escape);
    CHECK(text->hasFocus());
}

TEST_CASE("Texts for publishers: in the book window the points ticked off of a synopsis are "
          "saved and dropped with its text",
          "[gui][publishing][synopsisbox][window]") {
    // Test of 22.11 (part 38): the points were saved at once and the text not, so they did not
    // agree with the file after Don't Save
    using gui::dialogs::MessageDialog;
    gui::registerAllIcons();
    const test::RecentBooks recent;
    const QTemporaryDir folder;
    REQUIRE(folder.isValid());
    core::ProjectManager::loadBookTypes(
        {QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    auto window = std::make_unique<gui::MainWindow>();
    window->show();
    auto& pm = window->book();
    REQUIRE(pm.createProject(folder.path(), QStringLiteral("Rain"), QStringLiteral("Author"),
                             QStringLiteral("en"), false, QStringLiteral("kalahari.novel")));
    QCoreApplication::processEvents();
    const QString manifest = pm.getManifestPath();
    const QString id = pm.addPublishingText(QString::fromLatin1(PublishingTexts::SYNOPSIS),
                                            QStringLiteral("kalahari.novel:one_page"),
                                            QStringLiteral("Synopsis"));
    REQUIRE_FALSE(id.isEmpty());
    REQUIRE(pm.setPublishingState(id, QString::fromLatin1(PublishingTexts::CHOSEN)));  // in the set
    auto* properties = window->findChild<gui::PropertiesPanel*>();
    REQUIRE(properties != nullptr);
    PublishingTextBox* box = properties->publishingBox();
    const auto open = [&window, &id, box]() {
        window->openChapter(id, QStringLiteral("Synopsis"));
        QCoreApplication::processEvents();
        gui::EditorPanel* found = nullptr;
        for (gui::EditorPanel* panel : window->findChildren<gui::EditorPanel*>()) {
            if (panel->property("elementId").toString() == id && !panel->isHidden()) {
                found = panel;
            }
        }
        REQUIRE(found != nullptr);
        REQUIRE(box->elementId() == id);
        REQUIRE(box->points().size() == 5);
        return found;
    };
    const auto closeTab = [](gui::EditorPanel* panel, MessageDialog::Answer answer) {
        QTabWidget* tabs = gui::DocumentWindows::tabsOf(panel);
        REQUIRE(tabs != nullptr);
        QStringList said;
        CHECK(test::answering(
                  [tabs, panel]() { emit tabs->tabCloseRequested(tabs->indexOf(panel)); },
                  answer, said) == 1);
        QCoreApplication::processEvents();
    };
    const auto savedPoints = [&manifest, &id]() {
        QStringList problems;
        const std::optional<core::BookProject> saved = core::BookProject::load(manifest, problems);
        REQUIRE(saved);
        const core::ProjectElement* text = saved->findElement(id);
        REQUIRE(text != nullptr);
        return text->publishing.done.join(QStringLiteral(" | ")).toStdString();
    };

    // Written and ticked off, then the tab closed without saving: the point goes with the text
    gui::EditorPanel* panel = open();
    test::write(*panel, QStringLiteral("Leila goes to the last well."));
    box->points().at(0)->click();
    CHECK(box->points().at(0)->isChecked());
    CHECK(savedPoints().empty());
    CHECK(pm.publishingPointsOf(id) == QStringList{QStringLiteral("hero")});
    // The set for the publisher has the text as written, as File > Export has the open texts
    const auto inSet = [&pm]() {
        return gui::PublishingSetContents::plainTextOf(pm).contains(QStringLiteral("last well"));
    };
    CHECK(test::waitUntil(inSet));
    closeTab(panel, MessageDialog::Answer::Alternative);
    CHECK(pm.publishingPointsOf(id).isEmpty());
    CHECK(savedPoints().empty());
    CHECK_FALSE(pm.savedChapterText(id).contains(QStringLiteral("last well")));
    CHECK_FALSE(inSet());

    // Written, ticked off and saved: both in the files
    panel = open();
    CHECK_FALSE(box->points().at(0)->isChecked());
    test::write(*panel, QStringLiteral("Leila goes to the last well."));
    box->points().at(0)->click();
    QAction* save = window->commands().getAction(QStringLiteral("file.save"));
    REQUIRE(save != nullptr);
    save->trigger();
    CHECK(savedPoints() == "hero");
    CHECK(pm.savedChapterText(id).contains(QStringLiteral("last well")));

    // Ticked off with no changes: saved at once
    box->points().at(1)->click();
    CHECK(savedPoints() == "hero | event");

    // Written and ticked off, then the book closed without saving: the book opened again has
    // the text and the points saved
    test::write(*panel, QStringLiteral("Leila reaches the well and finds it dry."));
    box->points().at(3)->click();
    CHECK(savedPoints() == "hero | event");
    QStringList said;
    QAction* close = window->commands().getAction(QStringLiteral("file.close"));
    REQUIRE(close != nullptr);
    test::answering([close]() { close->trigger(); }, MessageDialog::Answer::Alternative, said);
    QCoreApplication::processEvents();
    REQUIRE_FALSE(said.isEmpty());
    CHECK(said.first().contains(QStringLiteral("save changes to 'Rain' before closing")));
    REQUIRE_FALSE(pm.isProjectOpen());
    CHECK(savedPoints() == "hero | event");
    REQUIRE(pm.openProject(manifest));
    QCoreApplication::processEvents();
    CHECK(pm.publishingPointsOf(id) == QStringList{QStringLiteral("hero"), QStringLiteral("event")});
    CHECK(pm.savedChapterText(id).contains(QStringLiteral("last well")));
    CHECK_FALSE(pm.savedChapterText(id).contains(QStringLiteral("dry")));
    // The summary of the writing session may show on closing
    test::answering([&pm]() { pm.closeProject(false); }, MessageDialog::Answer::Accept, said);
    CHECK_FALSE(pm.isProjectOpen());
}
