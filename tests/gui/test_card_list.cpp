// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_list.cpp
/// @brief The lists of the cards of the characters, the locations and the items on their tabs:
/// the columns of the fields, the rows, the search by the forms of the names, the sorting by the
/// values to choose, F2 in a cell, the menu of a card, the tiles and the Navigator that opens
/// them

#include "document_window.h"
#include "test_support/window_parts.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/card_list.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"

#include <catch2/catch_test_macros.hpp>

#include <QComboBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <algorithm>
#include <iterator>

namespace kalahari {
using gui::CardList;
using gui::MaterialList;
using gui::MaterialListPanel;

namespace {

/// The list of the cards of kind @p kindId of the Base: "kalahari.base:character"
QString listOf(const char* kindId) {
    return QStringLiteral("kalahari.base:") + QString::fromLatin1(kindId);
}

/// A new card @p title of kind @p kindId of the Base in the Workshop of the open book
QString addCard(const char* kindId, const QString& title) {
    auto& pm = test::book();
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QLatin1String(kindId));
    REQUIRE(kind);
    const QString id = pm.addElement(kind, title, core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// The column @p columnId of @p list in its model
int columnOf(const QString& list, const QString& columnId) {
    const QList<gui::MaterialColumn> columns = MaterialList::columnsOf(test::book(), list);
    const auto found =
        std::find_if(columns.cbegin(), columns.cend(), [&columnId](const gui::MaterialColumn& c) {
            return c.id == columnId;
        });
    // REQUIRE and not FAIL before a return, which a compiler finds unreachable
    INFO("No column " << columnId.toStdString());
    REQUIRE(found != columns.cend());
    return static_cast<int>(std::distance(columns.cbegin(), found));
}

/// The texts of column @p columnId of the rows @p panel shows, in their order
QStringList columnTexts(const MaterialListPanel& panel, const QString& columnId) {
    const int column = columnOf(panel.list(), columnId);
    const QSortFilterProxyModel* rows = panel.rows();
    QStringList texts;
    for (int row = 0; row < rows->rowCount(); ++row) {
        texts << rows->index(row, column).data().toString();
    }
    return texts;
}

/// The column of field @p fieldId of the cards
QString fieldColumn(const char* fieldId) {
    return QString::fromLatin1(CardList::FIELD_PREFIX) + QString::fromLatin1(fieldId);
}

/// Key @p key with @p modifiers sent to @p widget, as it gets it when it has the keys
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

/// What the table and the delegate do after a key, queued: the editor closes, then the card
/// gets the text
void settle() {
    for (int round = 0; round < 3; ++round) {
        QCoreApplication::processEvents();
    }
}

/// Make the cell of card @p cardId in column @p columnId the current one and press F2 there
void editCell(MaterialListPanel& panel, const QString& cardId, const QString& columnId) {
    REQUIRE(panel.selectMaterial(cardId));
    QTableView* table = panel.table();
    const QModelIndex index =
        table->currentIndex().siblingAtColumn(columnOf(panel.list(), columnId));
    table->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    press(table, Qt::Key_F2);
}

/// The editor of the cell being edited in @p panel, of type @p Editor
template <typename Editor>
Editor* editorOf(const MaterialListPanel& panel, const char* name) {
    auto* editor = panel.table()->viewport()->findChild<Editor*>(QLatin1String(name));
    REQUIRE(editor != nullptr);
    return editor;
}

/// The texts of the items of @p menu, without the separators
QStringList menuTexts(const QMenu& menu) {
    QStringList texts;
    for (const QAction* action : menu.actions()) {
        if (!action->isSeparator()) {
            texts << action->text();
        }
    }
    return texts;
}

/// The cards of the test: Anna, the main character, of Warsaw, with her brother Jan, in the
/// chapter; Ewa, a minor one
struct Cast {
    test::OpenNovel novel{QStringLiteral("Cast")};
    QString anna = addCard("character", QStringLiteral("Anna Wolska"));
    QString jan = addCard("character", QStringLiteral("Jan"));
    QString ewa = addCard("character", QStringLiteral("Ewa"));
    QString warsaw = addCard("location", QStringLiteral("Warsaw"));

    Cast() {
        auto& pm = novel.pm;
        REQUIRE(pm.setCardValue(anna, QStringLiteral("role"), QStringLiteral("main")));
        REQUIRE(pm.setCardValue(anna, QStringLiteral("age"), QStringLiteral("34")));
        REQUIRE(pm.setCardFieldText(anna, QStringLiteral("origin"), QStringLiteral("Warsaw")));
        REQUIRE(pm.setDescription(anna, QStringLiteral("A painter who\nlost her sight.")));
        REQUIRE(pm.setCardValue(jan, QStringLiteral("role"), QStringLiteral("supporting")));
        REQUIRE(pm.setCardValue(ewa, QStringLiteral("role"), QStringLiteral("minor")));
        REQUIRE(pm.addRelation(anna, jan, QStringLiteral("sibling"), QString()));
        const core::ProjectElement* card = pm.findElement(anna);
        REQUIRE(card != nullptr);
        QList<core::ElementLink> links = card->links;
        core::ElementLink chapter;
        chapter.to = novel.chapterId;
        chapter.role = QStringLiteral("appears");
        links.append(chapter);
        REQUIRE(pm.setLinks(anna, links));
    }
};

}  // namespace

TEST_CASE("Card list: the columns are the fields of the kind, the rows the cards",
          "[gui][cards][materials]") {
    gui::registerAllIcons();
    const Cast cast;
    auto& pm = cast.novel.pm;
    const QString characters = listOf("character");

    // A list of each kind of cards, with its tiles; the materials are no cards
    CHECK(CardList::isCardList(characters));
    CHECK(CardList::isCardList(listOf("location")));
    CHECK(CardList::isCardList(listOf("item")));
    CHECK_FALSE(CardList::isCardList(listOf("quote")));
    CHECK_FALSE(CardList::isCardList(QString()));
    CHECK(MaterialList::hasTiles(characters));
    CHECK(MaterialList::nameOf(characters) == QStringLiteral("Characters"));

    // The name, the fields "In the list", In Short and the numbers of the chapters and the
    // relations; the other fields hidden at first
    CHECK(MaterialList::shownColumns(pm, characters) ==
          QStringList{"title", "cardField:role", "cardField:age", "cardField:occupation",
                      "description", "chapters", "relations"});
    const QList<gui::MaterialColumn> columns = MaterialList::columnsOf(pm, characters);
    const auto appearance =
        std::find_if(columns.cbegin(), columns.cend(), [](const gui::MaterialColumn& column) {
            return column.id == QStringLiteral("cardField:appearance");
        });
    REQUIRE(appearance != columns.cend());
    CHECK_FALSE(appearance->shown);
    CHECK(appearance->editable);
    CHECK(appearance->title == QStringLiteral("Appearance"));
    CHECK_FALSE(columns.at(columnOf(characters, QStringLiteral("chapters"))).editable);
    CHECK_FALSE(columns.at(columnOf(characters, QStringLiteral("title"))).editable);

    // What each card shows: the names of the values to choose and of the card a field names
    const QList<gui::MaterialRow> rows = MaterialList::rowsOf(pm, characters);
    REQUIRE(rows.size() == 3);
    const gui::MaterialRow& anna = rows.at(0);
    CHECK(anna.id == cast.anna);
    CHECK(anna.card);
    CHECK(anna.texts.value(QStringLiteral("title")) == QStringLiteral("Anna Wolska"));
    CHECK(anna.texts.value(fieldColumn("role")) == QStringLiteral("main"));
    CHECK(anna.texts.value(fieldColumn("age")) == QStringLiteral("34"));
    CHECK(anna.texts.value(fieldColumn("origin")) == QStringLiteral("Warsaw"));
    CHECK(anna.texts.value(QStringLiteral("description")) ==
          QStringLiteral("A painter who lost her sight."));
    CHECK(anna.summary == QStringLiteral("A painter who lost her sight."));
    CHECK(anna.texts.value(QStringLiteral("chapters")) == QStringLiteral("1"));
    CHECK(anna.texts.value(QStringLiteral("relations")) == QStringLiteral("2"));
    CHECK(anna.texts.value(QStringLiteral("details")) == QStringLiteral("Character · main"));
    CHECK(anna.color == pm.findElement(cast.anna)->card.color);
    CHECK_FALSE(anna.color.isEmpty());
    CHECK(anna.copy == QStringLiteral("Anna Wolska"));
    // Jan: the relation of the sibling; Ewa: none, in no chapter
    CHECK(rows.at(1).texts.value(QStringLiteral("relations")) == QStringLiteral("1"));
    CHECK(rows.at(2).texts.value(QStringLiteral("chapters")) == QStringLiteral("0"));
    CHECK(rows.at(2).texts.value(QStringLiteral("details")) ==
          QStringLiteral("Character · minor"));

    // A field of the writer's is a column too, hidden at first
    REQUIRE(pm.setFields(cast.ewa, {core::ElementField{QStringLiteral("Nickname"),
                                                       QStringLiteral("Evie"), {}}}));
    const QList<gui::MaterialColumn> own = MaterialList::columnsOf(pm, characters);
    CHECK(own.last().id == QStringLiteral("field:Nickname"));
    CHECK_FALSE(own.last().shown);
    CHECK_FALSE(own.last().editable);

    // The locations have their own list
    CHECK(MaterialList::rowsOf(pm, listOf("location")).size() == 1);
}

TEST_CASE("Card list: the search finds the forms of the names, the values sort in their order",
          "[gui][cards][materials]") {
    gui::registerAllIcons();
    const Cast cast;
    auto& pm = cast.novel.pm;
    // The forms of Anna's name and her other name, in the book's dictionary
    core::DictionaryName name = core::makeName(QStringLiteral("Anna Wolska"),
                                               core::NameKind::Character,
                                               core::NameDeclension(QStringLiteral("pl")));
    name.card = cast.anna;
    core::NameVariant ania;
    ania.text = QStringLiteral("Ania");
    name.variants.append(ania);
    REQUIRE(pm.changeBookDictionary(
        [&name](core::BookDictionary& dictionary) { dictionary.putName(name); }));

    MaterialListPanel panel(pm, listOf("character"));
    panel.resize(900, 500);
    QSortFilterProxyModel* rows = panel.rows();
    REQUIRE(rows->rowCount() == 3);
    const QString title = QStringLiteral("title");

    // The search: a form of the name, another name, a field, In Short
    panel.searchField()->setText(QStringLiteral("wolskiej"));
    CHECK(columnTexts(panel, title) == QStringList{QStringLiteral("Anna Wolska")});
    panel.searchField()->setText(QStringLiteral("ania"));
    CHECK(columnTexts(panel, title) == QStringList{QStringLiteral("Anna Wolska")});
    panel.searchField()->setText(QStringLiteral("warsaw"));
    CHECK(columnTexts(panel, title) == QStringList{QStringLiteral("Anna Wolska")});
    panel.searchField()->setText(QStringLiteral("painter"));
    CHECK(rows->rowCount() == 1);
    panel.searchField()->setText(QStringLiteral("supporting"));
    CHECK(columnTexts(panel, title) == QStringList{QStringLiteral("Jan")});
    panel.searchField()->clear();
    CHECK(rows->rowCount() == 3);

    // A filter of the column of the role
    panel.setColumnFilter(fieldColumn("role"), QStringLiteral("min"));
    CHECK(columnTexts(panel, title) == QStringList{QStringLiteral("Ewa")});
    panel.setColumnFilter(fieldColumn("role"), QString());

    // The role sorts in the order of its values: main, supporting, minor
    panel.table()->sortByColumn(columnOf(panel.list(), fieldColumn("role")), Qt::AscendingOrder);
    CHECK(columnTexts(panel, fieldColumn("role")) ==
          QStringList{QStringLiteral("main"), QStringLiteral("supporting"), QStringLiteral("minor")});
    // The relations sort by their number
    panel.table()->sortByColumn(columnOf(panel.list(), QStringLiteral("relations")),
                                Qt::DescendingOrder);
    CHECK(columnTexts(panel, title) ==
          QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Jan"), QStringLiteral("Ewa")});
}

TEST_CASE("Card list: F2 edits the cells of the fields and of In Short", "[gui][cards][materials]") {
    gui::registerAllIcons();
    const Cast cast;
    auto& pm = cast.novel.pm;
    MaterialListPanel panel(pm, listOf("character"));
    panel.resize(1100, 500);
    panel.show();
    QStringList renamed;
    QObject::connect(&panel, &MaterialListPanel::requestRename,
                     [&renamed](const QString& id) { renamed << id; });
    const auto valueOf = [&pm](const QString& cardId, const char* fieldId) {
        const core::ProjectElement* card = pm.findElement(cardId);
        REQUIRE(card != nullptr);
        return card->card.value(QString::fromLatin1(fieldId));
    };

    // A value to choose: its names, "–" for none
    editCell(panel, cast.ewa, fieldColumn("role"));
    auto* choice = editorOf<QComboBox>(panel, "cardCellChoice");
    CHECK(choice->itemText(0) == QStringLiteral("–"));
    CHECK(choice->currentData().toString() == QStringLiteral("minor"));
    choice->setCurrentIndex(choice->findData(QStringLiteral("background")));
    press(choice, Qt::Key_Return);
    settle();
    CHECK(valueOf(cast.ewa, "role") == QStringLiteral("background"));
    // The current cell stays where it was, the list filled again
    panel.refresh();
    CHECK(panel.table()->currentIndex().column() == columnOf(panel.list(), fieldColumn("role")));

    // A text: a line; Esc keeps the card as it was
    editCell(panel, cast.ewa, fieldColumn("age"));
    auto* line = editorOf<QLineEdit>(panel, "cardCellLine");
    line->setText(QStringLiteral("19"));
    press(line, Qt::Key_Escape);
    settle();
    CHECK(valueOf(cast.ewa, "age").isEmpty());
    editCell(panel, cast.ewa, fieldColumn("age"));
    line = editorOf<QLineEdit>(panel, "cardCellLine");
    line->setText(QStringLiteral(" 19 "));
    press(line, Qt::Key_Return);
    settle();
    CHECK(valueOf(cast.ewa, "age") == QStringLiteral("19"));

    // A field that names a card: the names of the locations, or a name typed
    editCell(panel, cast.ewa, fieldColumn("origin"));
    auto* named = editorOf<QComboBox>(panel, "cardCellCard");
    CHECK(named->isEditable());
    CHECK(named->findText(QStringLiteral("Warsaw")) >= 0);
    named->setEditText(QStringLiteral("Warsaw"));
    press(named, Qt::Key_Return);
    settle();
    CHECK(pm.cardFieldText(cast.ewa, QStringLiteral("origin")) == QStringLiteral("Warsaw"));

    // In Short: a few lines; Enter keeps it, as the other cells
    editCell(panel, cast.ewa, QStringLiteral("description"));
    auto* text = editorOf<QPlainTextEdit>(panel, "cardCellText");
    CHECK(text->height() > panel.table()->verticalHeader()->defaultSectionSize());
    text->setPlainText(QStringLiteral("Anna's friend from school."));
    press(text, Qt::Key_Return);
    settle();
    CHECK(pm.findElement(cast.ewa)->description == QStringLiteral("Anna's friend from school."));

    // The name and the numbers: F2 renames the card
    editCell(panel, cast.ewa, QStringLiteral("title"));
    CHECK(renamed == QStringList{cast.ewa});
    editCell(panel, cast.ewa, QStringLiteral("chapters"));
    CHECK(renamed == QStringList{cast.ewa, cast.ewa});
}

TEST_CASE("Card list: the menu of a card, Relation..., the tiles and the gear",
          "[gui][cards][materials]") {
    gui::registerAllIcons();
    const Cast cast;
    auto& pm = cast.novel.pm;
    MaterialListPanel panel(pm, listOf("character"));
    panel.resize(900, 500);
    QStringList linked;
    QObject::connect(&panel, &MaterialListPanel::requestLink,
                     [&linked](const QString& id) { linked << id; });

    // The menu: Edit, Relation..., Rename..., Copy, Delete..., Properties, then Add
    REQUIRE(panel.selectMaterial(cast.anna));
    QMenu menu;
    panel.fillMenu(menu);
    const QStringList texts = menuTexts(menu);
    REQUIRE(texts.size() == 7);
    CHECK(texts.at(0).startsWith(QStringLiteral("&Edit\t")));
    CHECK(menu.actions().first()->font().bold());
    CHECK(texts.at(1) == QStringLiteral("Re&lation..."));
    CHECK(texts.at(2) == QStringLiteral("Rena&me..."));
    CHECK(texts.at(3).startsWith(QStringLiteral("&Copy\t")));
    CHECK(texts.at(4).startsWith(QStringLiteral("&Delete...\t")));
    CHECK(texts.at(5).startsWith(QStringLiteral("P&roperties\t")));
    CHECK(texts.at(6).startsWith(QStringLiteral("Add Character...\t")));
    CHECK_FALSE(texts.contains(QStringLiteral("&Open")));
    for (QAction* action : menu.actions()) {
        if (action->text() == QStringLiteral("Re&lation...")) {
            action->trigger();
        }
    }
    CHECK(linked == QStringList{cast.anna});

    // The buttons say what they do with a card
    auto* link = panel.findChild<QToolButton*>(QStringLiteral("materialListLink"));
    REQUIRE(link != nullptr);
    CHECK(link->toolTip() == QStringLiteral("Relation..."));
    CHECK(link->isEnabled());
    CHECK(panel.settingsButton()->toolTip() == QStringLiteral("Card Settings"));
    CHECK(panel.table()->accessibleName() == QStringLiteral("List of cards"));

    // The portrait beside the name; the tiles: the portrait, the color of the card, its kind
    // and In Short
    const QModelIndex anna = panel.rows()->index(0, columnOf(panel.list(), QStringLiteral("title")));
    CHECK_FALSE(anna.data(Qt::DecorationRole).value<QPixmap>().isNull());
    panel.showTiles(true);
    CHECK(panel.view() == panel.tiles());
    CHECK(panel.tiles()->model()->rowCount() == 3);
    const QModelIndex tile = panel.tiles()->model()->index(0, panel.tiles()->modelColumn());
    CHECK(tile.data(MaterialListPanel::CARD_ROLE).toBool());
    CHECK_FALSE(tile.data(MaterialListPanel::TILE_ROLE).value<QPixmap>().isNull());
    CHECK(tile.data(MaterialListPanel::COLOR_ROLE).value<QColor>() ==
          QColor(pm.findElement(cast.anna)->card.color));
    CHECK(tile.data(MaterialListPanel::SUMMARY_ROLE).toString() ==
          QStringLiteral("A painter who lost her sight."));
    CHECK(tile.data(MaterialListPanel::DETAIL_ROLE).toString() ==
          QStringLiteral("Character · main"));
    QStyleOptionViewItem option;
    option.font = panel.tiles()->font();
    CHECK(panel.tiles()->itemDelegate()->sizeHint(option, tile).height() >
          panel.tiles()->itemDelegate()->sizeHint(option, tile).width() / 2);
    CHECK(MaterialList::tilesShown(panel.list()));
    panel.showTiles(false);
}

TEST_CASE("Navigator: the groups of the cards show their lists on tabs", "[gui][cards][materials]") {
    gui::registerAllIcons();
    const Cast cast;
    auto& pm = cast.novel.pm;
    test::Window window;
    const gui::MaterialListCoordinator lists{pm, window.tabs, &window.navigator, &window.statusBar,
                                       [](bool) {}};
    QObject::connect(window.navigatorPanel, &gui::NavigatorPanel::requestOpenMaterialList,
                     &lists, &gui::MaterialListCoordinator::openList);
    int cardSettings = 0;
    int materialSettings = 0;
    QObject::connect(&lists, &gui::MaterialListCoordinator::cardSettingsRequested,
                     [&cardSettings]() { ++cardSettings; });
    QObject::connect(&lists, &gui::MaterialListCoordinator::settingsRequested,
                     [&materialSettings]() { ++materialSettings; });
    QTreeWidget* tree = window.navigatorPanel->tree();
    QTreeWidgetItem* group = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() ==
            QStringLiteral("workshop:") + listOf("character")) {
            group = *it;
        }
    }
    REQUIRE(group != nullptr);
    CHECK(gui::NavigatorPanel::cardListOf(group) == listOf("character"));
    CHECK_FALSE(gui::NavigatorPanel::materialListOf(group).has_value());
    // The group keeps its cards under it, open
    CHECK(group->childCount() == 3);

    // A click opens the list of the characters on a tab
    emit window.navigatorPanel->tree()->itemClicked(group, 0);
    REQUIRE(window.tabs->count() == 1);
    CHECK(window.tabs->tabText(0) == QStringLiteral("Characters"));
    auto* panel = qobject_cast<MaterialListPanel*>(window.tabs->widget(0));
    REQUIRE(panel != nullptr);
    CHECK(panel->rows()->rowCount() == 3);

    // A field changed elsewhere: the list shows it at once
    REQUIRE(pm.setCardValue(cast.jan, QStringLiteral("age"), QStringLiteral("40")));
    QCoreApplication::processEvents();
    CHECK(columnTexts(*panel, fieldColumn("age")).contains(QStringLiteral("40")));

    // The gear: the settings of the cards
    panel->settingsButton()->click();
    CHECK(cardSettings == 1);
    CHECK(materialSettings == 0);
    window.documents.onProjectClosed();
}

}  // namespace kalahari
