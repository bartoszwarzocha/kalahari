/// @file test_material_list.cpp
/// @brief The lists of the kinds of materials on their tabs: the columns, the rows, the search,
/// the filters, the tiles, the keys, and the Navigator that opens them

#include "../editor/editor_test_utils.h"
#include "document_window.h"

#include "kalahari/core/material_kinds.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/material_page.h"

#include <catch2/catch_test_macros.hpp>

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

using namespace kalahari;
using gui::MaterialList;
using gui::MaterialListPanel;

namespace {

/// The list of the kind @p kindId of the base package: "kalahari.base:quote"
QString listOf(const char* kindId) {
    return QStringLiteral("kalahari.base:") + QString::fromLatin1(kindId);
}

/// Kind @p kindId of the base package
core::KindRef baseKind(const char* kindId) {
    return core::ProjectManager::getInstance().bookTypes().findKind(
        QStringLiteral("kalahari.base"), QString::fromLatin1(kindId));
}

/// A quote of the open book: @p words, linked with @p links
QString addQuote(const QString& title, const QString& words, const QStringList& links = {}) {
    auto& pm = core::ProjectManager::getInstance();
    core::NewMaterial quote;
    quote.title = title;
    quote.links = links;
    const QString id = pm.addMaterial(quote, baseKind(core::MaterialKinds::QUOTE));
    REQUIRE_FALSE(id.isEmpty());
    REQUIRE(pm.setDescription(id, words));
    return id;
}

/// A picture of @p size in folder @p dir, a material of the open book named @p title
QString addPicture(const QString& dir, const QString& name, const QSize& size,
                   const QString& title) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::darkGreen);
    const QString path = QDir(dir).filePath(name);
    REQUIRE(image.save(path, "PNG"));
    const QString id = core::ProjectManager::getInstance().addMaterial(
        path, baseKind(core::MaterialKinds::PICTURE), title);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// The texts of column @p columnId of the rows @p panel shows, in their order
QStringList columnTexts(const MaterialListPanel& panel, const char* columnId) {
    const QList<gui::MaterialColumn> columns = MaterialList::columnsOf(panel.list());
    int column = -1;
    for (int i = 0; i < columns.size(); ++i) {
        if (columns.at(i).id == QLatin1String(columnId)) {
            column = i;
        }
    }
    REQUIRE(column >= 0);
    const QSortFilterProxyModel* rows = panel.rows();
    QStringList texts;
    for (int row = 0; row < rows->rowCount(); ++row) {
        texts << rows->index(row, column).data().toString();
    }
    return texts;
}

/// Key @p key with @p modifiers sent to @p widget, as it gets it when it has the keys
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QCoreApplication::sendEvent(widget, &event);
}

/// Whether the window's command of @p key waits while @p widget has the keys: the widget takes
/// the key for itself
bool takesKey(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::ShortcutOverride, key, modifiers);
    event.ignore();
    QCoreApplication::sendEvent(widget, &event);
    return event.isAccepted();
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

/// The node of the Navigator with the id or the type @p idOrType
QTreeWidgetItem* navigatorItem(const QTreeWidget* tree, const QString& idOrType) {
    for (QTreeWidgetItemIterator it(const_cast<QTreeWidget*>(tree)); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == idOrType ||
            (*it)->data(0, Qt::UserRole + 1).toString() == idOrType) {
            return *it;
        }
    }
    return nullptr;
}

/// The node of kind @p kindId of the Materials in the Navigator
QTreeWidgetItem* kindItem(const QTreeWidget* tree, const char* kindId) {
    return navigatorItem(tree, QStringLiteral("workshop:") + listOf(kindId));
}

/// The main window's tabs and panels with the lists of the materials, as MainWindow connects
/// them
struct ListWindow {
    test::Window window;
    gui::MaterialListCoordinator lists{window.tabs, &window.navigator, &window.statusBar,
                                       [this](bool keys) { propertiesShown << keys; }};
    QList<bool> propertiesShown;  ///< Each time the Properties panel came to the front

    ListWindow() {
        QObject::connect(window.navigatorPanel, &gui::NavigatorPanel::requestOpenMaterialList,
                         &lists, &gui::MaterialListCoordinator::openList);
    }

    /// The list on tab @p index
    [[nodiscard]] MaterialListPanel* list(int index) const {
        auto* panel = qobject_cast<MaterialListPanel*>(window.tabs->widget(index));
        REQUIRE(panel != nullptr);
        return panel;
    }

    /// A click on @p item of the Navigator
    void click(QTreeWidgetItem* item) const {
        REQUIRE(item != nullptr);
        emit window.navigatorPanel->tree()->itemClicked(item, 0);
    }

    /// Enter on @p item of the Navigator
    void activate(QTreeWidgetItem* item) const {
        REQUIRE(item != nullptr);
        emit window.navigatorPanel->tree()->itemActivated(item, 0);
    }
};

} // namespace

TEST_CASE("Materials list: the lists of a book, their names, columns and sorting",
          "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;

    // The working notes and the kinds of materials, as the Navigator has them in Materials
    const QStringList kinds = MaterialList::kinds();
    CHECK(kinds.contains(listOf(core::MaterialKinds::WORK_NOTE)));
    CHECK(kinds.contains(listOf(core::MaterialKinds::QUOTE)));
    CHECK(kinds.contains(listOf(core::MaterialKinds::PICTURE)));
    CHECK(kinds.contains(listOf(core::MaterialKinds::DOCUMENT)));
    CHECK_FALSE(kinds.contains(QString::fromLatin1(MaterialList::ALL)));
    CHECK(MaterialList::nameOf(QString::fromLatin1(MaterialList::ALL)) ==
          QStringLiteral("All Materials"));
    CHECK(MaterialList::nameOf(listOf(core::MaterialKinds::QUOTE)) == QStringLiteral("Quotes"));
    CHECK(MaterialList::iconIdOf(QString()) == QStringLiteral("structure.materials"));

    // The pictures, the documents and the publications have tiles too
    CHECK(MaterialList::hasTiles(listOf(core::MaterialKinds::PICTURE)));
    CHECK(MaterialList::hasTiles(listOf(core::MaterialKinds::DOCUMENT)));
    CHECK(MaterialList::hasTiles(listOf(core::MaterialKinds::PUBLICATION)));
    CHECK_FALSE(MaterialList::hasTiles(listOf(core::MaterialKinds::QUOTE)));
    CHECK_FALSE(MaterialList::hasTiles(QString()));

    // Each list has the columns of its kind; some are hidden at first
    CHECK(MaterialList::shownColumns(listOf(core::MaterialKinds::QUOTE)) ==
          QStringList{"quote", "links"});
    CHECK(MaterialList::shownColumns(QString()) ==
          QStringList{"type", "title", "details", "links"});
    CHECK(MaterialList::shownColumns(listOf(core::MaterialKinds::PICTURE)) ==
          QStringList{"thumbnail", "title", "format", "dimensions", "origin", "links"});
    CHECK(MaterialList::shownColumns(listOf(core::MaterialKinds::WORK_NOTE)) ==
          QStringList{"title", "words", "status", "modified", "links"});
    CHECK(MaterialList::firstSortOf(listOf(core::MaterialKinds::QUOTE)) == QStringLiteral("quote"));
    CHECK(MaterialList::firstSortOf(QString()) == QStringLiteral("type"));
    CHECK(MaterialList::firstSortOf(listOf(core::MaterialKinds::PICTURE)) ==
          QStringLiteral("title"));

    // A field of the writer's is a column too, hidden at first
    const QString quote = addQuote(QStringLiteral("Desert"), QStringLiteral("The sand is warm."));
    REQUIRE(pm.setFields(quote, {core::ElementField{QStringLiteral("Author"),
                                                    QStringLiteral("T. Lawrence"), {}}}));
    const QList<gui::MaterialColumn> columns =
        MaterialList::columnsOf(listOf(core::MaterialKinds::QUOTE));
    REQUIRE_FALSE(columns.isEmpty());
    CHECK(columns.last().id == QStringLiteral("field:Author"));
    CHECK(columns.last().title == QStringLiteral("Author"));
    CHECK_FALSE(columns.last().shown);

    // The writer's columns, in the writer's order, are kept for the list
    MaterialList::setShownColumns(listOf(core::MaterialKinds::QUOTE),
                                  {"title", "quote", "field:Author", "nothing"});
    CHECK(MaterialList::shownColumns(listOf(core::MaterialKinds::QUOTE)) ==
          QStringList{"title", "quote", "field:Author"});
    CHECK(MaterialList::shownColumns(QString()) ==
          QStringList{"type", "title", "details", "links"});

    // Copy gives the words of a quote; a document kind by its format
    CHECK(MaterialList::copyTextOf(quote) == QStringLiteral("The sand is warm."));
    CHECK(MaterialList::documentKindOf(QStringLiteral("PDF")) == QStringLiteral("PDF"));
    CHECK(MaterialList::documentKindOf(QStringLiteral("XLSX")) ==
          QStringLiteral("spreadsheet"));
}

TEST_CASE("Materials list: the rows, the search, the filters and the sorting",
          "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    const QString chapter = novel.chapterId;
    const QString sand = addQuote(QStringLiteral("Sand"), QStringLiteral("Sand everywhere"),
                                  {chapter});
    const QString water = addQuote(QStringLiteral("Water"), QStringLiteral("Water is life"));
    const QString camel =
        addQuote(QStringLiteral("Camel"), QStringLiteral("A camel kneels"), {chapter});

    MaterialListPanel panel(listOf(core::MaterialKinds::QUOTE));
    panel.resize(900, 500);
    QSortFilterProxyModel* rows = panel.rows();
    REQUIRE(rows->rowCount() == 3);

    // Sorted by the words at first; a quote without links shows a dash
    CHECK(columnTexts(panel, MaterialList::QUOTE) ==
          QStringList{"A camel kneels", "Sand everywhere", "Water is life"});
    CHECK(columnTexts(panel, MaterialList::LINKS).at(2) == QStringLiteral("–"));
    CHECK(columnTexts(panel, MaterialList::LINKS).at(0).startsWith(QStringLiteral("The Storm")));
    auto* heading = panel.findChild<QLabel*>(QStringLiteral("materialListHeading"));
    REQUIRE(heading != nullptr);
    CHECK(heading->text().contains(QStringLiteral("Quotes")));
    auto* count = panel.findChild<QLabel*>(QStringLiteral("materialListCount"));
    REQUIRE(count != nullptr);
    CHECK(count->text() == QStringLiteral("Shown: 3 of 3"));
    auto* sorting = panel.findChild<QLabel*>(QStringLiteral("materialListSorting"));
    REQUIRE(sorting != nullptr);
    CHECK(sorting->text() == QStringLiteral("Sorted by Quote, ascending"));

    // A click on the header sorts the other way
    panel.table()->sortByColumn(0, Qt::DescendingOrder);
    CHECK(columnTexts(panel, MaterialList::QUOTE) ==
          QStringList{"Water is life", "Sand everywhere", "A camel kneels"});
    CHECK(sorting->text() == QStringLiteral("Sorted by Quote, descending"));
    panel.table()->sortByColumn(0, Qt::AscendingOrder);

    // The search finds words of any column, in any order of the words
    panel.searchField()->setText(QStringLiteral("everywhere SAND"));
    CHECK(columnTexts(panel, MaterialList::QUOTE) == QStringList{"Sand everywhere"});
    CHECK(count->text() == QStringLiteral("Shown: 1 of 3"));
    auto* clear = panel.findChild<QToolButton*>(QStringLiteral("materialListClearFilters"));
    REQUIRE(clear != nullptr);
    CHECK_FALSE(clear->isHidden());
    // Esc clears it, then gives the keys to the list
    press(panel.searchField(), Qt::Key_Escape);
    CHECK(panel.searchField()->text().isEmpty());
    CHECK(rows->rowCount() == 3);
    CHECK(clear->isHidden());

    // Linked with an element: the elements the quotes are linked with, by title
    QComboBox* linked = panel.linkedWithBox();
    REQUIRE(linked->count() == 2);
    CHECK(linked->itemText(0) == QStringLiteral("any element"));
    CHECK(linked->itemData(1).toString() == chapter);
    linked->setCurrentIndex(1);
    CHECK(columnTexts(panel, MaterialList::QUOTE) ==
          QStringList{"A camel kneels", "Sand everywhere"});
    linked->setCurrentIndex(0);

    // Without links
    panel.withoutLinksBox()->setChecked(true);
    CHECK(columnTexts(panel, MaterialList::QUOTE) == QStringList{"Water is life"});
    panel.withoutLinksBox()->setChecked(false);

    // The field under the header of a column filters it
    panel.setColumnFilter(QString::fromLatin1(MaterialList::QUOTE), QStringLiteral("camel"));
    auto* field = panel.findChild<QLineEdit*>(QStringLiteral("columnFilter_quote"));
    REQUIRE(field != nullptr);
    CHECK(field->text() == QStringLiteral("camel"));
    CHECK(columnTexts(panel, MaterialList::QUOTE) == QStringList{"A camel kneels"});
    // Clear Filters shows all of them
    clear->click();
    CHECK(rows->rowCount() == 3);
    CHECK(field->text().isEmpty());

    // A column hidden and shown again; one column always stays
    panel.showColumn(QString::fromLatin1(MaterialList::TITLE), true);
    CHECK(panel.shownColumns() == QStringList{"quote", "links", "title"});
    CHECK(MaterialList::shownColumns(listOf(core::MaterialKinds::QUOTE)) ==
          QStringList{"quote", "links", "title"});
    panel.showColumn(QString::fromLatin1(MaterialList::LINKS), false);
    panel.showColumn(QString::fromLatin1(MaterialList::TITLE), false);
    CHECK(panel.shownColumns() == QStringList{"quote"});
    panel.showColumn(QString::fromLatin1(MaterialList::QUOTE), false);
    CHECK(panel.shownColumns() == QStringList{"quote"});

    // What is chosen, the search and the filters stay with the list when it shows another
    // kind, and come back with it
    REQUIRE(panel.selectMaterial(water));
    CHECK(panel.currentMaterial() == water);
    panel.searchField()->setText(QStringLiteral("life"));
    panel.setList(listOf(core::MaterialKinds::WORK_NOTE));
    CHECK(panel.searchField()->text().isEmpty());
    CHECK(panel.currentMaterial().isEmpty());
    panel.setList(listOf(core::MaterialKinds::QUOTE));
    CHECK(panel.searchField()->text() == QStringLiteral("life"));
    CHECK(panel.currentMaterial() == water);
    CHECK_FALSE(sand.isEmpty());
    CHECK_FALSE(camel.isEmpty());
}

TEST_CASE("Materials list: the keys, the commands and the menu of a row", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    const QString sand = addQuote(QStringLiteral("Sand"), QStringLiteral("Sand everywhere"));
    MaterialListPanel panel(listOf(core::MaterialKinds::QUOTE));
    QTableView* table = panel.table();

    // What the list asks for
    QStringList edit;
    QList<bool> editText;
    QStringList add;
    QStringList rename;
    QStringList remove;
    QStringList properties;
    QStringList chosen;
    QObject::connect(&panel, &MaterialListPanel::requestEdit,
                     [&](const QString& id, bool text) {
                         edit << id;
                         editText << text;
                     });
    QObject::connect(&panel, &MaterialListPanel::requestAdd,
                     [&](const QString& list) { add << list; });
    QObject::connect(&panel, &MaterialListPanel::requestRename,
                     [&](const QString& id) { rename << id; });
    QObject::connect(&panel, &MaterialListPanel::requestDelete,
                     [&](const QString& id) { remove << id; });
    QObject::connect(&panel, &MaterialListPanel::requestProperties,
                     [&](const QString& id) { properties << id; });
    QObject::connect(&panel, &MaterialListPanel::materialChosen,
                     [&](const QString& id) { chosen << id; });

    // Nothing chosen: only Insert does something
    auto* editButton = panel.findChild<QToolButton*>(QStringLiteral("materialListEdit"));
    auto* addButton = panel.findChild<QToolButton*>(QStringLiteral("materialListAdd"));
    REQUIRE(editButton != nullptr);
    REQUIRE(addButton != nullptr);
    CHECK_FALSE(editButton->isEnabled());
    CHECK(addButton->isEnabled());
    CHECK(addButton->text() == QStringLiteral("Add Quote..."));
    press(table, Qt::Key_Return);
    CHECK(edit.isEmpty());
    press(table, Qt::Key_Insert);
    CHECK(add == QStringList{listOf(core::MaterialKinds::QUOTE)});
    // Filled again, the list asks once for each click
    panel.refresh();
    addButton->click();
    CHECK(add.size() == 2);
    CHECK(panel.searchField()->actions().size() == 1);

    // The writer chooses a quote: it shows in the Properties panel
    REQUIRE(panel.selectMaterial(sand));
    CHECK(chosen == QStringList{sand});
    CHECK(editButton->isEnabled());

    // The keys of the list wait for nothing else
    CHECK(takesKey(table, Qt::Key_Return));
    CHECK(takesKey(table, Qt::Key_F2));
    CHECK(takesKey(table, Qt::Key_Delete));
    CHECK(takesKey(table, Qt::Key_C, Qt::ControlModifier));
    CHECK(takesKey(table, Qt::Key_Return, Qt::AltModifier));
    CHECK_FALSE(takesKey(table, Qt::Key_Down));

    // Enter edits the words of a quote, F2 renames, Delete deletes, Alt+Enter goes to the
    // Properties panel
    press(table, Qt::Key_Return);
    CHECK(edit == QStringList{sand});
    CHECK(editText == QList<bool>{true});
    press(table, Qt::Key_F2);
    CHECK(rename == QStringList{sand});
    press(table, Qt::Key_Delete);
    CHECK(remove == QStringList{sand});
    press(table, Qt::Key_Return, Qt::AltModifier);
    CHECK(properties == QStringList{sand});

    // Ctrl+C copies the words of the quote
    QGuiApplication::clipboard()->clear();
    press(table, Qt::Key_C, Qt::ControlModifier);
    CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("Sand everywhere"));

    // The menu of the row: Edit first, in bold, with the keys of the commands
    QMenu menu;
    panel.fillMenu(menu);
    const QStringList texts = menuTexts(menu);
    REQUIRE_FALSE(texts.isEmpty());
    CHECK(texts.first().startsWith(QStringLiteral("&Edit\t")));
    CHECK(menu.actions().first()->font().bold());
    CHECK(texts.contains(QStringLiteral("&Open")));
    CHECK(texts.contains(QStringLiteral("&Link...")));
    CHECK(std::any_of(texts.cbegin(), texts.cend(), [](const QString& text) {
        return text.startsWith(QStringLiteral("Add Quote...\t"));
    }));
    // A quote has no file to show in its folder
    CHECK_FALSE(texts.contains(QStringLiteral("Show in &Folder")));

}

TEST_CASE("Materials list: the pictures as a table with thumbnails or as tiles",
          "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString map = addPicture(files.path(), QStringLiteral("map.png"), QSize(80, 40),
                                   QStringLiteral("Map"));

    MaterialListPanel panel(listOf(core::MaterialKinds::PICTURE));
    QSortFilterProxyModel* rows = panel.rows();
    REQUIRE(rows->rowCount() == 1);
    // The thumbnail first, then the format and the size of the picture
    CHECK(panel.shownColumns().first() == QStringLiteral("thumbnail"));
    CHECK(rows->index(0, 0).data(Qt::DecorationRole).value<QPixmap>().isNull() == false);
    CHECK(columnTexts(panel, MaterialList::FORMAT) == QStringList{"PNG"});
    CHECK(columnTexts(panel, MaterialList::DIMENSIONS) == QStringList{"80 × 40"});
    CHECK(columnTexts(panel, MaterialList::ORIGIN) == QStringList{"disk"});

    // The tiles: kept for the list, also for a new tab of it
    auto* tilesButton = panel.findChild<QToolButton*>(QStringLiteral("materialListTiles"));
    REQUIRE(tilesButton != nullptr);
    CHECK_FALSE(tilesButton->isHidden());
    REQUIRE(panel.selectMaterial(map));
    tilesButton->click();
    CHECK(panel.view() == panel.tiles());
    CHECK(panel.currentMaterial() == map);
    CHECK(MaterialList::tilesShown(listOf(core::MaterialKinds::PICTURE)));
    CHECK_FALSE(rows->index(0, panel.tiles()->modelColumn())
                    .data(MaterialListPanel::TILE_ROLE)
                    .value<QPixmap>()
                    .isNull());
    MaterialListPanel again(listOf(core::MaterialKinds::PICTURE));
    CHECK(again.view() == again.tiles());
    again.showTiles(false);
    CHECK(again.view() == again.table());
    CHECK_FALSE(MaterialList::tilesShown(listOf(core::MaterialKinds::PICTURE)));

    // A list without tiles has no such buttons
    MaterialListPanel quotes(listOf(core::MaterialKinds::QUOTE));
    auto* quoteTiles = quotes.findChild<QToolButton*>(QStringLiteral("materialListTiles"));
    REQUIRE(quoteTiles != nullptr);
    CHECK(quoteTiles->isHidden());
    quotes.showTiles(true);
    CHECK(quotes.view() == quotes.table());
}

TEST_CASE("Materials list: All Materials has the working notes and the materials of every "
          "kind", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const QString note = pm.addElement(baseKind(core::MaterialKinds::WORK_NOTE),
                                       QStringLiteral("Ideas"), core::BookPlace::Workshop);
    REQUIRE_FALSE(note.isEmpty());
    addQuote(QStringLiteral("Sand"), QStringLiteral("Sand everywhere"));

    MaterialListPanel panel{QString::fromLatin1(MaterialList::ALL)};
    REQUIRE(panel.rows()->rowCount() == 2);
    // By kind at first: the quotes, then the working notes
    CHECK(columnTexts(panel, MaterialList::TYPE) == QStringList{"Quotes", "Working notes"});
    CHECK(columnTexts(panel, MaterialList::DETAILS) ==
          QStringList{"Sand everywhere", "Words: 0"});
    auto* addButton = panel.findChild<QToolButton*>(QStringLiteral("materialListAdd"));
    REQUIRE(addButton != nullptr);
    CHECK(addButton->text() == QStringLiteral("Add..."));

    // A working note is edited in the editor: it has no link of its own in the menu
    REQUIRE(panel.selectMaterial(note));
    QMenu menu;
    panel.fillMenu(menu);
    const QStringList texts = menuTexts(menu);
    CHECK_FALSE(texts.contains(QStringLiteral("&Link...")));
    CHECK_FALSE(texts.contains(QStringLiteral("&Open")));
}

TEST_CASE("Navigator: Materials and its kinds show their lists on tabs", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    const QString sand = addQuote(QStringLiteral("Sand"), QStringLiteral("Sand everywhere"));
    addQuote(QStringLiteral("Water"), QStringLiteral("Water is life"));
    ListWindow lists;
    QTabWidget* tabs = lists.window.tabs;
    const QTreeWidget* tree = lists.window.navigatorPanel->tree();

    // The kinds with their numbers, without their materials
    QTreeWidgetItem* quotes = kindItem(tree, core::MaterialKinds::QUOTE);
    REQUIRE(quotes != nullptr);
    CHECK(quotes->childCount() == 0);
    CHECK(quotes->data(0, Qt::AccessibleTextRole).toString() == QStringLiteral("Quotes (2)"));

    // A kind clicked opens its list on a tab of its own; clicked again, the tab comes to the
    // front
    lists.click(quotes);
    REQUIRE(tabs->count() == 1);
    CHECK(tabs->tabText(0) == QStringLiteral("Quotes"));
    CHECK(lists.list(0)->list() == listOf(core::MaterialKinds::QUOTE));
    CHECK(lists.list(0)->rows()->rowCount() == 2);
    QTreeWidgetItem* materials = navigatorItem(tree, QStringLiteral("workshop:materials"));
    lists.click(materials);
    REQUIRE(tabs->count() == 2);
    CHECK(tabs->tabText(1) == QStringLiteral("All Materials"));
    CHECK(tabs->currentIndex() == 1);
    lists.activate(kindItem(tree, core::MaterialKinds::QUOTE));
    CHECK(tabs->count() == 2);
    CHECK(tabs->currentIndex() == 0);
    CHECK(lists.lists.currentList() == lists.list(0));

    // The Navigator's other nodes open no list
    lists.click(navigatorItem(tree, novel.chapterId));
    CHECK(tabs->count() == 2);

    // A material chosen in a list shows in the Properties panel, which comes to the front
    lists.propertiesShown.clear();
    REQUIRE(lists.list(0)->selectMaterial(sand));
    CHECK(lists.propertiesShown == QList<bool>{false});
    press(lists.list(0)->table(), Qt::Key_Return, Qt::AltModifier);
    CHECK(lists.propertiesShown == QList<bool>{false, true});

    // A material changed on its card: the list in front shows it at once, the one behind
    // when it comes to the front
    REQUIRE(novel.pm.setDescription(sand, QStringLiteral("Sand, sand everywhere")));
    lists.window.navigator.onMaterialChanged(sand);
    QCoreApplication::processEvents();
    CHECK(columnTexts(*lists.list(0), MaterialList::QUOTE).contains(
        QStringLiteral("Sand, sand everywhere")));
    CHECK(lists.list(1)->property("materialListStale").toBool());
    tabs->setCurrentIndex(1);
    CHECK_FALSE(lists.list(1)->property("materialListStale").toBool());
    CHECK(columnTexts(*lists.list(1), MaterialList::DETAILS).contains(
        QStringLiteral("Sand, sand everywhere")));

    // Shown in the Navigator, a material shows its kind there
    lists.window.navigatorPanel->revealElement(sand);
    CHECK(tree->currentItem() == kindItem(tree, core::MaterialKinds::QUOTE));

    // The book closed takes its lists with it
    lists.window.documents.onProjectClosed();
    CHECK(lists.lists.lists().isEmpty());
}

TEST_CASE("Navigator: the settings of the Materials, their materials under their kinds and "
          "one tab of the lists", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    const QString sand = addQuote(QStringLiteral("Sand"), QStringLiteral("Sand everywhere"));
    QTemporaryDir files;
    REQUIRE(files.isValid());
    addPicture(files.path(), QStringLiteral("map.png"), QSize(20, 10), QStringLiteral("Map"));
    auto& settings = core::SettingsManager::getInstance();
    ListWindow lists;
    QTabWidget* tabs = lists.window.tabs;
    const QTreeWidget* tree = lists.window.navigatorPanel->tree();

    // The kinds with a list to expand: their materials under them, collapsed at first
    settings.set<std::string>(gui::NavigatorPanel::MATERIALS_TREE_SETTING, "expand");
    QCoreApplication::processEvents();
    QTreeWidgetItem* quotes = kindItem(tree, core::MaterialKinds::QUOTE);
    REQUIRE(quotes != nullptr);
    REQUIRE(quotes->childCount() == 1);
    CHECK(quotes->child(0)->data(0, Qt::UserRole).toString() == sand);
    CHECK_FALSE(quotes->isExpanded());
    lists.window.navigatorPanel->revealElement(sand);
    CHECK(tree->currentItem() == quotes->child(0));
    // A kind clicked still opens its list
    lists.click(quotes);
    CHECK(tabs->count() == 1);

    settings.set<std::string>(gui::NavigatorPanel::MATERIALS_TREE_SETTING, "types");
    QCoreApplication::processEvents();
    CHECK(kindItem(tree, core::MaterialKinds::QUOTE)->childCount() == 0);

    // One tab of the lists: the kind clicked changes on it
    settings.set<std::string>(MaterialList::TABS_SETTING, "one");
    lists.click(kindItem(tree, core::MaterialKinds::PICTURE));
    REQUIRE(tabs->count() == 1);
    CHECK(lists.list(0)->list() == listOf(core::MaterialKinds::PICTURE));
    CHECK(tabs->tabText(0) == QStringLiteral("Pictures"));
    lists.click(kindItem(tree, core::MaterialKinds::QUOTE));
    CHECK(tabs->count() == 1);
    CHECK(lists.list(0)->list() == listOf(core::MaterialKinds::QUOTE));
    CHECK(tabs->tabText(0) == QStringLiteral("Quotes"));
}
