/// @file test_new_file.cpp
/// @brief New File: an element of the open book in the New File window, or a file outside a
/// book; Open File in the File menu and on the toolbars

#include <catch2/catch_test_macros.hpp>
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

#include "document_window.h"

#include <QAbstractButton>
#include <QAction>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <algorithm>
#include <functional>
#include <string>

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

/// @brief The rows of the list of kinds of the New File window: "# Front matter" for the
/// name of a part of the book, the name of the kind for a tile, and "Title page (greyed): why"
/// for a greyed tile
QStringList rowsOf(const QListWidget& list) {
    QStringList rows;
    for (int row = 0; row < list.count(); ++row) {
        const QListWidgetItem* item = list.item(row);
        // A tile knows its kind; the name of a part of the book does not
        if (!item->data(Qt::UserRole).isValid()) {
            rows << QStringLiteral("# ") + item->text();
        } else if (item->flags().testFlag(Qt::ItemIsEnabled)) {
            rows << item->text();
        } else {
            rows << item->text() + QStringLiteral(" (greyed): ") + item->toolTip();
        }
    }
    return rows;
}

/// @brief The first tile of the kind named @p name in the New File window
QListWidgetItem* tileOf(const NewFileDialog& dialog, const QString& name) {
    for (int row = 0; row < dialog.kindList()->count(); ++row) {
        QListWidgetItem* item = dialog.kindList()->item(row);
        if (item->data(Qt::UserRole).isValid() && item->text() == name) {
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
    const auto* buttons = dialog.findChild<QDialogButtonBox*>();
    return buttons ? buttons->button(QDialogButtonBox::Cancel) : nullptr;
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
/// the window proposes goes to @p proposed
/// @return The number of windows that saved a file
int savingAs(const std::function<void()>& command, const QString& path, QString* proposed) {
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

TEST_CASE("New File: Open File under Open Book in the File menu, without an Open submenu",
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

    // New File opens a window, so it has an ellipsis; the other items keep their order
    const QStringList items = itemsOf(*file);
    CHECK(joined(items.mid(0, 10)) ==
          "New File... | New Book... | Open Book... | Open File... | Recent Books | Close Book | "
          "- | Save | Save As... | Save All");
    CHECK_FALSE(items.contains(QStringLiteral("Open")));

    // The shortcuts of Windows
    CHECK(registry.getAction(std::string("file.new"))->shortcut() ==
          QKeySequence(QKeySequence::New));
    CHECK(registry.getAction(std::string("file.open.file"))->shortcut() ==
          QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
}

TEST_CASE("New File: Open File after Open Book on the File and Quick Actions toolbars",
          "[gui][newfile]") {
    gui::registerAllCommands(gui::CommandCallbacks{});
    gui::registerAllIcons();
    gui::CommandRegistry& registry = gui::CommandRegistry::getInstance();
    QMainWindow window;
    gui::ToolbarManager manager(&window);
    manager.createToolbars(registry);

    const QStringList file = manager.getToolbarCommands(QStringLiteral("file"));
    CHECK(joined(file) ==
          "file.new | file.new.project | file.open | file.open.file | file.save | file.saveAs | "
          "file.saveAll | file.close");

    // Quick Actions starts with every command of the File toolbar, in its order
    const QStringList quick = manager.getToolbarCommands(QStringLiteral("quickActions"));
    CHECK(joined(quick.mid(0, file.size())) == joined(file));

    // The buttons are the commands of the menu
    QToolBar* toolbar = manager.getToolbar("quickActions");
    REQUIRE(toolbar != nullptr);
    REQUIRE(toolbar->actions().size() > 3);
    CHECK(toolbar->actions().at(3) == registry.getAction(std::string("file.open.file")));
}

TEST_CASE("New File: a toolbar customized before gets Open File once, where the default one "
          "has it",
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
              "file.new | file.open | file.open.file | file.save | file.saveAs | file.close | "
              "edit.find");
        CHECK(joined(manager.getToolbarCommands(QStringLiteral("file"))) ==
              "file.new | file.open | file.open.file | file.save | file.saveAll | file.close");

        // A command the writer removes after that stays removed
        QStringList without = manager.getToolbarCommands(quickActions);
        without.removeAll(QStringLiteral("file.open.file"));
        manager.setToolbarCommands(quickActions, without);
        gui::ToolbarManager next(&window);
        CHECK(joined(next.getToolbarCommands(quickActions)) == joined(without));
    }

    SECTION("without the commands Open File follows in the default toolbar") {
        settings.set<std::string>(
            "toolbars.configurations",
            R"({"builtIn":{"quickActions":["edit.find","file.new.project","file.close"]}})");
        gui::ToolbarManager manager(&window);
        // After the nearest command before it in the default toolbar that the toolbar has
        CHECK(joined(manager.getToolbarCommands(quickActions)) ==
              "edit.find | file.new.project | file.open.file | file.saveAs | file.close");
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

    // A change gets the "*" of a changed file
    test::write(*story, QStringLiteral("Once upon a time"));
    CHECK(window.tabs->tabText(0) == QStringLiteral("*Untitled 1"));
    CHECK(window.documents.unsavedDocumentNames() == QStringList{QStringLiteral("Untitled 1")});

    window.documents.onNewDocument();
    REQUIRE(window.tabs->count() == 2);
    CHECK(window.tabs->tabText(1) == QStringLiteral("Untitled 2"));
    CHECK(window.tabs->currentIndex() == 1);

    // The first save asks where, in the folder of Open File, with the name of the tab
    OwnFileDialogs own;
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    gui::ProgramFolders::remember(gui::ProgramFolders::Operation::OpenFile, folder.path());
    window.tabs->setCurrentIndex(0);
    const QString path = QDir(folder.path()).filePath(QStringLiteral("Story.kchapter"));
    QString proposed;
    CHECK(savingAs([&window]() { window.documents.onSaveDocument(); }, path, &proposed) == 1);
    CHECK(QFileInfo(proposed) ==
          QFileInfo(QDir(folder.path()).filePath(QStringLiteral("Untitled 1.kchapter"))));

    // Saved, under its new name, and in the Navigator with the other files outside a book
    CHECK(QFileInfo::exists(path));
    CHECK(window.tabs->tabText(0) == QStringLiteral("Story.kchapter"));
    CHECK_FALSE(story->property("dirty").toBool());
    CHECK(listsFile(window, path));

    // The next save writes the file without asking
    test::write(*story, QStringLiteral("Once upon a time there was a storm"));
    CHECK(savingAs([&window]() { window.documents.onSaveDocument(); }, path, nullptr) == 0);
    CHECK(window.tabs->tabText(0) == QStringLiteral("Story.kchapter"));
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
