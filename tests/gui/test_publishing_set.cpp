// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_publishing_set.cpp
/// @brief The set for a publisher, the second view of the list of the texts for publishers:
///        its parts with their versions and texts, its keys, Copy and Copy All

#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/gui/publishing_test_utils.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/editor/book_export.h"
#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/dialogs/export_book_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/publishing_set_page.h"
#include "kalahari/gui/widgets/length_meter.h"
#include "kalahari/gui/widgets/publishing_set_view.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>

namespace core = kalahari::core;
namespace editor = kalahari::editor;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::CoverMaterial;
using core::PublishingTexts;
using gui::MaterialList;
using gui::MaterialListPanel;
using gui::PropertiesPanel;
using gui::PublishingSetCard;
using gui::PublishingSetPage;

namespace {

/// The list of the texts for publishers: "kalahari.base:publishing"
constexpr QLatin1String LIST("kalahari.base:publishing");

/// Press @p key with @p modifiers on @p widget
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    REQUIRE(widget != nullptr);
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

/// The texts of the items of @p box
QStringList itemsOf(const QComboBox& box) {
    QStringList items;
    for (int item = 0; item < box.count(); ++item) {
        items << box.itemText(item);
    }
    return items;
}

/// The texts of the set of the open book
struct SetTexts {
    QString first;     ///< The sentence chosen
    QString second;    ///< Another sentence
    QString synopsis;  ///< The synopsis chosen
    QString blurb;     ///< The description of the cover chosen for it
};

/// Two sentences, the first chosen, a synopsis and a description of the cover, both chosen;
/// no tagline and no note about the author
SetTexts addSetTexts(core::ProjectManager& pm) {
    SetTexts texts;
    texts.first = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence – version 1"),
        QStringLiteral("When the well runs dry, Leila must find water."));
    texts.second = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence – version 2"), QStringLiteral("Sand."));
    texts.synopsis = test::addPublishingText(
        PublishingTexts::SYNOPSIS, QStringLiteral("Synopsis – version 1"),
        QStringLiteral("Leila crosses the desert."));
    texts.blurb = test::addCoverText(QStringLiteral("Description 1"), CoverMaterial::BLURB,
                                     QStringLiteral("A road through the sand."));
    REQUIRE(pm.setPublishingState(texts.first, QString::fromLatin1(PublishingTexts::CHOSEN)));
    REQUIRE(pm.setPublishingState(texts.synopsis, QString::fromLatin1(PublishingTexts::CHOSEN)));
    REQUIRE(pm.setCoverState(texts.blurb, QString::fromLatin1(CoverMaterial::CHOSEN)));
    return texts;
}

/// The texts of the rows of @p list
QStringList rowsOf(const QListWidget& list) {
    QStringList rows;
    for (int row = 0; row < list.count(); ++row) {
        rows << list.item(row)->text();
    }
    return rows;
}

}  // namespace

TEST_CASE("Set for the publisher: the second view of the list of the texts",
          "[gui][publishing][materials]") {
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const auto [first, second, synopsis, blurb] = addSetTexts(pm);

    test::Window window;
    gui::MaterialListCoordinator lists(pm, window.tabs, &window.navigator, &window.statusBar,
                                       [](bool) {});
    MaterialListPanel* panel = lists.openList(LIST, false);
    REQUIRE(panel != nullptr);
    // The second view, kept for the list
    REQUIRE(panel->planButton()->isVisibleTo(panel));
    CHECK(panel->planButton()->toolTip() == QStringLiteral("Set for the Publisher"));
    QStringList properties;
    QObject::connect(panel, &MaterialListPanel::requestSetProperties,
                     [&properties](const QString& part) { properties << part; });
    panel->showPlan(true);
    CHECK(MaterialList::planShown(LIST));
    CHECK(panel->planButton()->isChecked());
    CHECK(properties.count() == 1);
    // The search and the filters do not filter the set
    CHECK_FALSE(panel->searchField()->isVisibleTo(panel));

    gui::PublishingSetView* set = panel->setView();
    REQUIRE(set != nullptr);
    CHECK(set->titleLabel()->text() == QStringLiteral("SAND"));
    CHECK(set->parts() == PublishingTexts::setParts());
    CHECK(panel->setHeading()->text().contains(QStringLiteral("Set for the Publisher · 5 of 5")));
    // A sentence: its versions, the one chosen marked, its length with its target
    PublishingSetCard* pitch = set->card(QStringLiteral("pitch"));
    REQUIRE(pitch != nullptr);
    CHECK(itemsOf(*pitch->versionBox()) ==
          QStringList{QStringLiteral("Sentence – version 1 ★"),
                      QStringLiteral("Sentence – version 2")});
    CHECK(pitch->versionBox()->currentData().toString() == first);
    CHECK(pitch->textLabel()->text() ==
          QStringLiteral("When the well runs dry, Leila must find water."));
    CHECK(pitch->textLabel()->font().italic());
    CHECK(pitch->findChild<QLabel*>(QStringLiteral("publishingSetLength"))->text() ==
          QStringLiteral("46 / 200 ch."));
    // A description of the Cover, chosen for the cover; a tagline the book does not have
    PublishingSetCard* cover = set->card(QStringLiteral("blurb"));
    REQUIRE(cover != nullptr);
    CHECK(cover->findChild<QLabel*>(QStringLiteral("publishingSetFromCover")) != nullptr);
    CHECK(itemsOf(*cover->versionBox()) ==
          QStringList{QStringLiteral("Description 1 ★ on the cover")});
    CHECK_FALSE(cover->versionBox()->isEnabled());
    PublishingSetCard* tagline = set->card(QStringLiteral("tagline"));
    REQUIRE(tagline != nullptr);
    CHECK(tagline->textLabel()->text() == QStringLiteral("The book has no such text yet."));
    CHECK_FALSE(tagline->openButton()->isEnabled());
    CHECK_FALSE(tagline->copyButton()->isEnabled());

    // Copy All: the title and the author, then each part with a text under its name
    panel->copyAllButton()->click();
    CHECK(QApplication::clipboard()->text().toStdString() ==
          "Sand\nAuthor\n\n"
          "SENTENCE ABOUT THE BOOK\nWhen the well runs dry, Leila must find water.\n\n"
          "COVER DESCRIPTION\nA road through the sand.\n\n"
          "SYNOPSIS\nLeila crosses the desert.\n");
    // Copy of a part: its text
    pitch->copyButton()->click();
    CHECK(QApplication::clipboard()->text() ==
          QStringLiteral("When the well runs dry, Leila must find water."));

    // The keys: Down goes to the next part, which the Properties panel shows
    panel->focusList();
    CHECK(set->current() == QStringLiteral("pitch"));
    press(pitch, Qt::Key_Down);
    CHECK(set->current() == QStringLiteral("tagline"));
    CHECK(properties.last() == QStringLiteral("tagline"));
    // Enter opens the version of the part
    QStringList opened;
    QObject::connect(panel, &MaterialListPanel::requestOpen,
                     [&opened](const QString& elementId) { opened << elementId; });
    press(set->card(QStringLiteral("blurb")), Qt::Key_Return);
    CHECK(opened.contains(blurb));
    // Space leaves a part out, and includes it again
    press(set->card(QStringLiteral("synopsis")), Qt::Key_Space);
    CHECK_FALSE(pm.publishingSet().includes(QStringLiteral("synopsis")));
    CHECK(panel->setHeading()->text().contains(QStringLiteral("4 of 5")));
    CHECK(set->card(QStringLiteral("synopsis"))->accessibleName() ==
          QStringLiteral("Synopsis (left out of the set)"));
    panel->copyAllButton()->click();
    CHECK_FALSE(QApplication::clipboard()->text().contains(QStringLiteral("SYNOPSIS")));
    press(set->card(QStringLiteral("synopsis")), Qt::Key_Space);
    CHECK(pm.publishingSet().includes(QStringLiteral("synopsis")));

    // Another version for the set alone: the sentence chosen stays chosen
    QComboBox* versions = set->card(QStringLiteral("pitch"))->versionBox();
    const int other = versions->findData(second);
    REQUIRE(other >= 0);
    versions->setCurrentIndex(other);
    emit versions->activated(other);
    CHECK(pm.publishingSet().picks.value(QStringLiteral("pitch")) == second);
    CHECK(pm.findElement(first)->publishing.state == PublishingTexts::CHOSEN);
    CHECK(set->card(QStringLiteral("pitch"))->textLabel()->text() == QStringLiteral("Sand."));

    // Back to the table: the search again
    panel->showPlan(false);
    CHECK_FALSE(MaterialList::planShown(LIST));
    CHECK(panel->searchField()->isVisibleTo(panel));
    CHECK(panel->view() == static_cast<QAbstractItemView*>(panel->table()));
}

TEST_CASE("Set for the publisher in the Properties panel: its contents, the part chosen, Export",
          "[gui][publishing][materials][properties]") {
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const auto [first, second, synopsis, blurb] = addSetTexts(pm);

    test::Window window;
    int shown = 0;
    gui::MaterialListCoordinator lists(pm, window.tabs, &window.navigator, &window.statusBar,
                                       [&shown](bool) { ++shown; });
    MaterialListPanel* panel = lists.openList(LIST, false);
    REQUIRE(panel != nullptr);
    // The set shown: the Properties panel shows it, brought to the front
    panel->showPlan(true);
    CHECK(window.properties->currentPage() == PropertiesPanel::Page::PublishingSet);
    CHECK(shown == 1);
    PublishingSetPage* page = window.properties->publishingSetPage();
    REQUIRE(page != nullptr);
    CHECK(page->part() == QStringLiteral("pitch"));
    CHECK(page->summaryLabel()->text() == QStringLiteral("5 parts · 95 characters in all"));
    QListWidget* contents = page->contentsList();
    CHECK(rowsOf(*contents) ==
          QStringList{QStringLiteral("Sentence about the book · 46 ch."),
                      QStringLiteral("Tagline · no version"),
                      QStringLiteral("Cover description · 24 ch."),
                      QStringLiteral("Synopsis · 25 ch."),
                      QStringLiteral("About the author · no version")});
    CHECK(contents->item(0)->checkState() == Qt::Checked);
    CHECK(contents->currentRow() == 0);
    CHECK_FALSE(page->upButton()->isEnabled());
    CHECK(page->downButton()->isEnabled());
    // The part chosen: its version, where it comes from, its length against its target
    CHECK(page->partGroup()->title() == QStringLiteral("Part Chosen: Sentence about the book"));
    CHECK(page->versionLabel()->text() == QStringLiteral("Sentence – version 1"));
    CHECK(page->sourceLabel()->text() == QStringLiteral("Texts for Publishers"));
    CHECK(page->meter()->countsLabel()->text().startsWith(
        QStringLiteral("46 characters with spaces")));
    CHECK(page->meter()->bar()->isVisibleTo(page));

    // Down: the next part, chosen on the page of the set too; without a version
    window.properties->focusFirstField();
    press(contents, Qt::Key_Down);
    CHECK(page->part() == QStringLiteral("tagline"));
    CHECK(panel->setView()->current() == QStringLiteral("tagline"));
    CHECK(page->findChild<QLabel*>(QStringLiteral("publishingSetNoVersion"))->isVisibleTo(page));
    CHECK_FALSE(page->openButton()->isVisibleTo(page));
    // Space leaves it out; the page of the set and its heading follow
    press(contents, Qt::Key_Space);
    CHECK_FALSE(pm.publishingSet().includes(QStringLiteral("tagline")));
    CHECK(contents->item(1)->checkState() == Qt::Unchecked);
    CHECK(page->summaryLabel()->text() == QStringLiteral("4 parts · 95 characters in all"));
    CHECK(panel->setHeading()->text().contains(QStringLiteral("4 of 5")));
    // Alt+Down moves it, the keys stay with it
    press(contents, Qt::Key_Down, Qt::AltModifier);
    CHECK(pm.publishingSet().parts() ==
          QStringList{QStringLiteral("pitch"), QStringLiteral("blurb"),
                      QStringLiteral("tagline"), QStringLiteral("synopsis"),
                      QStringLiteral("aboutAuthor")});
    CHECK(contents->currentRow() == 2);
    CHECK(page->part() == QStringLiteral("tagline"));
    CHECK(panel->setView()->parts().at(2) == QStringLiteral("tagline"));
    // Alt+Up and the button move it back
    press(contents, Qt::Key_Up, Qt::AltModifier);
    CHECK(pm.publishingSet().parts().at(1) == QStringLiteral("tagline"));
    page->downButton()->click();
    CHECK(pm.publishingSet().parts().at(2) == QStringLiteral("tagline"));
    page->upButton()->click();
    CHECK(pm.publishingSet().parts() == PublishingTexts::setParts());
    // A click on its check includes it again
    contents->item(1)->setCheckState(Qt::Checked);
    CHECK(pm.publishingSet().includes(QStringLiteral("tagline")));

    // Enter opens the version of the part; Open does the same
    QStringList opened;
    QObject::connect(page, &PublishingSetPage::requestOpen,
                     [&opened](const QString& elementId) { opened << elementId; });
    press(contents, Qt::Key_Down);
    CHECK(page->part() == QStringLiteral("blurb"));
    CHECK(page->sourceLabel()->text() == QStringLiteral("Cover"));
    press(contents, Qt::Key_Return);
    CHECK(opened == QStringList{blurb});
    page->openButton()->click();
    CHECK(opened.size() == 2);

    // A part chosen on the page of the set shows here
    press(panel->setView()->card(QStringLiteral("blurb")), Qt::Key_Down);
    CHECK(page->part() == QStringLiteral("synopsis"));
    CHECK(contents->currentRow() == 3);
    CHECK(page->versionLabel()->text() == QStringLiteral("Synopsis – version 1"));

    // Copy All, Export Set... and the gear
    page->copyAllButton()->click();
    CHECK(QApplication::clipboard()->text().startsWith(QStringLiteral("Sand\nAuthor\n\n")));
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("Copied the set for the publisher"));
    int exports = 0;
    int settings = 0;
    QObject::connect(&lists, &gui::MaterialListCoordinator::exportSetRequested,
                     [&exports]() { ++exports; });
    QObject::connect(&lists, &gui::MaterialListCoordinator::settingsRequested,
                     [&settings]() { ++settings; });
    page->exportButton()->click();
    panel->exportSetButton()->click();
    page->settingsButton()->click();
    CHECK(exports == 2);
    CHECK(settings == 1);
}

TEST_CASE("Set for the publisher: the fourth source of the export, with the headings of its parts",
          "[gui][publishing][export]") {
    using gui::dialogs::ExportBookDialog;
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    (void)addSetTexts(pm);
    test::Window window;
    const gui::BookExportCoordinator coordinator(
        pm, test::commands(), window.tabs, []() -> gui::EditorPanel* { return nullptr; },
        nullptr, nullptr, &window.window);

    // The window of the export: the set as one text, with the parts that have a text
    const ExportBookDialog::Book shown = coordinator.dialogBook();
    CHECK(shown.setTitle == QStringLiteral("Sand – set for the publisher"));
    CHECK(shown.setParts == 3);

    // The title of the book with its author, then each part under its name
    ExportBookDialog::Choice choice;
    choice.content = ExportBookDialog::Content::Set;
    choice.format = editor::ExportFormat::Markdown;
    const editor::BookExport book = coordinator.bookExport(choice);
    REQUIRE(book.items.size() == 4);
    CHECK(book.items[0].title == QStringLiteral("Sand"));
    CHECK(book.items[0].titleStyle == QStringLiteral("heading_1"));
    CHECK(book.items[0].kml.contains(QStringLiteral("Author")));
    CHECK(book.items[1].title == QStringLiteral("Sentence about the book"));
    CHECK(book.items[1].titleStyle == QStringLiteral("heading_2"));
    CHECK(book.items[1].kml.contains(
        QStringLiteral("When the well runs dry, Leila must find water.")));
    CHECK(book.items[2].title == QStringLiteral("Cover description"));
    CHECK(book.items[3].title == QStringLiteral("Synopsis"));

    // A Markdown file: the headings of the parts
    const QTemporaryDir dir;
    const QString path = QDir(dir.path()).filePath(QStringLiteral("set.md"));
    editor::ExportResult result;
    editor::ExportProblem problem;
    REQUIRE(editor::exportMarkdown(book, path, QString(), {}, &result, &problem));
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(file.readAll());
    INFO(text.toStdString());
    CHECK(text.startsWith(QStringLiteral("# Sand\n\nAuthor\n\n## Sentence about the book\n\n"
                                         "When the well runs dry, Leila must find water.\n")));
    CHECK(text.contains(QStringLiteral("## Cover description\n\nA road through the sand.\n")));
    CHECK(text.contains(QStringLiteral("## Synopsis\n\nLeila crosses the desert.\n")));

    // A part left out of the set is not in the file
    REQUIRE(pm.setPublishingSet(pm.publishingSet().withPart(QStringLiteral("synopsis"), false)));
    CHECK(coordinator.bookExport(choice).items.size() == 3);
    CHECK(coordinator.dialogBook().setParts == 2);
}
