// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_figures_panel.cpp
/// @brief The panel Images and Tables: its cards, kinds, search, scope, keys and menu, and what
///        it lists and does in the texts of a book

#include <catch2/catch_test_macros.hpp>

#include "kalahari_tests/gui/document_window.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_figures.h"
#include "kalahari/gui/caption_coordinator.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/figures_panel_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/figures_panel.h"
#include "kalahari_tests/test_support/window_parts.h"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QStandardItemModel>
#include <QTextBlock>
#include <QTextCursor>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace kalahari;
using editor::FigureKind;
using gui::FigureAction;
using gui::FigureEntry;
using gui::FigureKinds;
using gui::FigureScope;
using gui::FiguresPanel;
using test::OpenNovel;
using test::Window;

namespace {

/// A figure of a text, as the panel is given it
FigureEntry entryOf(const QString& elementId, const QString& title, int order, FigureKind kind,
                    int ordinal, const QString& captionTitle = QString(),
                    const QString& caption = QString()) {
    FigureEntry entry;
    entry.elementId = elementId;
    entry.textTitle = title;
    entry.textOrder = order;
    entry.figure.kind = kind;
    entry.figure.ordinal = ordinal;
    entry.figure.title = captionTitle;
    entry.figure.caption = caption;
    entry.figure.captioned = !caption.isEmpty();
    if (entry.figure.captioned) {
        entry.figure.whole = captionTitle + QStringLiteral(". ") + caption;
    }
    return entry;
}

/// An image and a table with captions and an image without one in a text, an image with a
/// caption and a table without one in the next text
std::vector<FigureEntry> someFigures() {
    FigureEntry bridge = entryOf(QStringLiteral("storm"), QStringLiteral("The Storm"), 0,
                                 FigureKind::Image, 1, QStringLiteral("Figure 1"),
                                 QStringLiteral("The bridge"));
    bridge.figure.image.source = QStringLiteral("images/bridge.png");
    FigureEntry wells = entryOf(QStringLiteral("storm"), QStringLiteral("The Storm"), 0,
                                FigureKind::Table, 1, QStringLiteral("Table 1"),
                                QStringLiteral("The wells"));
    wells.figure.rows = 3;
    wells.figure.columns = 2;
    FigureEntry camel = entryOf(QStringLiteral("storm"), QStringLiteral("The Storm"), 0,
                                FigureKind::Image, 2);
    camel.figure.image.source = QStringLiteral("images/c.png");
    camel.figure.image.name = QStringLiteral("Camel");
    FigureEntry dunes = entryOf(QStringLiteral("calm"), QStringLiteral("The Calm"), 1,
                                FigureKind::Image, 1, QStringLiteral("Figure 2"),
                                QStringLiteral("Dunes at dawn"));
    dunes.figure.image.source = QStringLiteral("images/dunes.png");
    FigureEntry list = entryOf(QStringLiteral("calm"), QStringLiteral("The Calm"), 1,
                               FigureKind::Table, 1);
    list.figure.rows = 4;
    list.figure.columns = 5;
    return {bridge, wells, camel, dunes, list};
}

/// The keys of the figures the panel shows, parted by " "
std::string shownKeys(const FiguresPanel& panel) {
    QStringList keys;
    for (const FigureEntry& entry : panel.shownEntries()) {
        keys << entry.key();
    }
    return keys.join(QLatin1Char(' ')).toStdString();
}

/// The figures a signal of the panel gave, in their order
struct Caught {
    std::vector<FigureEntry> entries;
    std::vector<FigureAction> actions;

    [[nodiscard]] int count() const { return static_cast<int>(entries.size()); }
    [[nodiscard]] const FigureEntry& last() const { return entries.back(); }
};

/// Catch the figures a signal of the panel gives
template <typename Signal>
void catchIn(FiguresPanel& panel, Signal signal, Caught& caught) {
    QObject::connect(&panel, signal,
                     [&caught](const FigureEntry& entry) { caught.entries.push_back(entry); });
}

/// A key pressed and released in a widget
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent pressed(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &pressed);
    QKeyEvent released(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &released);
}

/// The texts of a menu's items, separators as "|"
QStringList textsOf(const QMenu& menu) {
    QStringList texts;
    for (const QAction* action : menu.actions()) {
        texts << (action->isSeparator() ? QStringLiteral("|") : action->text());
    }
    return texts;
}

/// A picture of a book's folder of images
void addPicture(const QString& folder, const QString& source) {
    REQUIRE(QDir(folder).mkpath(QStringLiteral("images")));
    QImage picture(QSize(40, 20), QImage::Format_RGB32);
    picture.fill(Qt::darkGreen);
    REQUIRE(picture.save(QDir(folder).filePath(source)));
}

/// The texts of a book with images and tables, saved: a numbered figure and a table in the
/// first, a numbered figure in the next one
struct FiguredBook {
    OpenNovel book{QStringLiteral("Figures")};
    QString storm;
    QString calm;

    FiguredBook() {
        storm = book.chapterId;
        calm = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Main),
                                  QStringLiteral("The Calm"), core::BookPlace::Main);
        REQUIRE_FALSE(calm.isEmpty());
        save(storm, QStringLiteral(
                        "<kml><p>Rain</p><p><img src=\"images/a.png\"/></p>"
                        "<p style=\"caption\">Figure <caption-number label=\"Figure\" "
                        "id=\"f1\"/>. The bridge</p>"
                        "<table><tr><td><p>A</p></td><td><p>B</p></td></tr></table>"
                        "<p>After</p></kml>"));
        save(calm, QStringLiteral(
                       "<kml><p><img src=\"images/b.png\" name=\"Dunes\"/></p>"
                       "<p style=\"caption\">Figure <caption-number label=\"Figure\" "
                       "id=\"f2\"/>. The dunes</p></kml>"));
        addPicture(book.pm.imagesBaseFolder(), QStringLiteral("images/b.png"));
    }

    void save(const QString& id, const QString& kml) {
        book.pm.setChapterContent(id, kml);
        REQUIRE(book.pm.saveChapterContent(id));
    }
};

/// The captions and the panel over the tabs of a window, as the main window has them
struct Figures {
    gui::CaptionCoordinator captions;
    FiguresPanel panel;
    gui::FiguresPanelCoordinator coordinator;

    explicit Figures(Window& window)
        : captions(kalahari::test::book(), kalahari::test::commands(), window.tabs,
                   [&window](const QString& id) {
                       window.navigator.onElementSelected(id, QString());
                   },
                   &window.window)
        , coordinator(
              kalahari::test::book(), kalahari::test::commands(), &panel, nullptr, window.tabs,
              [&window](const QString& id) { window.navigator.onElementSelected(id, QString()); },
              [this](const QString& id) { return captions.contextFor(id); }) {}

    /// The keys of the figures listed, parted by " "
    [[nodiscard]] std::string keys() const {
        QStringList keys;
        for (const FigureEntry& entry : coordinator.entries()) {
            keys << entry.key();
        }
        return keys.join(QLatin1Char(' ')).toStdString();
    }

    /// A figure listed, by its key; the test fails without it (no code after FAIL, which MSVC
    /// counts as unreachable)
    [[nodiscard]] FigureEntry entry(const QString& key) const {
        const std::vector<FigureEntry> entries = coordinator.entries();
        const auto it = std::find_if(entries.cbegin(), entries.cend(),
                                     [&key](const FigureEntry& each) { return each.key() == key; });
        if (it == entries.cend()) {
            FAIL("no figure " << key.toStdString());
        }
        return *it;
    }
};

/// The editor of a text opened in the window
editor::BookEditor& open(Window& window, const QString& id) {
    window.navigator.onElementSelected(id, QString());
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    REQUIRE(text != nullptr);
    return *text;
}

}  // namespace

TEST_CASE("Images and Tables panel: the kinds with their counts and the search narrow the cards",
          "[gui][figures][panel]") {
    FiguresPanel panel;
    panel.setDocumentState(true);
    panel.setEntries(someFigures());
    CHECK(shownKeys(panel) == "storm/i1 storm/t1 storm/i2 calm/i1 calm/t1");
    CHECK(panel.kindButton(FigureKinds::All)->text() == QStringLiteral("All (5)"));
    CHECK(panel.kindButton(FigureKinds::Images)->text() == QStringLiteral("Images (3)"));
    CHECK(panel.kindButton(FigureKinds::Tables)->text() == QStringLiteral("Tables (2)"));
    CHECK(panel.emptyLabel()->isHidden());

    // The kinds
    panel.kindButton(FigureKinds::Tables)->click();
    CHECK(panel.kinds() == FigureKinds::Tables);
    CHECK(shownKeys(panel) == "storm/t1 calm/t1");
    panel.setKinds(FigureKinds::Images);
    CHECK(shownKeys(panel) == "storm/i1 storm/i2 calm/i1");
    panel.setKinds(FigureKinds::All);

    // The search: in the captions, the names of the images and their files, case aside; the
    // counts follow it
    panel.setSearchText(QStringLiteral("WELLS"));
    CHECK(shownKeys(panel) == "storm/t1");
    panel.setSearchText(QStringLiteral("camel"));
    CHECK(shownKeys(panel) == "storm/i2");
    CHECK(panel.kindButton(FigureKinds::All)->text() == QStringLiteral("All (1)"));
    CHECK(panel.kindButton(FigureKinds::Tables)->text() == QStringLiteral("Tables (0)"));
    panel.setSearchText(QStringLiteral("bridge.png"));
    CHECK(shownKeys(panel) == "storm/i1");
    panel.setSearchText(QStringLiteral("zebra"));
    CHECK(panel.shownEntries().empty());
    CHECK_FALSE(panel.emptyLabel()->isHidden());
    CHECK(panel.emptyLabel()->text() == QStringLiteral("No caption or name contains “zebra”."));
    panel.setSearchText(QString());

    // A kind the text does not have
    panel.setEntries({someFigures().at(0)});
    panel.setKinds(FigureKinds::Tables);
    CHECK(panel.emptyLabel()->text() == QStringLiteral("No tables."));
    panel.setEntries({someFigures().at(1)});
    panel.setKinds(FigureKinds::Images);
    CHECK(panel.emptyLabel()->text() == QStringLiteral("No images."));
}

TEST_CASE("Images and Tables panel: what it says without figures", "[gui][figures][panel]") {
    FiguresPanel panel;
    panel.setDocumentState(false);
    CHECK(panel.emptyLabel()->text() == QStringLiteral("No text is open."));
    panel.setBookScopeAvailable(true);
    panel.setDocumentState(false);
    CHECK(panel.emptyLabel()->text().startsWith(
        QStringLiteral("No text is open. Open a text, or choose Book")));
    panel.setDocumentState(true);
    CHECK(panel.emptyLabel()->text().startsWith(
        QStringLiteral("This text has no images or tables yet")));
    panel.setScope(FigureScope::Book);
    CHECK(panel.emptyLabel()->text() == QStringLiteral("The book has no images or tables yet."));
}

TEST_CASE("Images and Tables panel: the cards say what each figure is, under the titles of the "
          "texts in the scope Book",
          "[gui][figures][panel]") {
    FiguresPanel panel;
    int scopes = 0;
    QObject::connect(&panel, &FiguresPanel::scopeChanged, [&scopes]() { ++scopes; });
    panel.setEntries(someFigures());
    const auto* model = qobject_cast<const QStandardItemModel*>(panel.list()->model());
    REQUIRE(model != nullptr);
    // The label and the number of its caption and its text; without a caption its kind, and
    // the name of the image or the size of the table
    REQUIRE(model->rowCount() == 5);
    CHECK(model->item(0)->text() == QStringLiteral("Figure 1: The bridge"));
    CHECK(model->item(1)->text() == QStringLiteral("Table 1: The wells"));
    CHECK(model->item(2)->text() == QStringLiteral("Image: No caption · Camel"));
    CHECK(model->item(4)->text() == QStringLiteral("Table: No caption · 4 row(s), 5 column(s)"));
    CHECK(model->item(0)->toolTip().startsWith(QStringLiteral("Figure 1. The bridge")));

    // Without a book only the text in front
    CHECK_FALSE(panel.scopeButton(FigureScope::Book)->isEnabled());
    panel.setScope(FigureScope::Book);
    CHECK(panel.scope() == FigureScope::Text);

    panel.setBookScopeAvailable(true);
    panel.scopeButton(FigureScope::Book)->click();
    CHECK(panel.scope() == FigureScope::Book);
    CHECK(scopes == 1);
    REQUIRE(model->rowCount() == 7);
    CHECK(model->item(0)->text() == QStringLiteral("The Storm"));
    CHECK(model->item(0)->flags() == Qt::NoItemFlags);  // the arrows go over it
    CHECK(model->item(4)->text() == QStringLiteral("The Calm"));
    CHECK(shownKeys(panel) == "storm/i1 storm/t1 storm/i2 calm/i1 calm/t1");

    // The book closed: back to the text in front
    panel.setBookScopeAvailable(false);
    CHECK(panel.scope() == FigureScope::Text);
    CHECK(model->rowCount() == 5);

    // The name of the kind of the text in front, cut when long
    panel.setTextName(QStringLiteral("Chapter"));
    CHECK(panel.scopeButton(FigureScope::Text)->text() == QStringLiteral("Chapter"));
    panel.setTextName(QStringLiteral("A very long name of a kind of text"));
    CHECK(panel.scopeButton(FigureScope::Text)->text().endsWith(QChar(0x2026)));
    CHECK(panel.scopeButton(FigureScope::Text)->toolTip() ==
          QStringLiteral("A very long name of a kind of text"));
    panel.setTextName(QString());
    CHECK(panel.scopeButton(FigureScope::Text)->text() == QStringLiteral("Text"));
}

TEST_CASE("Images and Tables panel: the keys of the list and of the search",
          "[gui][figures][panel]") {
    FiguresPanel panel;
    panel.setEntries(someFigures());
    Caught activated;
    Caught opened;
    catchIn(panel, &FiguresPanel::entryActivated, activated);
    catchIn(panel, &FiguresPanel::openRequested, opened);
    int back = 0;
    QObject::connect(&panel, &FiguresPanel::editorFocusRequested, [&back]() { ++back; });

    // Down in the search: the first card
    press(panel.searchEdit(), Qt::Key_Down);
    CHECK(panel.selectedKey() == QStringLiteral("storm/i1"));
    // The arrows select a figure: the text goes to it
    const int before = activated.count();
    panel.list()->setCurrentIndex(panel.list()->model()->index(1, 0));
    CHECK(activated.count() == before + 1);
    CHECK(activated.last().key() == QStringLiteral("storm/t1"));
    // Selected by the program: no going there
    panel.selectEntry(QStringLiteral("calm/i1"));
    CHECK(activated.count() == before + 1);
    CHECK(panel.selectedKey() == QStringLiteral("calm/i1"));

    // Enter goes to it in its text, with the keys there
    press(panel.list(), Qt::Key_Return);
    REQUIRE(opened.count() == 1);
    CHECK(opened.last().key() == QStringLiteral("calm/i1"));

    // Esc: back to the text, from the list and from the search
    press(panel.list(), Qt::Key_Escape);
    press(panel.searchEdit(), Qt::Key_Escape);
    CHECK(back == 2);

    // The selected figure stays selected when the list is made again
    const std::vector<FigureEntry> all = someFigures();
    panel.setEntries(std::vector<FigureEntry>(all.begin() + 1, all.end()));
    CHECK(panel.selectedKey() == QStringLiteral("calm/i1"));
    CHECK(activated.count() == before + 1);
}

TEST_CASE("Images and Tables panel: the menu of a figure", "[gui][figures][panel]") {
    FiguresPanel panel;
    panel.setEntries(someFigures());
    Caught opened;
    Caught asked;
    catchIn(panel, &FiguresPanel::openRequested, opened);
    QObject::connect(&panel, &FiguresPanel::actionRequested,
                     [&asked](const FigureEntry& entry, FigureAction action) {
                         asked.entries.push_back(entry);
                         asked.actions.push_back(action);
                     });
    CHECK(panel.createMenu(QStringLiteral("none/i1")) == nullptr);

    const std::unique_ptr<QMenu> image(panel.createMenu(QStringLiteral("storm/i1")));
    REQUIRE(image != nullptr);
    CHECK(textsOf(*image) ==
          QStringList({QStringLiteral("&Go to Image"), QStringLiteral("Image &Properties..."),
                       QStringLiteral("Edit &Caption..."), QStringLiteral("Cop&y Caption")}));
    for (QAction* item : image->actions()) {
        item->trigger();
    }
    CHECK(opened.count() == 1);
    REQUIRE(asked.count() == 2);
    CHECK(asked.actions.at(0) == FigureAction::Properties);
    CHECK(asked.actions.at(1) == FigureAction::Caption);
    CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("Figure 1. The bridge"));

    // A table without a caption: one to insert, none to copy
    const std::unique_ptr<QMenu> table(panel.createMenu(QStringLiteral("calm/t1")));
    REQUIRE(table != nullptr);
    CHECK(textsOf(*table) ==
          QStringList({QStringLiteral("&Go to Table"), QStringLiteral("Table &Properties..."),
                       QStringLiteral("Insert &Caption...")}));

    // Read-only: only gone to and copied
    panel.setReadOnly(true);
    const std::unique_ptr<QMenu> readOnly(panel.createMenu(QStringLiteral("storm/i1")));
    REQUIRE(readOnly != nullptr);
    CHECK(readOnly->actions().at(0)->isEnabled());
    CHECK_FALSE(readOnly->actions().at(1)->isEnabled());
    CHECK_FALSE(readOnly->actions().at(2)->isEnabled());
    CHECK(readOnly->actions().at(3)->isEnabled());
}

TEST_CASE("Images and Tables panel: the cards follow the settings and the theme",
          "[gui][figures][panel][settings]") {
    auto& settings = core::SettingsManager::getInstance();
    const int savedLines = settings.get<int>(FiguresPanel::LINES_SETTING, 2);
    const bool savedPictures = settings.get<bool>(FiguresPanel::PICTURES_SETTING, true);
    FiguresPanel panel;
    int drawn = 0;
    panel.setPictureSource([&drawn](const FigureEntry&, const QSize& pixels) {
        ++drawn;
        QImage picture(pixels, QImage::Format_RGB32);
        picture.fill(Qt::darkGreen);
        return picture;
    });
    panel.resize(300, 400);
    panel.setEntries(someFigures());
    panel.show();
    QApplication::processEvents();

    settings.set<int>(FiguresPanel::LINES_SETTING, 1);
    QApplication::processEvents();
    const int oneLine = panel.list()->sizeHintForRow(0);
    settings.set<int>(FiguresPanel::LINES_SETTING, 5);
    QApplication::processEvents();
    CHECK(panel.list()->sizeHintForRow(0) > oneLine);

    // The pictures of the images, drawn once for each image and size; none for a table, none
    // without the setting
    const FigureEntry bridge = someFigures().at(0);
    const FigureEntry wells = someFigures().at(1);
    drawn = 0;
    CHECK_FALSE(panel.pictureOf(bridge, QSize(31, 31)).isNull());
    CHECK_FALSE(panel.pictureOf(bridge, QSize(31, 31)).isNull());
    CHECK(drawn == 1);
    CHECK(panel.pictureOf(wells, QSize(31, 31)).isNull());
    settings.set<bool>(FiguresPanel::PICTURES_SETTING, false);
    QApplication::processEvents();
    CHECK(panel.pictureOf(bridge, QSize(31, 31)).isNull());

    // The colors of the kinds come from the theme
    const auto& themes = core::ThemeManager::getInstance();
    CHECK(panel.kindColor(FigureKind::Image) ==
          themes.editorColor(FiguresPanel::IMAGE_COLOR_KEY, QColor()));
    CHECK(panel.kindColor(FigureKind::Table) ==
          themes.editorColor(FiguresPanel::TABLE_COLOR_KEY, QColor()));
    CHECK(panel.kindColor(FigureKind::Image) != panel.kindColor(FigureKind::Table));

    settings.set<int>(FiguresPanel::LINES_SETTING, savedLines);
    settings.set<bool>(FiguresPanel::PICTURES_SETTING, savedPictures);
}

TEST_CASE("Images and Tables panel: the figures of the text in front and of the whole book",
          "[gui][figures][panel][book]") {
    FiguredBook figured;
    Window window;
    Figures figures(window);
    CHECK(figures.coordinator.entries().empty());

    // The text in front, its captions numbered as its editor numbers them
    editor::BookEditor& storm = open(window, figured.storm);
    figures.coordinator.refresh();
    const QString first = figured.storm + QStringLiteral("/i1");
    const QString table = figured.storm + QStringLiteral("/t1");
    CHECK(figures.keys() == (first + QLatin1Char(' ') + table).toStdString());
    const FigureEntry bridge = figures.entry(first);
    CHECK(bridge.textTitle == QStringLiteral("The Storm"));
    CHECK(bridge.figure.caption == QStringLiteral("The bridge"));
    CHECK(bridge.figure.title.startsWith(QStringLiteral("Figure ")));
    CHECK(figures.entry(table).figure.columns == 2);
    CHECK(figures.panel.scopeButton(FigureScope::Book)->isEnabled());

    // The book: every text in reading order, the closed ones as saved, numbered after the
    // captions of the texts before them, as their editors number them once open
    figures.panel.setScope(FigureScope::Book);
    figures.coordinator.refresh();
    const QString dunes = figured.calm + QStringLiteral("/i1");
    CHECK(figures.keys() == (first + QLatin1Char(' ') + table + QLatin1Char(' ') + dunes)
                                .toStdString());
    const FigureEntry closed = figures.entry(dunes);
    CHECK(closed.textTitle == QStringLiteral("The Calm"));
    CHECK(closed.figure.caption == QStringLiteral("The dunes"));
    CHECK(closed.figure.title != bridge.figure.title);
    editor::BookEditor& calm = open(window, figured.calm);
    figures.coordinator.refresh();
    // At once, before its editor is given the numbers of the book
    CHECK(figures.entry(dunes).figure == closed.figure);
    CHECK(calm.captionNumbers().own().size() == 1);
    figures.captions.refresh();
    const std::vector<editor::FigureInText> numbered =
        editor::figuresIn(*calm.textDocument(), calm.captionNumbers());
    REQUIRE(numbered.size() == 1);
    CHECK(numbered.front() == closed.figure);

    // A caption changed: the list follows it
    window.tabs->setCurrentWidget(window.editor(0));
    QTextCursor cursor(storm.textDocument()->findBlockByNumber(2));
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertText(QStringLiteral(" in 1931"));
    figures.coordinator.refresh();
    CHECK(figures.entry(first).figure.caption == QStringLiteral("The bridge in 1931"));
}

TEST_CASE("Images and Tables panel: going to a figure, and its properties and caption",
          "[gui][figures][panel][book]") {
    gui::registerAllCommands(kalahari::test::commands(), gui::CommandCallbacks{});
    std::vector<std::string> ran;
    for (const char* id : {"image.properties", "image.addCaption", "table.properties",
                           "table.caption"}) {
        gui::Command* command = kalahari::test::commands().getCommand(id);
        REQUIRE(command != nullptr);
        command->execute = [&ran, id]() { ran.emplace_back(id); };
        command->isEnabled = []() { return true; };
    }
    FiguredBook figured;
    Window window;
    Figures figures(window);
    editor::BookEditor& storm = open(window, figured.storm);
    figures.panel.setScope(FigureScope::Book);
    figures.coordinator.refresh();
    const int tabs = window.tabs->count();
    const FigureEntry first = figures.entry(figured.storm + QStringLiteral("/i1"));
    const FigureEntry table = figures.entry(figured.storm + QStringLiteral("/t1"));
    const FigureEntry dunes = figures.entry(figured.calm + QStringLiteral("/i1"));

    // To it in the text in front: the image selected, the cursor in the table; a figure of a
    // closed text does not open it
    CHECK(figures.coordinator.goToFigure(first));
    CHECK(storm.selectedImage().has_value());
    CHECK(figures.coordinator.goToFigure(table));
    CHECK(storm.isInTable());
    CHECK_FALSE(figures.coordinator.goToFigure(dunes));
    CHECK(window.tabs->count() == tabs);

    // Go To opens its text, in front, the image selected there
    CHECK(figures.coordinator.openFigure(dunes));
    CHECK(window.tabs->count() == tabs + 1);
    auto* front = qobject_cast<gui::EditorPanel*>(window.tabs->currentWidget());
    REQUIRE(front != nullptr);
    CHECK(front->property("elementId").toString() == figured.calm);
    CHECK(front->getBookEditor()->selectedImage().has_value());

    // Properties and Insert Caption: the commands of the menus, in its text in front
    CHECK(figures.coordinator.runAction(first, FigureAction::Properties));
    CHECK(window.tabs->currentWidget() == window.editor(0));
    CHECK(storm.selectedImage().has_value());
    CHECK(figures.coordinator.runAction(table, FigureAction::Caption));
    CHECK(figures.coordinator.runAction(table, FigureAction::Properties));
    CHECK(figures.coordinator.runAction(dunes, FigureAction::Caption));
    CHECK(ran == std::vector<std::string>{"image.properties", "table.caption",
                                          "table.properties", "image.addCaption"});

    // The picture of an image, from the book's folder of images; none for a missing file
    CHECK_FALSE(figures.coordinator.pictureOf(dunes, QSize(24, 24)).isNull());
    CHECK(figures.coordinator.pictureOf(first, QSize(24, 24)).isNull());
    kalahari::test::commands().clear();
}
