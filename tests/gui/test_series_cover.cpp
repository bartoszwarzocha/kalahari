// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_series_cover.cpp
/// @brief A series in the window: the cover and the texts for publishers of each volume – the
/// list Volume in the Properties panel, the lists in groups of the volumes, the plan of the
/// cover, the set for the publisher and the export of each volume

#include "kalahari_tests/gui/context_menu_reader.h"
#include "kalahari_tests/gui/dialog_checks.h"
#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/gui/publishing_test_utils.h"
#include "kalahari_tests/polish.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/editor/book_export.h"
#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/dialogs/export_book_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/material_page.h"
#include "kalahari/gui/panels/publishing_text_box.h"
#include "kalahari/gui/publishing_set.h"
#include "kalahari/gui/widgets/cover_plan.h"
#include "kalahari/gui/widgets/publishing_set_view.h"

#include <catch2/catch_test_macros.hpp>

#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QSortFilterProxyModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTableView>
#include <QString>
#include <QStringList>

#include <algorithm>

namespace core = kalahari::core;
namespace editor = kalahari::editor;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::CoverMaterial;
using core::PublishingTexts;
using gui::MaterialListPanel;

namespace {

/// The list of the cover: "kalahari.base:cover"
constexpr QLatin1String COVER_LIST("kalahari.base:cover");

/// A material of the cover of the open book, in its current volume: @p part at @p place
QString addCover(const char* title, const char* part, const char* place) {
    auto& pm = test::book();
    core::NewMaterial material;
    material.title = QString::fromUtf8(title);
    material.description = QString::fromUtf8(title);
    material.cover.part = QString::fromLatin1(part);
    material.cover.place = QString::fromLatin1(place);
    const QString id = pm.addMaterial(
        material,
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("cover")));
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// The open novel "Sand" becomes a series with the volume "Dunes"; the id of the volume
QString addDunes(core::ProjectManager& pm) {
    core::NewBook request;
    request.title = QStringLiteral("Dunes");
    const QString dunes = pm.addBook(request);
    REQUIRE_FALSE(dunes.isEmpty());
    return dunes;
}

/// The texts of the items of @p box
QStringList itemsOf(const QComboBox& box) {
    QStringList items;
    for (int item = 0; item < box.count(); ++item) {
        items << box.itemText(item);
    }
    return items;
}

/// Choose item @p index of @p box as the writer does
void choose(QComboBox& box, int index) {
    box.setCurrentIndex(index);
    emit box.activated(index);
}

/// The names of the groups of the rows of @p panel, in their order
QStringList headingsOf(const MaterialListPanel& panel) {
    QStringList headings;
    const QSortFilterProxyModel* rows = panel.rows();
    for (int row = 0; row < rows->rowCount(); ++row) {
        const QString heading =
            rows->index(row, 0).data(MaterialListPanel::HEADING_ROLE).toString();
        if (!heading.isEmpty()) {
            headings << heading;
        }
    }
    return headings;
}

/// A click of the left button at @p pos of @p widget, as the mouse gives it
void click(QWidget* widget, const QPoint& pos) {
    const QPointF globalPos = widget->mapToGlobal(QPointF(pos));
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(pos), globalPos, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(pos), globalPos, Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &release);
}

/// A key pressed and released in @p widget
void press(QWidget* widget, int key) {
    QKeyEvent pressed(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &pressed);
    QKeyEvent released(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &released);
}

/// The text of the column @p title of the row of material @p id in @p panel; empty: none
QString cellOf(const MaterialListPanel& panel, const QString& id, const QString& title) {
    const QSortFilterProxyModel* rows = panel.rows();
    int column = -1;
    for (int section = 0; section < rows->columnCount(); ++section) {
        if (rows->headerData(section, Qt::Horizontal).toString() == title) {
            column = section;
        }
    }
    for (int row = 0; column >= 0 && row < rows->rowCount(); ++row) {
        if (rows->index(row, 0).data(MaterialListPanel::ID_ROLE).toString() == id) {
            return rows->index(row, column).data().toString();
        }
    }
    return {};
}

/// The label of field @p field in @p root
const QLabel* labelOf(const QWidget& root, const QWidget* field) {
    for (const QLabel* label : root.findChildren<QLabel*>()) {
        if (label->buddy() == field) {
            return label;
        }
    }
    return nullptr;
}

/// The element of part @p part of the parts of a set; empty: none
QString elementOf(const QList<gui::PublishingSetItem>& items, const char* part) {
    for (const gui::PublishingSetItem& item : items) {
        if (item.part == QLatin1String(part)) {
            return item.elementId;
        }
    }
    return {};
}

}  // namespace

TEST_CASE("Series: the volume of a material of the cover and of a text for publishers in the "
          "Properties panel",
          "[gui][series][cover][publishing]") {
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString first = addCover("Description 1", CoverMaterial::BLURB, CoverMaterial::BACK);
    REQUIRE(pm.setCoverState(first, QString::fromLatin1(CoverMaterial::CHOSEN)));

    gui::MaterialPage card(pm, nullptr, true);
    QStringList messages;
    QObject::connect(&card, &gui::MaterialPage::message,
                     [&messages](const QString& text) { messages << text; });
    card.showMaterial(first);
    // A book that is no series: no volume
    CHECK(card.coverGroup()->isVisibleTo(&card));
    CHECK_FALSE(card.coverVolumeBox()->isVisibleTo(&card));

    // A series: the material of the book is for its first volume
    const QString dunes = addDunes(pm);
    card.showMaterial(first);
    REQUIRE(card.coverVolumeBox()->isVisibleTo(&card));
    CHECK(itemsOf(*card.coverVolumeBox()) ==
          QStringList({QStringLiteral("Whole series"), QStringLiteral("Volume 1: Sand"),
                       QStringLiteral("Volume 2: Dunes")}));
    CHECK(card.coverVolumeBox()->currentText() == QStringLiteral("Volume 1: Sand"));

    // A new one is for the current volume, chosen there apart from the first volume's
    REQUIRE(pm.setCurrentBook(dunes));
    const QString second = addCover("Description 2", CoverMaterial::BLURB, CoverMaterial::BACK);
    REQUIRE(pm.setCoverState(second, QString::fromLatin1(CoverMaterial::CHOSEN)));
    CHECK(pm.findElement(first)->cover.state == CoverMaterial::CHOSEN);
    card.showMaterial(second);
    CHECK(card.coverVolumeBox()->currentText() == QStringLiteral("Volume 2: Dunes"));

    // Given to the second volume: the one chosen there is a draft again
    card.showMaterial(first);
    choose(*card.coverVolumeBox(), 2);
    CHECK(pm.findElement(first)->bookId == dunes);
    CHECK(pm.findElement(first)->cover.state == CoverMaterial::CHOSEN);
    CHECK(pm.findElement(second)->cover.state.isEmpty());
    REQUIRE(messages.size() == 1);
    CHECK(messages.at(0) == QStringLiteral("“Description 1” is now for: Volume 2: Dunes · "
                                           "“Description 2” is a draft again"));
    // ...and to the whole series
    choose(*card.coverVolumeBox(), 0);
    CHECK(pm.findElement(first)->bookId.isEmpty());
    REQUIRE(messages.size() == 2);
    CHECK(messages.at(1) == QStringLiteral("“Description 1” is now for: Whole series"));

    // A text for publishers: its volume, and the set of its volume
    const QString pitch = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence 1"), QStringLiteral("The dune moves."));
    REQUIRE(pm.setPublishingState(pitch, QString::fromLatin1(PublishingTexts::CHOSEN)));
    gui::PublishingTextBox box(pm);
    int changed = 0;
    QObject::connect(&box, &gui::PublishingTextBox::publishingChanged,
                     [&changed]() { ++changed; });
    box.showText(pitch, nullptr);
    REQUIRE(box.volumeBox()->isVisibleTo(&box));
    CHECK(box.volumeBox()->currentText() == QStringLiteral("Volume 2: Dunes"));
    CHECK(box.inSetLabel()->text() == QStringLiteral("In the set: yes – this version"));
    choose(*box.volumeBox(), 1);
    CHECK(pm.findElement(pitch)->bookId == pm.project()->books.at(0).id);
    CHECK(changed == 1);
    // In the set of the first volume, not of the current one
    CHECK(box.inSetLabel()->text() == QStringLiteral("In the set: yes – this version"));
    const QList<core::PublishingSetPart> current = pm.publishingSetParts();
    CHECK(std::none_of(current.cbegin(), current.cend(), [&pitch](const auto& part) {
        return part.elementId == pitch;
    }));
}

TEST_CASE("Series: the volume of a material in the Properties panel has a key of its own, in "
          "English and in Polish",
          "[gui][series][cover][publishing]") {
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString blurb = addCover("Description 1", CoverMaterial::BLURB, CoverMaterial::BACK);
    const QString pitch = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence 1"), QStringLiteral("The dune moves."));
    (void)addDunes(pm);
    const auto check = [&pm, &blurb, &pitch]() {
        QStringList texts;
        gui::MaterialPage card(pm, nullptr, true);
        card.showMaterial(blurb);
        const QLabel* label = labelOf(card, card.coverVolumeBox());
        REQUIRE(label != nullptr);
        texts << label->text();
        test::checkKeys(card);
        gui::PublishingTextBox box(pm);
        box.showText(pitch, nullptr);
        label = labelOf(box, box.volumeBox());
        REQUIRE(label != nullptr);
        texts << label->text();
        test::checkKeys(box);
        return texts;
    };
    CHECK(check() == QStringList({QStringLiteral("&Volume:"), QStringLiteral("&Volume:")}));
    const test::Polish polish;
    REQUIRE(polish.loaded());
    CHECK(check() == QStringList({QStringLiteral("&Tom:"), QStringLiteral("&Tom:")}));
}

TEST_CASE("Series: the list of the cover in groups of each volume, with the column Volume, and "
          "the plan of the current volume",
          "[gui][series][cover][materials]") {
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString first = addCover("Description 1", CoverMaterial::BLURB, CoverMaterial::BACK);
    const QString tagline = addCover("Tagline 1", CoverMaterial::TAGLINE, CoverMaterial::FRONT);
    const QString dunes = addDunes(pm);
    REQUIRE(pm.setCurrentBook(dunes));
    const QString second = addCover("Description 2", CoverMaterial::BLURB, CoverMaterial::BACK);
    const QString series =
        addCover("Description of the series", CoverMaterial::BLURB, CoverMaterial::BACK);
    REQUIRE(pm.setMaterialBook(series, QString()));

    test::Window window;
    gui::MaterialListCoordinator lists(pm, window.tabs, &window.navigator, &window.statusBar,
                                       [](bool) {});
    MaterialListPanel* panel = lists.openList(COVER_LIST, false);
    REQUIRE(panel != nullptr);

    // The groups: the whole series first, then each volume, each under the row of its volume
    // by its kinds
    panel->showGroups(true);
    CHECK(headingsOf(*panel) ==
          QStringList({QStringLiteral("Whole series · 1"), QStringLiteral("Cover descriptions · 1"),
                       QStringLiteral("Volume 1: Sand · 2"),
                       QStringLiteral("Cover descriptions · 1"), QStringLiteral("Taglines · 1"),
                       QStringLiteral("Volume 2: Dunes · 1"),
                       QStringLiteral("Cover descriptions · 1")}));
    // Kind: one kind in each volume
    panel->setPartFilter(QStringLiteral("tagline"));
    CHECK(headingsOf(*panel) ==
          QStringList({QStringLiteral("Volume 1: Sand · 2"), QStringLiteral("Taglines · 1")}));
    panel->setPartFilter(QString());

    // The column Volume
    const QString volume = QStringLiteral("Volume");
    CHECK(cellOf(*panel, first, volume) == QStringLiteral("Volume 1: Sand"));
    CHECK(cellOf(*panel, second, volume) == QStringLiteral("Volume 2: Dunes"));
    CHECK(cellOf(*panel, series, volume) == QStringLiteral("Whole series"));

    // The plan: the cover of the current volume, with the materials of the whole series
    panel->showPlan(true);
    gui::CoverPlan* plan = panel->plan();
    REQUIRE(plan != nullptr);
    CHECK(plan->hasCard(second));
    CHECK(plan->hasCard(series));
    CHECK_FALSE(plan->hasCard(first));
    CHECK_FALSE(plan->hasCard(tagline));
    // Another volume current: its cover
    REQUIRE(pm.setCurrentBook(pm.project()->books.at(0).id));
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents();
    }
    CHECK(plan->hasCard(first));
    CHECK(plan->hasCard(tagline));
    CHECK(plan->hasCard(series));
    CHECK_FALSE(plan->hasCard(second));
    panel->showPlan(false);
}

TEST_CASE("Series: a volume of the list of the cover collapses and expands, with the mouse, the "
          "keys and its menu",
          "[gui][series][cover][materials]") {
    // Test of 22.11 (part 52): the volumes of the list of materials did not collapse
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString first = addCover("Description 1", CoverMaterial::BLURB, CoverMaterial::BACK);
    const QString tagline = addCover("Tagline 1", CoverMaterial::TAGLINE, CoverMaterial::FRONT);
    const QString dunes = addDunes(pm);
    REQUIRE(pm.setCurrentBook(dunes));
    const QString second = addCover("Description 2", CoverMaterial::BLURB, CoverMaterial::BACK);
    REQUIRE(pm.setCurrentBook(pm.project()->books.at(0).id));

    test::Window window;
    gui::MaterialListCoordinator lists(pm, window.tabs, &window.navigator, &window.statusBar,
                                       [](bool) {});
    MaterialListPanel* panel = lists.openList(COVER_LIST, false);
    REQUIRE(panel != nullptr);
    panel->showGroups(true);
    window.window.resize(900, 600);
    window.window.show();
    QTableView* table = panel->table();
    const QSortFilterProxyModel* rows = panel->rows();

    // The rows shown, by their headings or the ids of their materials
    const auto shown = [table, rows]() {
        QStringList texts;
        for (int row = 0; row < rows->rowCount(); ++row) {
            if (table->isRowHidden(row)) {
                continue;
            }
            const QModelIndex index = rows->index(row, 0);
            const QString heading = index.data(MaterialListPanel::HEADING_ROLE).toString();
            texts << (heading.isEmpty() ? index.data(MaterialListPanel::ID_ROLE).toString()
                                        : heading);
        }
        return texts;
    };
    const auto rowOf = [rows](const QString& heading) {
        for (int row = 0; row < rows->rowCount(); ++row) {
            if (rows->index(row, 0).data(MaterialListPanel::HEADING_ROLE).toString() == heading) {
                return rows->index(row, 0);
            }
        }
        return QModelIndex();
    };
    const QString sand = QStringLiteral("Volume 1: Sand · 2");
    const QString ofDunes = QStringLiteral("Volume 2: Dunes · 1");
    const QStringList all = {sand,
                             QStringLiteral("Cover descriptions · 1"),
                             first,
                             QStringLiteral("Taglines · 1"),
                             tagline,
                             ofDunes,
                             QStringLiteral("Cover descriptions · 1"),
                             second};
    CHECK(shown() == all);
    const QModelIndex sandRow = rowOf(sand);
    REQUIRE(sandRow.isValid());
    CHECK(sandRow.data(MaterialListPanel::COLLAPSED_ROLE) == QVariant(false));
    CHECK(sandRow.flags().testFlag(Qt::ItemIsSelectable));
    CHECK(sandRow.data(Qt::AccessibleTextRole).toString() == sand + QStringLiteral(", expanded"));
    CHECK_FALSE(rowOf(QStringLiteral("Taglines · 1")).flags().testFlag(Qt::ItemIsSelectable));

    // A click on the row of the volume collapses it and expands it
    const QPoint place = table->visualRect(sandRow).center();
    click(table->viewport(), place);
    CHECK(panel->isVolumeCollapsed(1));
    CHECK(shown() == QStringList({sand, ofDunes, QStringLiteral("Cover descriptions · 1"), second}));
    CHECK(rowOf(sand).data(MaterialListPanel::COLLAPSED_ROLE) == QVariant(true));
    CHECK(rowOf(sand).data(Qt::AccessibleTextRole).toString() == sand + QStringLiteral(", collapsed"));
    click(table->viewport(), table->visualRect(rowOf(sand)).center());
    CHECK_FALSE(panel->isVolumeCollapsed(1));
    CHECK(shown() == all);

    // The keys on the row of the volume: Left collapses it, Right expands it, Space and Enter
    // do one or the other; the row stays chosen
    table->setFocus();
    table->setCurrentIndex(rowOf(ofDunes));
    press(table, Qt::Key_Left);
    CHECK(panel->isVolumeCollapsed(2));
    CHECK(shown() == QStringList({sand, QStringLiteral("Cover descriptions · 1"), first,
                                  QStringLiteral("Taglines · 1"), tagline, ofDunes}));
    CHECK(table->currentIndex().data(MaterialListPanel::HEADING_ROLE).toString() == ofDunes);
    press(table, Qt::Key_Left);
    CHECK(panel->isVolumeCollapsed(2));
    press(table, Qt::Key_Right);
    CHECK_FALSE(panel->isVolumeCollapsed(2));
    press(table, Qt::Key_Space);
    CHECK(panel->isVolumeCollapsed(2));
    press(table, Qt::Key_Return);
    CHECK_FALSE(panel->isVolumeCollapsed(2));
    // The row of a volume is no material: nothing to open, no material chosen
    CHECK(panel->currentMaterial().isEmpty());

    // Collapsed with the keys in it, the keys go to the row of the volume
    table->setCurrentIndex(rows->index(rows->rowCount() - 1, 0));
    REQUIRE(panel->currentMaterial() == second);
    panel->setVolumeCollapsed(2, true);
    CHECK(table->currentIndex().data(MaterialListPanel::HEADING_ROLE).toString() == ofDunes);

    // A material chosen elsewhere (the Navigator, the plan) opens its volume
    CHECK(panel->selectMaterial(second));
    CHECK_FALSE(panel->isVolumeCollapsed(2));
    CHECK(panel->currentMaterial() == second);

    // The plan keeps the materials of a collapsed volume
    panel->setVolumeCollapsed(1, true);
    panel->showPlan(true);
    REQUIRE(panel->plan() != nullptr);
    CHECK(panel->plan()->hasCard(first));
    CHECK(panel->plan()->hasCard(tagline));
    panel->showPlan(false);
    CHECK(panel->isVolumeCollapsed(1));
    CHECK(shown().mid(0, 2) == QStringList({sand, ofDunes}));

    // The menu of the row of a volume
    table->setCurrentIndex(rowOf(sand));
    {
        test::ContextMenuReader reader(panel);
        reader.chooseItem(QStringLiteral("Expand All Vol&umes"));
        emit table->customContextMenuRequested(table->visualRect(rowOf(sand)).center());
        REQUIRE(reader.menusRead() == 1);
        QStringList texts;
        for (const test::MenuItem& item : reader.items()) {
            if (!item.separator) {
                texts << item.text;
            }
        }
        CHECK(texts.mid(0, 3) == QStringList({QStringLiteral("&Expand Volume"),
                                              QStringLiteral("Collapse &All Volumes"),
                                              QStringLiteral("Expand All Vol&umes")}));
    }
    CHECK_FALSE(panel->isVolumeCollapsed(1));
    CHECK(shown() == all);

    // Without the groups nothing is collapsed
    panel->setVolumeCollapsed(1, true);
    panel->showGroups(false);
    for (int row = 0; row < rows->rowCount(); ++row) {
        CHECK_FALSE(table->isRowHidden(row));
    }
    panel->showGroups(true);
    CHECK(shown().mid(0, 2) == QStringList({sand, ofDunes}));
}

TEST_CASE("Series: the set for the publisher and the export of each volume, with the texts of "
          "the whole series",
          "[gui][series][publishing][export]") {
    using gui::dialogs::ExportBookDialog;
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString pitch1 = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence 1"), QStringLiteral("The well runs dry."));
    const QString blurb1 = test::addCoverText(QStringLiteral("Description 1"),
                                              CoverMaterial::BLURB,
                                              QStringLiteral("A road through the sand."));
    REQUIRE(pm.setPublishingState(pitch1, QString::fromLatin1(PublishingTexts::CHOSEN)));
    REQUIRE(pm.setCoverState(blurb1, QString::fromLatin1(CoverMaterial::CHOSEN)));
    const QString sand = pm.book()->id;
    const QString dunes = addDunes(pm);
    REQUIRE(pm.setCurrentBook(dunes));
    const QString pitch2 = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence 2"), QStringLiteral("The dune moves."));
    REQUIRE(pm.setPublishingState(pitch2, QString::fromLatin1(PublishingTexts::CHOSEN)));
    const QString synopsis = test::addPublishingText(
        PublishingTexts::SYNOPSIS, QStringLiteral("Synopsis of the series"),
        QStringLiteral("Three volumes of sand."));
    REQUIRE(pm.setMaterialBook(synopsis, QString()));
    REQUIRE(pm.setPublishingState(synopsis, QString::fromLatin1(PublishingTexts::CHOSEN)));

    // The set of the current volume: its own versions and those of the whole series
    const QList<gui::PublishingSetItem> items = gui::PublishingSetContents::itemsOf(pm);
    CHECK(elementOf(items, PublishingTexts::PITCH) == pitch2);
    CHECK(elementOf(items, PublishingTexts::SYNOPSIS) == synopsis);
    CHECK(elementOf(items, PublishingTexts::BLURB).isEmpty());
    for (const gui::PublishingSetItem& item : items) {
        if (item.part == QLatin1String(PublishingTexts::PITCH)) {
            REQUIRE(item.versions.size() == 1);
            CHECK(item.versions.first().id == pitch2);
        }
    }

    test::Window window;
    const gui::BookExportCoordinator coordinator(
        pm, test::commands(), window.tabs, []() -> gui::EditorPanel* { return nullptr; },
        nullptr, nullptr, &window.window);

    // The window of the export: the set of each volume
    const ExportBookDialog::Book ofDunes = coordinator.dialogBook(dunes);
    CHECK(ofDunes.setTitle == QStringLiteral("Dunes – set for the publisher"));
    CHECK(ofDunes.setParts == 2);
    CHECK(ofDunes.description.isEmpty());
    const ExportBookDialog::Book ofSand = coordinator.dialogBook(sand);
    CHECK(ofSand.setTitle == QStringLiteral("Sand – set for the publisher"));
    CHECK(ofSand.setParts == 3);
    CHECK(ofSand.description == QStringLiteral("A road through the sand."));

    // The set of another volume than the current one, under its title
    ExportBookDialog::Choice choice;
    choice.content = ExportBookDialog::Content::Set;
    choice.format = editor::ExportFormat::Markdown;
    choice.volume = sand;
    const editor::BookExport book = coordinator.bookExport(choice);
    REQUIRE(book.items.size() == 4);
    CHECK(book.items[0].title == QStringLiteral("Sand"));
    CHECK(book.items[1].title == QStringLiteral("Sentence about the book"));
    CHECK(book.items[1].kml.contains(QStringLiteral("The well runs dry.")));
    CHECK(book.items[2].kml.contains(QStringLiteral("A road through the sand.")));
    CHECK(book.items[3].kml.contains(QStringLiteral("Three volumes of sand.")));
    choice.volume = dunes;
    const editor::BookExport other = coordinator.bookExport(choice);
    REQUIRE(other.items.size() == 3);
    CHECK(other.items[0].title == QStringLiteral("Dunes"));
    CHECK(other.items[1].kml.contains(QStringLiteral("The dune moves.")));

    // An e-book of each volume: the description of its cover
    ExportBookDialog::Choice epub;
    epub.format = editor::ExportFormat::Epub;
    epub.formatId = QStringLiteral("epub");
    epub.volume = sand;
    CHECK(coordinator.bookExport(epub).epub.description ==
          QStringLiteral("A road through the sand."));
    epub.volume = dunes;
    CHECK(coordinator.bookExport(epub).epub.description.isEmpty());
}
