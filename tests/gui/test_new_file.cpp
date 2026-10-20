/// @file test_new_file.cpp
/// @brief New File: an element of the open book or an empty text file in the New File window,
/// or a file outside a book; Open File in the File menu and on the toolbars

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/gui/command_registrar.h>
#include <kalahari/gui/command_registry.h>
#include <kalahari/gui/dialogs/new_file_dialog.h>
#include <kalahari/gui/icon_registrar.h>
#include <kalahari/gui/menu_builder.h>
#include <kalahari/gui/recent_books_menu.h>
#include <kalahari/gui/toolbar_manager.h>
#include <kalahari/gui/utils/access_keys.h>
#include <kalahari/gui/utils/program_folders.h>
#include <kalahari/gui/widgets/element_place_picker.h>
#include <kalahari/gui/widgets/fitting_scroll_area.h>

#include "document_window.h"
#include "../editor/editor_test_utils.h"

#include <QAbstractButton>
#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <algorithm>
#include <functional>
#include <string>

/// Qt's own way to press a key over the window's shortcuts, as the system delivers it (Qt
/// Test presses keys with it; the program does not link Qt Test)
Q_GUI_EXPORT bool qt_sendShortcutOverrideEvent(QObject* o, ulong timestamp, int k,
                                               Qt::KeyboardModifiers mods,
                                               const QString& text = QString(),
                                               bool autorep = false, ushort count = 1);

using namespace kalahari;
using core::BookPlace;
using gui::dialogs::NewFileDialog;
using test::onScreen;
using test::OwnFileDialogs;
using test::SHOWN_TICKS;

namespace {

/// @brief The items of @p menu: their texts, a submenu by its title, "-" for a separator
QStringList itemsOf(const QMenu& menu) {
    QStringList items;
    for (const QAction* action : menu.actions()) {
        items << (action->isSeparator() ? QStringLiteral("-")
                                        : gui::withoutAccessKey(action->text()));
    }
    return items;
}

/// @brief @p texts in one line, as the checks print them: "New File... | New Book..."
std::string joined(const QStringList& texts) {
    return texts.join(QStringLiteral(" | ")).toStdString();
}

/// The role that marks the name of a group of tiles in the list of kinds of the New File window
constexpr int HEADING_ROLE = Qt::UserRole + 1;

/// @brief The rows of the list of kinds of the New File window: "# Front matter" for the
/// name of a part of the book, the name of the kind for a tile, and "Title page (greyed): why"
/// for a greyed tile
QStringList rowsOf(const QListWidget& list) {
    QStringList rows;
    for (int row = 0; row < list.count(); ++row) {
        const QListWidgetItem* item = list.item(row);
        if (item->data(HEADING_ROLE).toBool()) {
            rows << QStringLiteral("# ") + item->text();
        } else if (item->flags().testFlag(Qt::ItemIsEnabled)) {
            rows << item->text();
        } else {
            rows << item->text() + QStringLiteral(" (greyed): ") + item->toolTip();
        }
    }
    return rows;
}

/// @brief The first tile named @p name in the New File window: of a kind, or of an empty text
/// file
QListWidgetItem* tileOf(const NewFileDialog& dialog, const QString& name) {
    for (int row = 0; row < dialog.kindList()->count(); ++row) {
        QListWidgetItem* item = dialog.kindList()->item(row);
        if (!item->data(HEADING_ROLE).toBool() && item->text() == name) {
            return item;
        }
    }
    FAIL_CHECK("No tile " << name.toStdString());
    return nullptr;
}

/// @brief A click on the tile of the kind named @p name, as the writer chooses a kind
void choose(const NewFileDialog& dialog, const QString& name) {
    QListWidgetItem* tile = tileOf(dialog, name);
    if (tile) {
        dialog.kindList()->setCurrentItem(tile);
    }
}

/// @brief Type @p text into @p field key by key, as the writer does
void typeText(QLineEdit* field, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(field, &press);
    }
}

/// @brief Erase the text of @p field with the keys, as the writer does
void eraseText(QLineEdit* field) {
    field->selectAll();
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(field, &press);
}

/// @brief The titles of @p elements: "Chapter 1, The Storm"
std::string titlesOf(const QList<core::ProjectElement>& elements) {
    QStringList titles;
    for (const core::ProjectElement& element : elements) {
        titles << element.title;
    }
    return titles.join(QStringLiteral(", ")).toStdString();
}

/// @brief The Cancel button of the New File window
QAbstractButton* cancelButton(const NewFileDialog& dialog) {
    return dialog.cancelButton();
}

/// @brief Run @p command; in the New File window it shows, do @p act and press the button it
/// returns (Create or Cancel). The texts of the other windows go to @p others, and those
/// windows are closed.
///
/// The button is pressed in the loop of the window once the system shows the window, as the
/// writer presses it: on macOS, Qt built with its checks stopped the test in its own code when
/// the test closed the window at the moment it was shown.
/// @return The number of New File windows the command showed
int pressing(const std::function<void()>& command,
             const std::function<QAbstractButton*(NewFileDialog&)>& act, QStringList& others) {
    int shown = 0;
    int waited = 0;
    bool noButton = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        QDialog* dialog = test::shownDialog();
        if (!dialog) {
            return;
        }
        auto* newFile = qobject_cast<NewFileDialog*>(dialog);
        if (!newFile) {
            others.append(test::textsOf(*dialog));
            dialog->reject();
            return;
        }
        if (newFile->property("handled").toBool()) {
            return;  // The button is pressed in its loop
        }
        if (!onScreen(*newFile) && ++waited < SHOWN_TICKS) {
            return;
        }
        newFile->setProperty("handled", true);
        ++shown;
        if (QAbstractButton* button = act(*newFile)) {
            QMetaObject::invokeMethod(button, "click", Qt::QueuedConnection);
        } else {
            // Closed all the same: the test never waits for the window
            noButton = true;
            QMetaObject::invokeMethod(newFile, "reject", Qt::QueuedConnection);
        }
    });
    timer.start();
    command();
    CHECK_FALSE(noButton);
    return shown;
}

/// @brief Run @p command, choosing the file @p path in the window that saves a file; the file
/// the window proposes goes to @p proposed, and the types of files it offers to @p types
/// @return The number of windows that saved a file
int savingAs(const std::function<void()>& command, const QString& path, QString* proposed,
             QStringList* types = nullptr) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        auto* dialog = qobject_cast<QFileDialog*>(test::shownDialog());
        if (!dialog || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        ++shown;
        if (proposed) {
            *proposed = dialog->selectedFiles().value(0);
        }
        if (types) {
            *types = dialog->nameFilters();
        }
        // The writer types the name in the field of the name. QFileDialog::selectFile() leaves
        // the field as it is while it has the focus, which it has where the system gives the
        // window the focus.
        if (auto* name = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"))) {
            name->setText(path);
        } else {
            dialog->selectFile(path);
        }
        QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
    });
    timer.start();
    command();
    return shown;
}

/// @brief The texts of the items of the Navigator of @p window of the type @p type, in their
/// order: "standalone_file" for the files outside the book, "book_file" for the files of the
/// book outside its text
QStringList navigatorItems(const test::Window& window, const char* type) {
    auto* tree = window.navigatorPanel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    QStringList texts;
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole + 1).toString() == QLatin1String(type)) {
            texts << (*it)->text(0);
        }
    }
    return texts;
}

/// @brief The item of the Navigator of @p window with the text @p text
QTreeWidgetItem* navigatorItem(const test::Window& window, const QString& text) {
    auto* tree = window.navigatorPanel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->text(0) == text) {
            return *it;
        }
    }
    FAIL_CHECK("No item " << text.toStdString() << " in the Navigator");
    return nullptr;
}

/// @brief Type the character @p ch in the text of @p panel, as the writer does
void typeInto(gui::EditorPanel& panel, QChar ch) {
    QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
    QCoreApplication::sendEvent(panel.getBookEditor(), &press);
}

/// @brief Whether the Navigator of @p window lists the file @p path outside the book
bool listsFile(const test::Window& window, const QString& path) {
    auto* tree = window.navigatorPanel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == path) {
            return true;
        }
    }
    return false;
}

/// @brief The card of the recent book @p path on the Dashboard of @p window
QWidget* recentBookCard(const test::Window& window, const QString& path) {
    const QList<QWidget*> cards =
        window.dashboard->findChildren<QWidget*>(QStringLiteral("recentFileCard"));
    const auto card = std::find_if(cards.cbegin(), cards.cend(), [&path](const QWidget* widget) {
        return widget->property("filePath").toString() == path;
    });
    REQUIRE(card != cards.cend());
    return *card;
}

/// @brief A click of the mouse @p button in the middle of @p widget
void click(QWidget& widget, Qt::MouseButton button) {
    const QPointF at(widget.width() / 2.0, widget.height() / 2.0);
    QMouseEvent press(QEvent::MouseButtonPress, at, widget.mapToGlobal(at), button, button,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, at, widget.mapToGlobal(at), button,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &release);
}

} // namespace

// ----------------------------------------------------------------------------
// The File menu and the toolbars
// ----------------------------------------------------------------------------

TEST_CASE("New File: the files first, then the books in the File menu, without an Open submenu",
          "[gui][newfile]") {
    gui::registerAllCommands(gui::CommandCallbacks{});
    gui::CommandRegistry& registry = gui::CommandRegistry::getInstance();
    QMainWindow window;
    gui::MenuBuilder builder;
    builder.buildMenuBar(registry, &window);
    QMenu* file = builder.getMenu("FILE");
    REQUIRE(file != nullptr);
    auto* recentBooks = new gui::RecentBooksMenu(file);
    recentBooks->insertInto(file);

    // New File opens a window, so it has an ellipsis; Recent Books follows Open Book
    const QStringList items = itemsOf(*file);
    CHECK(joined(items.mid(0, 10)) ==
          "New File... | Open File... | New Book... | Open Book... | Recent Books | Close Book | "
          "- | Save | Save As... | Save All");
    CHECK_FALSE(items.contains(QStringLiteral("Open")));

    // The shortcuts of Windows
    CHECK(registry.getAction(std::string("file.new"))->shortcut() ==
          QKeySequence(QKeySequence::New));
    CHECK(registry.getAction(std::string("file.open.file"))->shortcut() ==
          QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
}

TEST_CASE("New File: the files first, then the books on the File and Quick Actions toolbars",
          "[gui][newfile]") {
    gui::registerAllCommands(gui::CommandCallbacks{});
    gui::registerAllIcons();
    gui::CommandRegistry& registry = gui::CommandRegistry::getInstance();
    QMainWindow window;
    gui::ToolbarManager manager(&window);
    manager.createToolbars(registry);

    const QStringList file = manager.getToolbarCommands(QStringLiteral("file"));
    CHECK(joined(file) ==
          "file.new | file.open.file | file.new.project | file.open | file.save | file.saveAs | "
          "file.saveAll | file.close");

    // Quick Actions starts with every command of the File toolbar, in its order
    const QStringList quick = manager.getToolbarCommands(QStringLiteral("quickActions"));
    CHECK(joined(quick.mid(0, file.size())) == joined(file));

    // The buttons are the commands of the menu
    QToolBar* toolbar = manager.getToolbar("quickActions");
    REQUIRE(toolbar != nullptr);
    REQUIRE(toolbar->actions().size() > 3);
    CHECK(toolbar->actions().at(1) == registry.getAction(std::string("file.open.file")));
}

TEST_CASE("New File: the Dashboard names the commands to start with in the order of the File "
          "menu",
          "[gui][newfile]") {
    gui::registerAllCommands(gui::CommandCallbacks{});
    gui::DashboardPanel dashboard;
    auto* section = dashboard.findChild<QFrame*>(QStringLiteral("shortcutsFrame"));
    REQUIRE(section != nullptr);

    // The names of the commands, each beside its keys or under them (not the section's title)
    QStringList names;
    for (const QLabel* label : section->findChildren<QLabel*>()) {
        if (label->parentWidget() != section && label->textFormat() == Qt::PlainText) {
            names << label->text();
        }
    }
    CHECK(joined(names) == "Open File | New Book | Open Book");
}

TEST_CASE("New File: a toolbar customized before gets Open File once, where the default one "
          "has it, and the new order of the file commands",
          "[gui][newfile]") {
    gui::registerAllCommands(gui::CommandCallbacks{});
    auto& settings = core::SettingsManager::getInstance();
    // The toolbars of the defaults, for each section
    settings.set<std::string>("toolbars.configurations", "{}");
    settings.set<int>("toolbars.addedCommands", 0);
    QMainWindow window;
    const QString quickActions = QStringLiteral("quickActions");

    SECTION("Quick Actions and the File toolbar of an earlier version") {
        settings.set<std::string>(
            "toolbars.configurations",
            R"({"builtIn":{"quickActions":["file.new","file.open","file.save","edit.find"],)"
            R"("file":["file.new","file.open","file.save","file.close"]}})");
        gui::ToolbarManager manager(&window);
        CHECK(joined(manager.getToolbarCommands(quickActions)) ==
              "file.new | file.open.file | file.open | file.save | file.saveAs | file.close | "
              "edit.find");
        CHECK(joined(manager.getToolbarCommands(QStringLiteral("file"))) ==
              "file.new | file.open.file | file.open | file.save | file.saveAll | file.close");

        // A command the writer removes after that stays removed
        QStringList without = manager.getToolbarCommands(quickActions);
        without.removeAll(QStringLiteral("file.open.file"));
        manager.setToolbarCommands(quickActions, without);
        gui::ToolbarManager next(&window);
        CHECK(joined(next.getToolbarCommands(quickActions)) == joined(without));
    }

    SECTION("without the commands a new command follows in the default toolbar") {
        settings.set<std::string>(
            "toolbars.configurations",
            R"({"builtIn":{"quickActions":["edit.find","file.new.project","file.close"]}})");
        gui::ToolbarManager manager(&window);
        // After the nearest command before it in the default toolbar that the toolbar has
        // (Save As after New Book), at the start when it has none (Open File)
        CHECK(joined(manager.getToolbarCommands(quickActions)) ==
              "file.open.file | edit.find | file.new.project | file.saveAs | file.close");
    }

    SECTION("the file commands in the order of the File menu, in the places they had") {
        // Customized with the commands added before
        settings.set<std::string>(
            "toolbars.configurations",
            R"({"builtIn":{"quickActions":["file.new","file.new.project","file.open",)"
            R"("file.open.file","edit.find"],"file":["file.open","file.save","file.open.file"]}})");
        settings.set<int>("toolbars.addedCommands", 1);
        gui::ToolbarManager manager(&window);
        CHECK(joined(manager.getToolbarCommands(quickActions)) ==
              "file.new | file.open.file | file.new.project | file.open | edit.find");
        CHECK(joined(manager.getToolbarCommands(QStringLiteral("file"))) ==
              "file.open.file | file.save | file.open");

        // An order the writer gives them after that stays
        const QStringList own{QStringLiteral("file.open"), QStringLiteral("file.new"),
                              QStringLiteral("edit.find")};
        manager.setToolbarCommands(quickActions, own);
        gui::ToolbarManager next(&window);
        CHECK(joined(next.getToolbarCommands(quickActions)) == joined(own));
    }

    SECTION("toolbars with the defaults stay as they are") {
        gui::ToolbarManager manager(&window);
        const QStringList quick = manager.getToolbarCommands(quickActions);
        CHECK(quick.count(QStringLiteral("file.open.file")) == 1);
        CHECK(quick.count(QStringLiteral("file.saveAs")) == 1);
        CHECK(quick.count(QStringLiteral("file.close")) == 1);
        CHECK(manager.getToolbarCommands(QStringLiteral("file")).count(
                  QStringLiteral("file.saveAll")) == 1);
        CHECK(settings.get<std::string>("toolbars.configurations") == "{}");
    }
}

// ----------------------------------------------------------------------------
// The New File window
// ----------------------------------------------------------------------------

TEST_CASE("New File: the kinds of the book in its parts, the greyed ones saying why",
          "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    NewFileDialog dialog(NewFileDialog::openBook(), book.pm.bookTypes());

    // The parts of the book have their names in the Navigator. A part (a group) is no file,
    // so it has no tile; a book can have one title page, and the program has no window of
    // the table of contents yet.
    const std::string titlePage = "Title page (greyed): The book already has a title page.";
    const std::string toc = "Table of contents (greyed): The program does not have an editor "
                            "for the table of contents yet.";
    CHECK(joined(rowsOf(*dialog.kindList())) ==
          "# Front Section | " + titlePage + " | Copyright page | Dedication | Epigraph | " +
              toc + " | Preface | # Main Section | Prologue | Chapter | Epilogue | " +
              "# Back Section | Afterword | Acknowledgments | Glossary | About the author | " +
              toc);
}

TEST_CASE("New File: the chosen kind, the title of the new element and its place",
          "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    NewFileDialog dialog(NewFileDialog::openBook(), book.pm.bookTypes());

    // A chapter at the start, the third one of the book, at the end of the main section
    CHECK(dialog.nameLabel()->text().toStdString() == "Chapter");
    CHECK(dialog.whereLabel()->text().toStdString() == "in the main section and in parts");
    CHECK(dialog.countLabel()->text().toStdString() == "any number");
    CHECK(dialog.fileLabel()->text().toStdString() ==
          "chapter_003.kchapter in the folder \"book\"");
    CHECK(dialog.typeLabel()->text().toStdString() == "Novel");
    CHECK(dialog.titleEdit()->text().toStdString() == "Chapter 3");
    CHECK(dialog.titleEdit()->selectedText().toStdString() == "Chapter 3");
    CHECK(dialog.place() == core::ElementPlace{BookPlace::Main, QString(), 2});
    CHECK(dialog.createButton()->isEnabled());

    SECTION("the title follows the kind until the writer types one") {
        choose(dialog, QStringLiteral("Prologue"));
        CHECK(dialog.nameLabel()->text().toStdString() == "Prologue");
        CHECK(dialog.countLabel()->text().toStdString() == "one prologue in the book");
        CHECK(dialog.titleEdit()->text().toStdString() == "Prologue");
        // A prologue opens the body
        CHECK(dialog.place() == core::ElementPlace{BookPlace::Main, QString(), 0});

        eraseText(dialog.titleEdit());
        typeText(dialog.titleEdit(), QStringLiteral("Before the Rain"));
        choose(dialog, QStringLiteral("Epilogue"));
        CHECK(dialog.titleEdit()->text().toStdString() == "Before the Rain");
        CHECK(dialog.place() == core::ElementPlace{BookPlace::Main, QString(), 2});
    }

    SECTION("a kind of another part of the book, with the places of that part") {
        choose(dialog, QStringLiteral("Dedication"));
        CHECK(dialog.whereLabel()->text().toStdString() == "in the front section");
        CHECK(dialog.titleEdit()->text().toStdString() == "Dedication");
        CHECK(dialog.place().place == BookPlace::Front);
        for (const core::ElementPlace& place : dialog.picker()->places()) {
            CHECK(place.place == BookPlace::Front);
        }
    }

    SECTION("no element without a title") {
        eraseText(dialog.titleEdit());
        CHECK_FALSE(dialog.createButton()->isEnabled());
        // An emptied title follows the kind again
        choose(dialog, QStringLiteral("Prologue"));
        CHECK(dialog.titleEdit()->text().toStdString() == "Prologue");
        CHECK(dialog.createButton()->isEnabled());
    }

    SECTION("a greyed tile cannot be chosen") {
        QListWidgetItem* titlePage = tileOf(dialog, QStringLiteral("Title page"));
        REQUIRE(titlePage != nullptr);
        CHECK_FALSE(titlePage->flags().testFlag(Qt::ItemIsSelectable));
        CHECK_FALSE(titlePage->flags().testFlag(Qt::ItemIsEnabled));
    }
}

TEST_CASE("New File: the kind, then the title, then the place; the title in view at the start "
          "also on a small screen",
          "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    NewFileDialog dialog(NewFileDialog::openBook(), book.pm.bookTypes());
    dialog.show();
    QCoreApplication::processEvents();

    // The tiles of the kinds, the title under them and the place under the title; the
    // description at the left
    const auto at = [&dialog](const QWidget* widget) {
        return widget->mapTo(&dialog, QPoint(0, 0));
    };
    CHECK(at(dialog.kindList()).y() < at(dialog.titleEdit()).y());
    CHECK(at(dialog.titleEdit()).y() < at(dialog.picker()).y());
    CHECK(at(dialog.nameLabel()).x() < at(dialog.kindList()).x());

    // The title has the focus; Tab goes from the tiles to the title and on to the place
    CHECK(dialog.focusWidget() == dialog.titleEdit());
    const auto nextOf = [](QWidget* widget) {
        QWidget* next = widget->nextInFocusChain();
        while (next != widget &&
               ((next->focusPolicy() & Qt::TabFocus) == 0 || !next->isVisible())) {
            next = next->nextInFocusChain();
        }
        return next;
    };
    CHECK(nextOf(dialog.kindList()) == dialog.titleEdit());
    CHECK(dialog.picker()->isAncestorOf(nextOf(dialog.titleEdit())));

    // A window lower than the tiles and the place, as on 1366x768 at 150%: the content
    // scrolls, and the title is in view at the start
    dialog.resize(dialog.width(), 360);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    REQUIRE(dialog.height() == 360);
    auto* area = dialog.findChild<gui::FittingScrollArea*>();
    REQUIRE(area != nullptr);
    CHECK(area->verticalScrollBar()->isVisible());
    CHECK(area->verticalScrollBar()->value() == 0);
    const QLineEdit* title = dialog.titleEdit();
    const QRect inView(title->mapTo(area->viewport(), QPoint(0, 0)), title->size());
    INFO("title at " << inView.y() << ".." << inView.bottom() << " in a view of "
                     << area->viewport()->height());
    CHECK(area->viewport()->rect().contains(inView));
}

TEST_CASE("New File: a click in the proposed title selects it, so that typing replaces it",
          "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    NewFileDialog dialog(NewFileDialog::openBook(), book.pm.bookTypes());
    QLineEdit* title = dialog.titleEdit();

    // The click gives the field the focus, then puts the cursor
    const auto clickIn = [title]() {
        title->deselect();
        QFocusEvent focus(QEvent::FocusIn, Qt::MouseFocusReason);
        QCoreApplication::sendEvent(title, &focus);
        QCoreApplication::processEvents();
    };
    clickIn();
    CHECK(title->selectedText().toStdString() == "Chapter 3");

    // In the writer's own title the click only puts the cursor
    eraseText(title);
    typeText(title, QStringLiteral("The Calm"));
    clickIn();
    CHECK(title->selectedText().isEmpty());
}

TEST_CASE("New File: an empty text file outside the book, or with it in Other Files, outside "
          "its text",
          "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    NewFileDialog dialog(NewFileDialog::openBook(QStringLiteral("Untitled 2")),
                         book.pm.bookTypes());

    // After the parts of the book, the group of the other files
    const QStringList rows = rowsOf(*dialog.kindList());
    CHECK(joined(rows.mid(rows.size() - 2)) == "# Other Files | Empty Text File");

    // A chapter at the start, as before: the list of places, without the buttons of the place
    // of an empty text file
    CHECK(dialog.nameLabel()->text().toStdString() == "Chapter");
    CHECK_FALSE(dialog.outsideButton()->isVisibleTo(&dialog));
    CHECK_FALSE(dialog.insideButton()->isVisibleTo(&dialog));
    CHECK(dialog.picker()->isVisibleTo(&dialog));
    CHECK_FALSE(dialog.placeNote()->isVisibleTo(&dialog));
    CHECK_FALSE(dialog.outsideBook());
    CHECK_FALSE(dialog.emptyFileInBook());

    choose(dialog, QStringLiteral("Empty Text File"));

    SECTION("outside the book at first: an untitled file in a tab of its own") {
        CHECK(dialog.outsideButton()->isVisibleTo(&dialog));
        CHECK(dialog.insideButton()->isVisibleTo(&dialog));
        CHECK(dialog.outsideButton()->isChecked());
        CHECK(dialog.insideButton()->isEnabled());
        CHECK(dialog.outsideBook());
        CHECK_FALSE(dialog.emptyFileInBook());
        CHECK_FALSE(dialog.kind());
        CHECK(dialog.nameLabel()->text().toStdString() == "Empty Text File");
        CHECK(dialog.whereLabel()->text().toStdString() ==
              "outside the book, in a tab of its own");
        CHECK(dialog.countLabel()->text().toStdString() == "any number");
        CHECK(dialog.fileLabel()->text().toStdString() ==
              "chosen when it is saved for the first time");
        CHECK_FALSE(dialog.typeLabel()->isVisibleTo(&dialog));
        CHECK(dialog.titleEdit()->text().toStdString() == "Untitled 2");
        CHECK(dialog.createButton()->isEnabled());

        // No place in the list of the book: the note says where the file goes
        CHECK_FALSE(dialog.picker()->isVisibleTo(&dialog));
        CHECK(dialog.placeNote()->isVisibleTo(&dialog));
        CHECK(dialog.placeNote()->text().toStdString() ==
              "The file opens in a new tab, outside the book. You choose its place and format "
              "when you save it for the first time.");

        // No file without a title
        eraseText(dialog.titleEdit());
        CHECK_FALSE(dialog.createButton()->isEnabled());
    }

    SECTION("in the book: a file of the book in Other Files, which is no part of its text") {
        dialog.insideButton()->click();
        CHECK_FALSE(dialog.outsideBook());
        CHECK(dialog.emptyFileInBook());
        CHECK_FALSE(dialog.kind());
        CHECK(dialog.nameLabel()->text().toStdString() == "Empty Text File");
        CHECK(dialog.whereLabel()->text().toStdString() ==
              "in the book, in \"Other Files\", outside its text");
        CHECK(dialog.countLabel()->text().toStdString() == "any number");
        CHECK(dialog.fileLabel()->text().toStdString() ==
              "file_001.kchapter in the folder \"book\"");
        CHECK_FALSE(dialog.typeLabel()->isVisibleTo(&dialog));
        CHECK(dialog.titleEdit()->text().toStdString() == "Untitled 2");
        CHECK(dialog.createButton()->isEnabled());

        // The list of the book does not choose its place: the note says where it goes
        CHECK_FALSE(dialog.picker()->isVisibleTo(&dialog));
        CHECK(dialog.placeNote()->isVisibleTo(&dialog));
        CHECK(dialog.placeNote()->text().toStdString() ==
              "The file goes to the book's folder and to \"Other Files\" in the Navigator. It "
              "is not part of the text of the book, so the finished book does not have it.");

        // Out of the book again
        dialog.outsideButton()->click();
        CHECK(dialog.outsideBook());
        CHECK(dialog.whereLabel()->text().toStdString() ==
              "outside the book, in a tab of its own");
    }

    SECTION("the writer's title stays wherever the file goes") {
        eraseText(dialog.titleEdit());
        typeText(dialog.titleEdit(), QStringLiteral("Notes"));
        dialog.insideButton()->click();
        CHECK(dialog.titleEdit()->text().toStdString() == "Notes");
        dialog.outsideButton()->click();
        CHECK(dialog.titleEdit()->text().toStdString() == "Notes");
    }

    SECTION("another tile brings back the list of places") {
        dialog.insideButton()->click();
        choose(dialog, QStringLiteral("Prologue"));
        CHECK_FALSE(dialog.outsideButton()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.insideButton()->isVisibleTo(&dialog));
        CHECK(dialog.picker()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.placeNote()->isVisibleTo(&dialog));
        CHECK(dialog.typeLabel()->isVisibleTo(&dialog));
        CHECK(dialog.titleEdit()->text().toStdString() == "Prologue");
        CHECK(dialog.kind().kind->id.toStdString() == "prologue");
        CHECK_FALSE(dialog.outsideBook());
        CHECK_FALSE(dialog.emptyFileInBook());
    }
}

TEST_CASE("New File: choosing the empty text file moves neither the tiles nor the title",
          "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    NewFileDialog dialog(NewFileDialog::openBook(QStringLiteral("Untitled 1")),
                         book.pm.bookTypes());
    dialog.show();
    // In a low window the content scrolls: its scroll bar comes after the first layout
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    const QRect tiles = dialog.kindList()->geometry();
    const QPoint title = dialog.titleEdit()->mapTo(&dialog, QPoint(0, 0));
    for (const char* name : {"Empty Text File", "Chapter", "Empty Text File"}) {
        choose(dialog, QString::fromLatin1(name));
        QCoreApplication::processEvents();
        INFO(name);
        CHECK(dialog.kindList()->geometry() == tiles);
        CHECK(dialog.titleEdit()->mapTo(&dialog, QPoint(0, 0)) == title);
    }
}

TEST_CASE("New File: the description of the kind keeps all its lines in a low window",
          "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    NewFileDialog dialog(NewFileDialog::openBook(), book.pm.bookTypes());
    dialog.show();
    QCoreApplication::processEvents();

    // From the default height to a window lower than the content, which scrolls then: the
    // lists give up their rows, and the lines of the description stay whole
    for (const int height : {740, 640, 560, 520, 480, 440, 400, 360}) {
        dialog.resize(dialog.width(), height);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        for (const QLabel* label : {dialog.nameLabel(), dialog.whereLabel(),
                                    dialog.countLabel(), dialog.fileLabel(),
                                    dialog.typeLabel()}) {
            INFO(height << ": " << label->text().toStdString());
            CHECK(label->height() >= label->heightForWidth(label->width()));
        }
    }
}

TEST_CASE("New File: Create adds the element at its place and opens it in the editor",
          "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    test::Window window;

    QStringList others;
    const int shown = pressing(
        [&window]() { window.documents.onNewDocument(); },
        [](NewFileDialog& dialog) {
            eraseText(dialog.titleEdit());
            typeText(dialog.titleEdit(), QStringLiteral("The Calm"));
            // Before the first chapter
            dialog.picker()->setPlace(core::ElementPlace{BookPlace::Main, QString(), 0});
            return dialog.createButton();
        },
        others);
    INFO(others.join(QStringLiteral("\n")).toStdString());
    CHECK(shown == 1);
    CHECK(others.isEmpty());

    // In the book, where the writer put it, with its file
    CHECK(titlesOf(book.pm.project()->elementsIn(BookPlace::Main)) ==
          "The Calm, Chapter 1, The Storm");
    const core::ProjectElement calm = book.pm.project()->elementsIn(BookPlace::Main).first();
    CHECK(calm.kind.kindId == QStringLiteral("chapter"));
    CHECK(QFileInfo::exists(book.pm.filePathOf(calm)));

    // Open in the editor
    REQUIRE(window.tabs->count() > 0);
    const int current = window.tabs->currentIndex();
    CHECK(window.tabs->tabText(current) == QStringLiteral("The Calm"));
    CHECK(window.editor(current)->property("elementId").toString() == calm.id);
}

TEST_CASE("New File: Cancel adds nothing", "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    test::Window window;
    const qsizetype elements = book.pm.project()->elementsIn(BookPlace::Main).size();

    QStringList others;
    const int shown = pressing([&window]() { window.documents.onNewDocument(); },
                               [](NewFileDialog& dialog) { return cancelButton(dialog); }, others);
    INFO(others.join(QStringLiteral("\n")).toStdString());
    CHECK(shown == 1);
    CHECK(others.isEmpty());

    CHECK(book.pm.project()->elementsIn(BookPlace::Main).size() == elements);
    CHECK(window.tabs->count() == 0);
}

TEST_CASE("New File: an empty text file outside the open book opens in a tab of its own and "
          "in the Navigator at once",
          "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    test::Window window;
    const std::string elements = titlesOf(book.pm.project()->elementsIn(BookPlace::Main));

    QStringList others;
    const auto emptyFile = [](NewFileDialog& dialog) {
        choose(dialog, QStringLiteral("Empty Text File"));
        return dialog.createButton();
    };
    CHECK(pressing([&window]() { window.documents.onNewDocument(); }, emptyFile, others) == 1);
    CHECK(pressing([&window]() { window.documents.onNewDocument(); }, emptyFile, others) == 1);
    INFO(others.join(QStringLiteral("\n")).toStdString());
    CHECK(others.isEmpty());

    // Untitled files, numbered as without a book; the book as it was
    REQUIRE(window.tabs->count() == 2);
    CHECK(window.tabs->tabText(0) == QStringLiteral("Untitled 1"));
    CHECK(window.tabs->tabText(1) == QStringLiteral("Untitled 2"));
    CHECK(window.tabs->currentIndex() == 1);
    CHECK(window.editor(1)->property("elementId").toString().isEmpty());
    CHECK(titlesOf(book.pm.project()->elementsIn(BookPlace::Main)) == elements);

    // In the Navigator at once, with the other files outside the book, without the "*" of a
    // changed file while nothing is written in them; the bar says that the file has no file yet
    CHECK(joined(navigatorItems(window, "standalone_file")) == "Untitled 1 | Untitled 2");
    CHECK_FALSE(window.infoBar->isHidden());
    CHECK(window.infoBar->message() == QStringLiteral("New file – not saved yet"));

    // The first character typed gives the tab and the Navigator the "*"
    typeInto(*window.editor(1), QLatin1Char('A'));
    CHECK(window.tabs->tabText(1) == QStringLiteral("*Untitled 2"));
    CHECK(joined(navigatorItems(window, "standalone_file")) == "Untitled 1 | *Untitled 2");

    // A double click on the file in the Navigator shows its tab
    auto* tree = window.navigatorPanel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    QTreeWidgetItem* first = navigatorItem(window, QStringLiteral("Untitled 1"));
    REQUIRE(first != nullptr);
    emit tree->itemActivated(first, 0);
    CHECK(window.tabs->currentIndex() == 0);

    // The file leaves the Navigator with its tab, closed as the main window closes it
    gui::EditorPanel* closed = window.editor(0);
    window.tabs->removeTab(0);
    closed->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(joined(navigatorItems(window, "standalone_file")) == "*Untitled 2");
}

TEST_CASE("New File: an empty text file in the open book goes to Other Files, outside its text",
          "[gui][newfile]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    test::Window window;

    QStringList others;
    const int shown = pressing(
        [&window]() { window.documents.onNewDocument(); },
        [](NewFileDialog& dialog) {
            choose(dialog, QStringLiteral("Empty Text File"));
            dialog.insideButton()->click();
            eraseText(dialog.titleEdit());
            typeText(dialog.titleEdit(), QStringLiteral("Notes"));
            return dialog.createButton();
        },
        others);
    INFO(others.join(QStringLiteral("\n")).toStdString());
    CHECK(shown == 1);
    CHECK(others.isEmpty());

    // No element of the text of the book: a file of the book, with no kind, in its folder
    CHECK(titlesOf(book.pm.project()->elementsIn(BookPlace::Main)) == "Chapter 1, The Storm");
    REQUIRE(book.pm.book() != nullptr);
    REQUIRE(book.pm.book()->files.size() == 1);
    const core::ProjectElement notes = book.pm.book()->files.first();
    CHECK(notes.title == QStringLiteral("Notes"));
    CHECK_FALSE(notes.hasKind());
    CHECK(notes.file == QStringLiteral("book/file_001.kchapter"));
    CHECK(QFileInfo::exists(book.pm.filePathOf(notes)));
    CHECK(book.pm.isBookFile(notes.id));

    // Open in the editor
    REQUIRE(window.tabs->count() > 0);
    const int current = window.tabs->currentIndex();
    CHECK(window.tabs->tabText(current) == QStringLiteral("Notes"));
    CHECK(window.editor(current)->property("elementId").toString() == notes.id);

    // In the Navigator in "Other Files" of the book, not among the files outside it
    CHECK(joined(navigatorItems(window, "book_files")) == "Other Files");
    CHECK(joined(navigatorItems(window, "book_file")) == "Notes");
    CHECK(navigatorItems(window, "standalone_file").isEmpty());

    // Its properties: its title and its words, without a status and notes
    window.properties->showChapterProperties(notes.id);
    const QList<QComboBox*> boxes = window.properties->findChildren<QComboBox*>();
    const auto status = std::find_if(boxes.cbegin(), boxes.cend(), [](const QComboBox* box) {
        return box->findData(QStringLiteral("draft")) >= 0;
    });
    REQUIRE(status != boxes.cend());
    CHECK_FALSE((*status)->isVisibleTo(window.properties));
    for (const QTextEdit* edit : window.properties->findChildren<QTextEdit*>()) {
        if (edit->placeholderText() == QStringLiteral("Enter notes...")) {
            CHECK_FALSE(edit->isVisibleTo(window.properties));
        }
    }

    // A chapter has them again
    window.properties->showChapterProperties(book.chapterId);
    CHECK((*status)->isVisibleTo(window.properties));
}

// ----------------------------------------------------------------------------
// Without a book
// ----------------------------------------------------------------------------

TEST_CASE("New File: without a book, an untitled file saved where the writer chooses",
          "[gui][newfile]") {
    REQUIRE_FALSE(core::ProjectManager::getInstance().isProjectOpen());
    test::Window window;

    window.documents.onNewDocument();
    REQUIRE(window.tabs->count() == 1);
    CHECK(window.tabs->tabText(0) == QStringLiteral("Untitled 1"));
    gui::EditorPanel* story = window.editor(0);
    CHECK(window.documents.unsavedDocumentNames().isEmpty());

    // In the Navigator at once, with the bar that says it has no file yet
    CHECK(joined(navigatorItems(window, "standalone_file")) == "Untitled 1");
    CHECK_FALSE(window.infoBar->isHidden());
    CHECK(window.infoBar->message() == QStringLiteral("New file – not saved yet"));

    // A change gets the "*" of a changed file, in the tab and in the Navigator
    test::write(*story, QStringLiteral("Once upon a time"));
    CHECK(window.tabs->tabText(0) == QStringLiteral("*Untitled 1"));
    CHECK(joined(navigatorItems(window, "standalone_file")) == "*Untitled 1");
    CHECK(window.documents.unsavedDocumentNames() == QStringList{QStringLiteral("Untitled 1")});

    window.documents.onNewDocument();
    REQUIRE(window.tabs->count() == 2);
    CHECK(window.tabs->tabText(1) == QStringLiteral("Untitled 2"));
    CHECK(window.tabs->currentIndex() == 1);

    // The first save asks where and in which format, in the folder of Open File, with the
    // name of the tab
    OwnFileDialogs own;
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    gui::ProgramFolders::remember(gui::ProgramFolders::Operation::OpenFile, folder.path());
    window.tabs->setCurrentIndex(0);
    const QString path = QDir(folder.path()).filePath(QStringLiteral("Story.kchapter"));
    QString proposed;
    QStringList types;
    CHECK(savingAs([&window]() { window.documents.onSaveDocument(); }, path, &proposed,
                   &types) == 1);
    CHECK(QFileInfo(proposed) ==
          QFileInfo(QDir(folder.path()).filePath(QStringLiteral("Untitled 1.kchapter"))));
    CHECK(joined(types) == "Kalahari Documents (*.kchapter) | Text Files (*.txt)");

    // Saved, under its new name, and in the Navigator by its file with the other files outside
    // a book
    CHECK(QFileInfo::exists(path));
    CHECK(window.tabs->tabText(0) == QStringLiteral("Story.kchapter"));
    CHECK_FALSE(story->property("dirty").toBool());
    CHECK(listsFile(window, path));
    CHECK(joined(navigatorItems(window, "standalone_file")) == "Untitled 2 | Story.kchapter");
    CHECK(window.infoBar->message() ==
          QStringLiteral("This file is not part of a book. Some features are not available."));

    // The next save writes the file without asking
    test::write(*story, QStringLiteral("Once upon a time there was a storm"));
    CHECK(savingAs([&window]() { window.documents.onSaveDocument(); }, path, nullptr) == 0);
    CHECK(window.tabs->tabText(0) == QStringLiteral("Story.kchapter"));

    // The bar of a new file saves it as well, as no book can take it (test report of
    // 17 October, part 8)
    window.tabs->setCurrentIndex(1);
    auto* button = window.infoBar->findChild<QPushButton*>();
    REQUIRE(button != nullptr);
    CHECK(button->isEnabled());
    CHECK(button->text() == QStringLiteral("Save As..."));
    CHECK(button->toolTip() == QStringLiteral("Save the new file on the disk"));
    const QString notes = QDir(folder.path()).filePath(QStringLiteral("Notes.kchapter"));
    CHECK(savingAs([button]() { button->click(); }, notes, nullptr) == 1);
    CHECK(QFileInfo::exists(notes));
    CHECK(window.tabs->tabText(1) == QStringLiteral("Notes.kchapter"));

    // Saved, it is a file on the disk that only an open book can take
    CHECK(button->text() == QStringLiteral("Add to Book..."));
    CHECK_FALSE(button->isEnabled());
    CHECK(button->toolTip() == QStringLiteral("No book is open"));
}

TEST_CASE("New File: Add to Book puts a new file into the open book at once, asking nothing",
          "[gui][newfile]") {
    test::RecentBooks keepRecentBooks;
    auto& pm = core::ProjectManager::getInstance();
    QTemporaryDir books;
    REQUIRE(books.isValid());
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(books.path(), QStringLiteral("Rain"), QStringLiteral("Author"),
                             QStringLiteral("en"), true, QStringLiteral("kalahari.novel")));
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));

    // A new file written before the book opens (the flag outlives the window and its tabs)
    bool fileClosed = false;
    test::Window window;
    // The window follows the book as the main window does, its info bar too
    QObject::connect(&pm, &core::ProjectManager::projectOpened, &window.documents,
                     &gui::DocumentCoordinator::onProjectOpened);
    QObject::connect(&pm, &core::ProjectManager::projectAboutToClose, &window.documents,
                     &gui::DocumentCoordinator::prepareForProjectClose);
    QObject::connect(&pm, &core::ProjectManager::projectClosed, &window.documents,
                     &gui::DocumentCoordinator::onProjectClosed);
    window.documents.onNewDocument();
    gui::EditorPanel* file = window.editor(0);
    QObject::connect(file, &QObject::destroyed, [&fileClosed]() { fileClosed = true; });
    test::write(*file, QStringLiteral("Ideas for a new book"));
    // Without a book the file can go into none: the button of its info bar saves it on the
    // disk (round 3 report 19.2; test report of 17 October, part 8)
    CHECK_FALSE(window.infoBar->isHidden());
    auto* button = window.infoBar->findChild<QPushButton*>();
    REQUIRE(button != nullptr);
    CHECK(button->isEnabled());
    CHECK(button->text() == QStringLiteral("Save As..."));
    REQUIRE(test::accepting([&]() { window.documents.onOpenRecentFile(manifest); }) == 0);
    REQUIRE(pm.isProjectOpen());
    window.tabs->setCurrentWidget(file);
    CHECK_FALSE(window.infoBar->isHidden());
    CHECK(button->isEnabled());
    // No window follows, so no ellipsis
    CHECK(button->text() == QStringLiteral("Add to Book"));

    // No window to save the file, no question whether to copy or move it
    QStringList said;
    CHECK(test::accepting([&]() { window.documents.onAddToProject(); }, &said) == 0);
    INFO(said.join(QStringLiteral("\n")).toStdString());

    // A chapter at the end of the main text, with the title of a new chapter and the text of
    // the file, in a file of its own in the book's folder
    const QList<core::ProjectElement>& main = pm.project()->elementsIn(BookPlace::Main);
    REQUIRE_FALSE(main.isEmpty());
    const core::ProjectElement& added = main.last();
    CHECK(added.title == QStringLiteral("Chapter 2"));
    const QString addedPath = pm.filePathOf(added);
    CHECK(addedPath.startsWith(QFileInfo(manifest).absolutePath()));
    const std::optional<core::ChapterDocument> chapter = core::ChapterDocument::load(addedPath);
    REQUIRE(chapter.has_value());
    CHECK(core::ChapterDocument::kmlToPlainText(chapter->kml()).trimmed() ==
          QStringLiteral("Ideas for a new book"));

    // The chapter takes the place of the file's tab, which goes with its info bar
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(fileClosed);
    CHECK(window.tabs->tabText(window.tabs->currentIndex()) == QStringLiteral("Chapter 2"));
    CHECK(window.editor(window.tabs->currentIndex())->getText().trimmed() ==
          QStringLiteral("Ideas for a new book"));
    CHECK(window.infoBar->isHidden());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("Chapter added to the book: Chapter 2"));

    // A file on the disk goes to the book in the Add to Book window, which asks whether to copy
    // or move it
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notePath = QDir(files.path()).filePath(QStringLiteral("note.txt"));
    {
        QFile note(notePath);
        REQUIRE(note.open(QIODevice::WriteOnly));
        note.write("A note");
    }
    window.documents.openStandaloneFile(notePath);
    CHECK(window.infoBar->filePath() == notePath);
    CHECK(button->text() == QStringLiteral("Add to Book..."));
    said.clear();
    CHECK(test::accepting([&]() { window.documents.onAddToProject(); }, &said) == 1);
    CHECK(said.join(QStringLiteral("\n")).contains(QStringLiteral("Add the File to the Book")));
    pm.closeProject(false);
}

TEST_CASE("New File: a book opened from Recent Books leaves the changed files outside it open "
          "without asking",
          "[gui][newfile]") {
    test::RecentBooks keepRecentBooks;
    auto& pm = core::ProjectManager::getInstance();
    QTemporaryDir books;
    REQUIRE(books.isValid());
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(books.path(), QStringLiteral("Rain"), QStringLiteral("Author"),
                             QStringLiteral("en"), true, QStringLiteral("kalahari.novel")));
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));

    test::Window window;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notePath = QDir(files.path()).filePath(QStringLiteral("note.txt"));
    {
        QFile note(notePath);
        REQUIRE(note.open(QIODevice::WriteOnly));
        note.write("A note");
    }
    gui::EditorPanel* note = window.writeFile(notePath, QStringLiteral("A changed note"));

    QStringList said;
    CHECK(test::accepting([&]() { window.documents.onOpenRecentFile(manifest); }, &said) == 0);
    INFO(said.join(QStringLiteral("\n")).toStdString());
    CHECK(pm.isProjectOpen());
    CHECK(window.tabs->indexOf(note) >= 0);
    CHECK(note->property("dirty").toBool());
    pm.closeProject(false);
}

TEST_CASE("New File: a book opened from its card on the Dashboard leaves the changed files "
          "outside it open without asking",
          "[gui][newfile]") {
    test::RecentBooks keepRecentBooks;
    auto& pm = core::ProjectManager::getInstance();
    QTemporaryDir books;
    REQUIRE(books.isValid());
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(books.path(), QStringLiteral("Rain"), QStringLiteral("Author"),
                             QStringLiteral("en"), true, QStringLiteral("kalahari.novel")));
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    core::RecentBooksManager::getInstance().addRecentFile(manifest);

    // The card opens the book as in the main window
    test::Window window;
    QObject::connect(window.dashboard, &gui::DashboardPanel::openRecentBookRequested,
                     &window.documents, &gui::DocumentCoordinator::onOpenRecentFile);
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notePath = QDir(files.path()).filePath(QStringLiteral("note.txt"));
    {
        QFile note(notePath);
        REQUIRE(note.open(QIODevice::WriteOnly));
        note.write("A note");
    }
    gui::EditorPanel* note = window.writeFile(notePath, QStringLiteral("A changed note"));

    // A right click opens nothing; a left click opens the book, and asks nothing
    QWidget* card = recentBookCard(window, manifest);
    QStringList said;
    CHECK(test::accepting([&]() { click(*card, Qt::RightButton); }, &said) == 0);
    CHECK_FALSE(pm.isProjectOpen());
    CHECK(test::accepting([&]() { click(*card, Qt::LeftButton); }, &said) == 0);
    INFO(said.join(QStringLiteral("\n")).toStdString());
    CHECK(pm.isProjectOpen());
    CHECK(pm.getManifestPath() == manifest);
    CHECK(window.tabs->indexOf(note) >= 0);
    CHECK(note->property("dirty").toBool());
    pm.closeProject(false);
}

// =============================================================================
// Add Element: the window at a place of the Navigator or of the editor
// =============================================================================

namespace {

/// @brief The window of files that chooses the file of a new material, in place of the
/// system's until the test ends: it chooses @c file
struct MaterialChooser {
    QString file;         ///< The file it chooses; empty: Cancel
    QStringList folders;  ///< The folders it was opened in

    explicit MaterialChooser(QString chosen) : file(std::move(chosen)) {
        NewFileDialog::setFileChooserForTests([this](const QString& folder) {
            folders << folder;
            return file;
        });
    }
    ~MaterialChooser() { NewFileDialog::setFileChooserForTests({}); }
    MaterialChooser(const MaterialChooser&) = delete;
    MaterialChooser& operator=(const MaterialChooser&) = delete;
};

/// @brief Add Element on @p scope of the book, @p id the group or the element of the scope
gui::AddTarget addingOn(gui::AddTarget::Scope scope, const QString& id = QString(),
                        BookPlace place = BookPlace::Main) {
    gui::AddTarget target;
    target.scope = scope;
    target.id = id;
    target.place = place;
    return target;
}

/// @brief The kinds of Add Element opened on @p target without a choice at its top: those of
/// the place only
NewFileDialog::Book withoutChoice(const gui::AddTarget& target) {
    return NewFileDialog::forAdding(target, gui::AddChooser::None);
}

/// @brief The rows of the kinds that can be chosen in the list of @p dialog
QStringList enabledRowsOf(const NewFileDialog& dialog) {
    QStringList rows;
    for (const QString& row : rowsOf(*dialog.kindList())) {
        if (!row.contains(QStringLiteral(" (greyed): "))) {
            rows << row;
        }
    }
    return rows;
}

} // namespace

TEST_CASE("Add Element: the kinds of the place it was opened on, the usual one chosen",
          "[gui][newfile][addelement]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    const core::BookTypeRegistry& types = book.pm.bookTypes();
    REQUIRE(book.pm.book() != nullptr);
    const QString chapterOne = book.pm.book()->mainElements.first().id;

    SECTION("the book: the kinds of all its sections, the chapter at the end of the body") {
        NewFileDialog dialog(withoutChoice(addingOn(gui::AddTarget::Scope::Book)), types);
        CHECK(dialog.windowTitle() == QStringLiteral("Add Element"));
        CHECK(test::textsOf(dialog).contains(
            QStringLiteral("Choose the kind of the new element, its title, status and place.")));
        CHECK(joined(enabledRowsOf(dialog)) ==
              "# Front Section | Copyright page | Dedication | Epigraph | Preface | "
              "# Main Section | Prologue | Part | Chapter | Epilogue | # Back Section | "
              "Afterword | Acknowledgments | Glossary | About the author");
        REQUIRE(dialog.kind());
        CHECK(dialog.kind().kind->id == QStringLiteral("chapter"));
        CHECK(dialog.place() == core::ElementPlace{BookPlace::Main, QString(), 2});
        CHECK(dialog.titleEdit()->text() == QStringLiteral("Chapter 3"));

        // A text of the book has a status, a draft at the start; a part has none
        CHECK(dialog.statusBox()->isVisibleTo(&dialog));
        CHECK(dialog.status() == QStringLiteral("draft"));
        dialog.statusBox()->setCurrentIndex(dialog.statusBox()->findData(QStringLiteral("final")));
        CHECK(dialog.status() == QStringLiteral("final"));
        choose(dialog, QStringLiteral("Part"));
        CHECK_FALSE(dialog.statusBox()->isVisibleTo(&dialog));
        CHECK(dialog.status().isEmpty());
        CHECK(test::textsOf(dialog).contains(
            QStringLiteral("Choose the kind of the new element, its title and place.")));
    }

    SECTION("a section: its kinds only") {
        NewFileDialog dialog(
            withoutChoice(addingOn(gui::AddTarget::Scope::Section, QString(), BookPlace::Back)),
            types);
        CHECK(joined(enabledRowsOf(dialog)) ==
              "# Back Section | Afterword | Acknowledgments | Glossary | About the author");
        REQUIRE(dialog.kind());
        CHECK(dialog.kind().kind->id == QStringLiteral("afterword"));
        CHECK(dialog.place().place == BookPlace::Back);
    }

    SECTION("a part: what goes inside it (an epigraph opens a part), at its end") {
        const QString part =
            book.pm.addElement(book.pm.partKind(), QStringLiteral("Part One"), BookPlace::Main);
        REQUIRE_FALSE(part.isEmpty());
        NewFileDialog dialog(withoutChoice(addingOn(gui::AddTarget::Scope::Group, part)), types);
        CHECK(joined(enabledRowsOf(dialog)) ==
              "# In the part \"Part One\" | Prologue | Chapter | Epilogue | Epigraph");
        REQUIRE(dialog.kind());
        CHECK(dialog.kind().kind->id == QStringLiteral("chapter"));
        CHECK(dialog.place() == core::ElementPlace{BookPlace::Main, part, 0});
    }

    SECTION("an element: the kinds of its list, right after it") {
        NewFileDialog dialog(
            withoutChoice(addingOn(gui::AddTarget::Scope::Element, chapterOne)), types);
        REQUIRE(dialog.kind());
        CHECK(dialog.kind().kind->id == QStringLiteral("chapter"));
        CHECK(dialog.place() == core::ElementPlace{BookPlace::Main, QString(), 1});
        CHECK(enabledRowsOf(dialog).contains(QStringLiteral("Part")));
        CHECK_FALSE(enabledRowsOf(dialog).contains(QStringLiteral("Dedication")));
    }

    SECTION("the kind chosen at the start, where an element of it can be made") {
        gui::AddTarget target = addingOn(gui::AddTarget::Scope::Book);
        target.kind = types.findKind(QStringLiteral("kalahari.novel"), QStringLiteral("prologue"));
        REQUIRE(target.kind);
        NewFileDialog dialog(withoutChoice(target), types);
        REQUIRE(dialog.kind());
        CHECK(dialog.kind().kind->id == QStringLiteral("prologue"));
        CHECK(dialog.place() == core::ElementPlace{BookPlace::Main, QString(), 0});
    }
}

TEST_CASE("Add Element: a new part takes the elements that close the main section, or leaves "
          "them",
          "[gui][newfile][addelement]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    const core::BookTypeRegistry& types = book.pm.bookTypes();
    // The epilogue closes the main section at the end of its only part
    const QString part =
        book.pm.addElement(book.pm.partKind(), QStringLiteral("Part One"), BookPlace::Main);
    REQUIRE_FALSE(part.isEmpty());
    const QString epilogue = book.pm.addElement(
        types.findKind(QStringLiteral("kalahari.novel"), QStringLiteral("epilogue")),
        QStringLiteral("Epilogue"), BookPlace::Main, part);
    REQUIRE_FALSE(epilogue.isEmpty());

    gui::AddTarget target = addingOn(gui::AddTarget::Scope::Book);
    target.kind = book.pm.partKind();
    NewFileDialog dialog(withoutChoice(target), types);
    REQUIRE(dialog.kind());
    CHECK(dialog.kind().kind->id == QStringLiteral("part"));
    CHECK(dialog.takeBox()->isVisibleTo(&dialog));
    CHECK(dialog.takeBox()->isChecked());
    CHECK(dialog.takeInside() == QStringList{epilogue});
    dialog.takeBox()->setChecked(false);
    CHECK(dialog.takeInside().isEmpty());

    // A chapter takes nothing
    choose(dialog, QStringLiteral("Chapter"));
    CHECK_FALSE(dialog.takeBox()->isVisibleTo(&dialog));
    CHECK(dialog.takeInside().isEmpty());
}

TEST_CASE("Add Element: in the Workshop a working note and a material, without places and "
          "statuses",
          "[gui][newfile][addelement]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    const core::BookTypeRegistry& types = book.pm.bookTypes();

    SECTION("the kinds of the Workshop; those without an editor say why") {
        NewFileDialog dialog(withoutChoice(addingOn(gui::AddTarget::Scope::Workshop)), types);
        CHECK(test::textsOf(dialog).contains(
            QStringLiteral("Choose the kind of the new element of the Workshop and its title.")));
        CHECK(joined(enabledRowsOf(dialog)) == "# Workshop | Working note | Material");
        CHECK(rowsOf(*dialog.kindList()).contains(
            QStringLiteral("Mind map (greyed): The program does not have an editor for the mind "
                           "map yet.")));
        REQUIRE(dialog.kind());
        CHECK(dialog.kind().kind->id == QStringLiteral("work_note"));
        CHECK(dialog.titleEdit()->text() == QStringLiteral("Note 1"));
        CHECK_FALSE(dialog.statusBox()->isVisibleTo(&dialog));
        CHECK(dialog.status().isEmpty());
        CHECK_FALSE(dialog.picker()->isVisibleTo(&dialog));
        CHECK(dialog.placeNote()->text() ==
              QStringLiteral("In the Workshop the elements are in alphabetical order, so the "
                             "working note goes to the group \"Working notes\" at the place of "
                             "its title."));
        CHECK(dialog.place().place == BookPlace::Workshop);
    }

    SECTION("the kinds of a group of the Workshop only") {
        gui::AddTarget target = addingOn(gui::AddTarget::Scope::Workshop);
        target.kinds = {types.findKind(QStringLiteral("kalahari.base"), QStringLiteral("work_note"))};
        NewFileDialog dialog(withoutChoice(target), types);
        CHECK(joined(enabledRowsOf(dialog)) == "# Workshop | Working note");
    }

    SECTION("a material is a copy of the file chosen, named after it") {
        QTemporaryDir files;
        REQUIRE(files.isValid());
        const QString picture = QDir(files.path()).filePath(QStringLiteral("Old Map.png"));
        MaterialChooser chooser(QString{});
        NewFileDialog dialog(withoutChoice(addingOn(gui::AddTarget::Scope::Workshop)), types);
        choose(dialog, QStringLiteral("Material"));
        CHECK(test::textsOf(dialog).contains(
            QStringLiteral("Choose the file of the material and its title.")));
        CHECK(dialog.materialLabel()->isVisibleTo(&dialog));
        CHECK(dialog.chooseButton()->isVisibleTo(&dialog));

        // No file chosen yet: nothing to add
        dialog.chooseButton()->click();
        CHECK(dialog.materialFile().isEmpty());
        CHECK(dialog.titleEdit()->text().isEmpty());
        CHECK_FALSE(dialog.createButton()->isEnabled());

        // The file chosen gives its title; the writer's own title stays
        chooser.file = picture;
        dialog.chooseButton()->click();
        CHECK(dialog.materialFile() == QFileInfo(picture).absoluteFilePath());
        CHECK(dialog.titleEdit()->text() == QStringLiteral("Old Map"));
        CHECK(dialog.createButton()->isEnabled());
        eraseText(dialog.titleEdit());
        typeText(dialog.titleEdit(), QStringLiteral("The Map"));
        dialog.chooseButton()->click();
        CHECK(dialog.titleEdit()->text() == QStringLiteral("The Map"));
        // The second time the window opens in the folder of the file
        REQUIRE(chooser.folders.size() == 3);
        CHECK(chooser.folders.at(2) == QFileInfo(picture).absolutePath());

        // Another kind has no file
        choose(dialog, QStringLiteral("Working note"));
        CHECK(dialog.materialFile().isEmpty());
        CHECK_FALSE(dialog.materialLabel()->isVisibleTo(&dialog));
    }
}

namespace {

/// @brief The texts of the buttons at the top of Add Element, with their underlined letters
QStringList choicesOf(const NewFileDialog& dialog) {
    QStringList texts;
    if (dialog.chooserButtons()) {
        for (const QAbstractButton* button : dialog.chooserButtons()->buttons()) {
            texts << button->text();
        }
    }
    return texts;
}

/// @brief The button at the top of Add Element chosen now: its text without the underline
QString chosenOf(const NewFileDialog& dialog) {
    const QAbstractButton* button =
        dialog.chooserButtons() ? dialog.chooserButtons()->checkedButton() : nullptr;
    return button ? gui::withoutAccessKey(button->text()) : QString();
}

/// @brief The button at the top of Add Element named @p name, without the underline
QAbstractButton* choiceOf(const NewFileDialog& dialog, const QString& name) {
    for (QAbstractButton* button : dialog.chooserButtons()->buttons()) {
        if (gui::withoutAccessKey(button->text()) == name) {
            return button;
        }
    }
    return nullptr;
}

/// @brief The tile chosen in the list of @p dialog
QString chosenTileOf(const NewFileDialog& dialog) {
    const QListWidgetItem* item = dialog.kindList()->currentItem();
    return item ? item->text() : QString();
}

/// @brief Add Element on @p target with the sections of the book and the Workshop at its top
NewFileDialog::Book bySections(const gui::AddTarget& target) {
    return NewFileDialog::forAdding(target, gui::AddChooser::Sections);
}

} // namespace

TEST_CASE("Add Element: the sections of the book and the Workshop at the top, one at a time",
          "[gui][newfile][addelement]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    const core::BookTypeRegistry& types = book.pm.bookTypes();
    REQUIRE(book.pm.book() != nullptr);
    const QString chapterOne = book.pm.book()->mainElements.first().id;

    SECTION("the book: the main section with a chapter at the end; the others a click away") {
        NewFileDialog dialog(bySections(addingOn(gui::AddTarget::Scope::Book)), types);
        // The letters do not repeat those of the title, the status and the file
        CHECK(choicesOf(dialog) == QStringList{"&Front Section", "&Main Section", "&Back Section",
                                               "&Workshop"});
        CHECK(chosenOf(dialog) == QStringLiteral("Main Section"));
        CHECK(joined(rowsOf(*dialog.kindList())) == "Prologue | Part | Chapter | Epilogue");
        REQUIRE(dialog.kind());
        CHECK(dialog.kind().kind->id == QStringLiteral("chapter"));
        CHECK(dialog.place() == core::ElementPlace{BookPlace::Main, QString(), 2});
        CHECK(dialog.titleEdit()->text() == QStringLiteral("Chapter 3"));

        // Another section: its kinds, the first that can be made, at its place
        QAbstractButton* back = choiceOf(dialog, QStringLiteral("Back Section"));
        REQUIRE(back != nullptr);
        back->click();
        CHECK(chosenOf(dialog) == QStringLiteral("Back Section"));
        CHECK(joined(enabledRowsOf(dialog)) ==
              "Afterword | Acknowledgments | Glossary | About the author");
        CHECK(chosenTileOf(dialog) == QStringLiteral("Afterword"));
        CHECK(dialog.place().place == BookPlace::Back);
        CHECK(dialog.titleEdit()->text() == QStringLiteral("Afterword"));
        CHECK(dialog.titleEdit()->selectedText() == dialog.titleEdit()->text());

        // Each section keeps the kind chosen in it
        choose(dialog, QStringLiteral("Glossary"));
        choiceOf(dialog, QStringLiteral("Main Section"))->click();
        CHECK(chosenTileOf(dialog) == QStringLiteral("Chapter"));
        CHECK(dialog.place() == core::ElementPlace{BookPlace::Main, QString(), 2});
        back->click();
        CHECK(chosenTileOf(dialog) == QStringLiteral("Glossary"));

        // The front section: the title page the book has is greyed
        choiceOf(dialog, QStringLiteral("Front Section"))->click();
        CHECK(rowsOf(*dialog.kindList()).first().startsWith(QStringLiteral("Title page (greyed)")));
        CHECK(chosenTileOf(dialog) == QStringLiteral("Copyright page"));
        CHECK(dialog.place().place == BookPlace::Front);

        // The Workshop: its kinds without places and statuses
        choiceOf(dialog, QStringLiteral("Workshop"))->click();
        CHECK(joined(enabledRowsOf(dialog)) == "Working note | Material");
        CHECK(chosenTileOf(dialog) == QStringLiteral("Working note"));
        CHECK(dialog.place().place == BookPlace::Workshop);
        CHECK_FALSE(dialog.statusBox()->isVisibleTo(&dialog));
        CHECK(test::textsOf(dialog).contains(
            QStringLiteral("Choose the kind of the new element of the Workshop and its title.")));
    }

    SECTION("a section, a part or an element: the window starts there") {
        NewFileDialog front(
            bySections(addingOn(gui::AddTarget::Scope::Section, QString(), BookPlace::Front)),
            types);
        CHECK(chosenOf(front) == QStringLiteral("Front Section"));
        CHECK(chosenTileOf(front) == QStringLiteral("Copyright page"));

        // In a part: what goes inside it there, at its end; a new part after the others
        const QString part =
            book.pm.addElement(book.pm.partKind(), QStringLiteral("Part One"), BookPlace::Main);
        REQUIRE_FALSE(part.isEmpty());
        NewFileDialog inPart(bySections(addingOn(gui::AddTarget::Scope::Group, part)), types);
        CHECK(chosenOf(inPart) == QStringLiteral("Main Section"));
        CHECK(joined(rowsOf(*inPart.kindList())) ==
              "Prologue | Part | Chapter | Epilogue | Epigraph");
        REQUIRE(inPart.kind());
        CHECK(inPart.kind().kind->id == QStringLiteral("chapter"));
        CHECK(inPart.place() == core::ElementPlace{BookPlace::Main, part, 0});
        choose(inPart, QStringLiteral("Part"));
        CHECK(inPart.place().place == BookPlace::Main);
        CHECK(inPart.place().groupId.isEmpty());

        // After an element: its kind right after it, also back from another section
        NewFileDialog after(bySections(addingOn(gui::AddTarget::Scope::Element, chapterOne)),
                            types);
        REQUIRE(after.kind());
        CHECK(after.kind().kind->id == QStringLiteral("chapter"));
        CHECK(after.place() == core::ElementPlace{BookPlace::Main, QString(), 1});
        choiceOf(after, QStringLiteral("Front Section"))->click();
        choiceOf(after, QStringLiteral("Main Section"))->click();
        CHECK(after.place() == core::ElementPlace{BookPlace::Main, QString(), 1});
    }

    SECTION("a group of the Workshop: the Workshop with its kind, the book a click away") {
        gui::AddTarget target = addingOn(gui::AddTarget::Scope::Workshop);
        target.kind = types.findKind(QStringLiteral("kalahari.base"), QStringLiteral("work_note"));
        target.kinds = {target.kind};
        NewFileDialog dialog(bySections(target), types);
        CHECK(chosenOf(dialog) == QStringLiteral("Workshop"));
        CHECK(joined(enabledRowsOf(dialog)) == "Working note | Material");
        CHECK(chosenTileOf(dialog) == QStringLiteral("Working note"));
        choiceOf(dialog, QStringLiteral("Main Section"))->click();
        CHECK(chosenTileOf(dialog) == QStringLiteral("Chapter"));
    }
}

TEST_CASE("Add Element: the keys choose the sections at the top", "[gui][newfile][addelement]") {
    // The keys are shortcuts of the window: they need an active window, which the offscreen
    // platform has without a window on screen
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs an active window without a window on screen: run with "
             "QT_QPA_PLATFORM=offscreen");
    }
    test::OpenNovel book(QStringLiteral("Rain"));
    const core::BookTypeRegistry& types = book.pm.bookTypes();
    NewFileDialog dialog(bySections(addingOn(gui::AddTarget::Scope::Book)), types);
    dialog.show();
    dialog.activateWindow();
    REQUIRE(test::waitUntil([&dialog] { return dialog.isActiveWindow(); }));
    REQUIRE(chosenOf(dialog) == QStringLiteral("Main Section"));

    // Alt with the underlined letter
    CHECK(qt_sendShortcutOverrideEvent(dialog.titleEdit(), 0, Qt::Key_B, Qt::AltModifier));
    CHECK(test::waitUntil(
        [&dialog] { return chosenOf(dialog) == QStringLiteral("Back Section"); }));
    CHECK(chosenTileOf(dialog) == QStringLiteral("Afterword"));
    CHECK(dialog.titleEdit()->hasFocus());

    // Ctrl+Tab and Ctrl+Shift+Tab, round
    CHECK(qt_sendShortcutOverrideEvent(dialog.titleEdit(), 0, Qt::Key_Tab, Qt::ControlModifier));
    CHECK(chosenOf(dialog) == QStringLiteral("Workshop"));
    CHECK(qt_sendShortcutOverrideEvent(dialog.titleEdit(), 0, Qt::Key_Tab, Qt::ControlModifier));
    CHECK(chosenOf(dialog) == QStringLiteral("Front Section"));
    CHECK(qt_sendShortcutOverrideEvent(dialog.titleEdit(), 0, Qt::Key_Backtab,
                                       Qt::ControlModifier | Qt::ShiftModifier));
    CHECK(chosenOf(dialog) == QStringLiteral("Workshop"));
    dialog.reject();
}

TEST_CASE("Add Element: the book and the Workshop at the top, all sections of the book at once",
          "[gui][newfile][addelement]") {
    test::OpenNovel book(QStringLiteral("Rain"));
    const core::BookTypeRegistry& types = book.pm.bookTypes();

    NewFileDialog dialog(
        NewFileDialog::forAdding(addingOn(gui::AddTarget::Scope::Book), gui::AddChooser::Book),
        types);
    CHECK(choicesOf(dialog) == QStringList{"&Book", "&Workshop"});
    CHECK(chosenOf(dialog) == QStringLiteral("Book"));
    CHECK(joined(enabledRowsOf(dialog)) ==
          "# Front Section | Copyright page | Dedication | Epigraph | Preface | "
          "# Main Section | Prologue | Part | Chapter | Epilogue | # Back Section | "
          "Afterword | Acknowledgments | Glossary | About the author");
    CHECK(chosenTileOf(dialog) == QStringLiteral("Chapter"));
    choiceOf(dialog, QStringLiteral("Workshop"))->click();
    CHECK(joined(enabledRowsOf(dialog)) == "Working note | Material");

    // On the front section the window starts at its first kind
    NewFileDialog front(NewFileDialog::forAdding(addingOn(gui::AddTarget::Scope::Section,
                                                          QString(), BookPlace::Front),
                                                 gui::AddChooser::Book),
                        types);
    CHECK(chosenOf(front) == QStringLiteral("Book"));
    CHECK(chosenTileOf(front) == QStringLiteral("Copyright page"));
}

TEST_CASE("Add Element: in Polish the sections are chosen with Alt+P, G, K and W",
          "[gui][newfile][addelement]") {
    QTranslator polish;
    REQUIRE(polish.load(QStringLiteral(":/i18n/kalahari_pl.qm")));
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto& pm = core::ProjectManager::getInstance();
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(dir.path(), QStringLiteral("Deszcz"), QStringLiteral("Autor"),
                             QStringLiteral("pl"), false, QStringLiteral("kalahari.novel")));
    QStringList choices;
    QCoreApplication::installTranslator(&polish);
    {
        NewFileDialog dialog(bySections(addingOn(gui::AddTarget::Scope::Book)), pm.bookTypes());
        choices = choicesOf(dialog);
    }
    QCoreApplication::removeTranslator(&polish);
    pm.closeProject(false);
    // Tytuł, Stan, Plik and Wybierz keep their letters
    CHECK(choices == QStringList{"Sekcja &początkowa", "Sekcja &główna", "Sekcja &końcowa",
                                 "&Warsztat"});
}
