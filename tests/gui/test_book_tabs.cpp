/// @file test_book_tabs.cpp
/// @brief The tabs of the open book: one file is edited in one tab, Save As and Close Book
/// with a book open, the Properties panel without an editor in front, the words of the book
/// while the changes of its chapters are not saved, and the changes: only a text that differs
/// from the saved one has them

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/project_manager.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/panels/editor_panel.h>
#include <kalahari/gui/panels/properties_panel.h>

#include "document_window.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <functional>

using namespace kalahari;
using gui::dialogs::MessageDialog;
using test::answering;
using test::OpenNovel;
using test::Window;

namespace {

/// Write @p text to a new file @p path
void writeText(const QString& path, const QByteArray& text) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(text) == text.size());
}

/// The text of the file @p path
QByteArray readText(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

/// The file of the element @p elementId of the open book
QString fileOf(const QString& elementId) {
    auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(elementId);
    REQUIRE(element != nullptr);
    return pm.filePathOf(*element);
}

/// Let the program work until @p done, for at most five seconds
bool waitFor(const std::function<bool()>& done) {
    QElapsedTimer clock;
    clock.start();
    while (!done() && clock.elapsed() < 5000) {
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return done();
}

/// The name of the file @p path in the Navigator of @p window, with its marks; empty when the
/// Navigator does not list the file
QString nameInNavigator(const Window& window, const QString& path) {
    auto* tree = window.navigatorPanel->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == path) {
            return (*it)->text(0);
        }
    }
    return {};
}

/// The tabs of @p window follow the book as in the main window: they take the files of the
/// book when it opens, and its chapters close with it
void followBook(Window& window) {
    auto& pm = core::ProjectManager::getInstance();
    QObject::connect(&pm, &core::ProjectManager::projectOpened, &window.documents,
                     &gui::DocumentCoordinator::onProjectOpened);
    QObject::connect(&pm, &core::ProjectManager::projectClosed, &window.documents,
                     &gui::DocumentCoordinator::onProjectClosed);
}

} // namespace

// ----------------------------------------------------------------------------
// One file in one tab
// ----------------------------------------------------------------------------

TEST_CASE("Book tabs: the file of a chapter of the open book opens as the chapter, in its tab",
          "[gui][booktabs]") {
    OpenNovel novel(QStringLiteral("Rain"));
    Window window;
    const QString chapterPath = fileOf(novel.chapterId);

    window.documents.openStandaloneFile(chapterPath);
    REQUIRE(window.tabs->count() == 1);
    gui::EditorPanel* chapter = window.editor(0);
    CHECK(chapter->property("elementId").toString() == novel.chapterId);
    CHECK(window.documents.standaloneFilePaths().isEmpty());
    CHECK(window.infoBar->isHidden());  // it does not say that the file is outside the book
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("This file belongs to the open book: it is the chapter \"The Storm\"."));

    // Opened again, by another path to it or from the Navigator: the same tab
    const QFileInfo file(chapterPath);
    window.documents.openStandaloneFile(file.absolutePath() + QStringLiteral("/../") +
                                        file.dir().dirName() + QLatin1Char('/') +
                                        file.fileName());
    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
    CHECK(window.tabs->count() == 1);
    CHECK(window.tabs->currentWidget() == chapter);
}

TEST_CASE("Book tabs: opening the book, its files open outside it become the tabs of their "
          "elements, with their changes",
          "[gui][booktabs]") {
    const test::RecentBooks recent;
    OpenNovel novel(QStringLiteral("Rain"));
    const QString manifest = novel.pm.getManifestPath();
    const QString chapterPath = fileOf(novel.chapterId);
    novel.pm.closeProject(false);

    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    writeText(notes, "Notes");

    // Without the book, the file of its chapter is a file like the others
    Window window;
    followBook(window);
    gui::EditorPanel* notesTab = window.writeFile(notes, QStringLiteral("Notes of the night"));
    window.writeFile(chapterPath, QStringLiteral("The storm came at night."));
    REQUIRE(window.tabs->count() == 2);

    QStringList said;
    CHECK(answering([&]() { window.documents.onOpenRecentFile(manifest); },
                    MessageDialog::Answer::Accept, said) == 0);
    INFO(said.join(QLatin1Char('\n')).toStdString());
    REQUIRE(novel.pm.isProjectOpen());

    // The tab of the chapter takes the place of the file's, with its changes; the other file
    // stays as it was
    REQUIRE(window.tabs->count() == 2);
    CHECK(window.tabs->widget(0) == notesTab);
    CHECK(notesTab->property("dirty").toBool());
    gui::EditorPanel* chapter = window.editor(1);
    CHECK(chapter->property("elementId").toString() == novel.chapterId);
    CHECK(window.tabs->currentWidget() == chapter);
    CHECK(chapter->getText().contains(QStringLiteral("The storm came at night.")));
    CHECK(window.navigator.isChapterDirty(novel.chapterId));
    CHECK(window.documents.standaloneFilePaths() == QStringList{notes});
    CHECK(window.infoBar->isHidden());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("Files of this book that were open on their own are open as its "
                         "elements now: The Storm"));

    // Saved with the book
    window.documents.onSaveBook();
    CHECK(readText(chapterPath).contains("The storm came at night."));
    CHECK_FALSE(window.navigator.isChapterDirty(novel.chapterId));
}

TEST_CASE("Book tabs: Save As does not give a file outside the book the file of a chapter of "
          "the open book",
          "[gui][booktabs]") {
    OpenNovel novel(QStringLiteral("Rain"));
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    writeText(notes, "Notes");
    const QString chapterPath = fileOf(novel.chapterId);
    const QByteArray chapter = readText(chapterPath);

    Window window;
    gui::EditorPanel* notesTab = window.writeFile(notes, QStringLiteral("Notes of the night"));

    // The writer chooses the file of the chapter, and agrees to replace it
    const test::OwnFileDialogs own;
    QStringList said;
    CHECK(answering([&]() { window.documents.onSaveAsDocument(); },
                    MessageDialog::Answer::Accept, said, chapterPath) >= 2);
    INFO(said.join(QLatin1Char('\n')).toStdString());
    REQUIRE(said.size() >= 2);
    CHECK(said.first() == QStringLiteral("Save File As"));
    CHECK(said.last().contains(
        QStringLiteral("'%1' is the file of the chapter \"The Storm\" in the open book. "
                       "Choose another name.")
            .arg(QFileInfo(chapterPath).fileName())));

    // Nothing is written: the file stays the file of the chapter, the tab keeps its file
    CHECK(readText(chapterPath) == chapter);
    CHECK(notesTab->property("standaloneFilePath").toString() == notes);
    CHECK(notesTab->property("dirty").toBool());
    CHECK(readText(notes) == "Notes");
}

// ----------------------------------------------------------------------------
// Save As and Close Book
// ----------------------------------------------------------------------------

TEST_CASE("Book tabs: Save As on a chapter of the book says that Save saves it with the book",
          "[gui][booktabs]") {
    OpenNovel novel(QStringLiteral("Rain"));
    Window window;
    window.writeChapter(novel.chapterId, QStringLiteral("The storm came at night."));
    const QByteArray saved = novel.chapterFile();

    const test::OwnFileDialogs own;
    QStringList said;
    CHECK(answering([&]() { window.documents.onSaveAsDocument(); },
                    MessageDialog::Answer::Cancel, said) == 0);
    INFO(said.join(QLatin1Char('\n')).toStdString());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("Chapter \"The Storm\" is saved with the book: use Save."));
    CHECK(novel.chapterFile() == saved);
    CHECK(window.navigator.isChapterDirty(novel.chapterId));
}

TEST_CASE("Book tabs: Close Book asks only about the changes of the book, and the files "
          "outside it stay open with theirs",
          "[gui][booktabs]") {
    OpenNovel novel(QStringLiteral("Rain"));
    const QString chapterPath = fileOf(novel.chapterId);
    const QByteArray chapter = readText(chapterPath);
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    writeText(notes, "Notes");

    Window window;
    followBook(window);
    gui::EditorPanel* notesTab = window.writeFile(notes, QStringLiteral("Notes of the night"));
    const QString question =
        QStringLiteral("Do you want to save changes to 'Rain' before closing the book?");
    QStringList said;

    SECTION("Only a file outside the book is changed: the book closes without a question") {
        CHECK(answering([&]() { window.documents.onCloseDocument(); },
                        MessageDialog::Answer::Cancel, said) == 0);
        CHECK_FALSE(novel.pm.isProjectOpen());
        CHECK(window.tabs->count() == 1);

        // Without a book, the command says that none is open
        CHECK(answering([&]() { window.documents.onCloseDocument(); },
                        MessageDialog::Answer::Cancel, said) == 0);
        CHECK(window.statusBar.currentMessage() == QStringLiteral("No book is open"));
    }

    SECTION("Save: the book is saved and closed") {
        window.writeChapter(novel.chapterId, QStringLiteral("The storm came at night."));
        CHECK(answering([&]() { window.documents.onCloseDocument(); },
                        MessageDialog::Answer::Accept, said) == 1);
        INFO(said.join(QLatin1Char('\n')).toStdString());
        REQUIRE(said.size() == 1);
        CHECK(said.first().contains(question));
        CHECK_FALSE(novel.pm.isProjectOpen());
        CHECK(window.tabs->count() == 1);
        CHECK(readText(chapterPath).contains("The storm came at night."));
    }

    SECTION("Don't Save: the changes of the book go with it") {
        window.writeChapter(novel.chapterId, QStringLiteral("The storm came at night."));
        CHECK(answering([&]() { window.documents.onCloseDocument(); },
                        MessageDialog::Answer::Alternative, said) == 1);
        INFO(said.join(QLatin1Char('\n')).toStdString());
        REQUIRE(said.size() == 1);
        CHECK(said.first().contains(question));
        CHECK_FALSE(novel.pm.isProjectOpen());
        CHECK(window.tabs->count() == 1);
        CHECK(readText(chapterPath) == chapter);
    }

    SECTION("Cancel: the book stays open with its changes") {
        window.writeChapter(novel.chapterId, QStringLiteral("The storm came at night."));
        CHECK(answering([&]() { window.documents.onCloseDocument(); },
                        MessageDialog::Answer::Cancel, said) == 1);
        CHECK(novel.pm.isProjectOpen());
        CHECK(window.tabs->count() == 2);
        CHECK(window.navigator.isChapterDirty(novel.chapterId));
    }

    // The file outside the book stays open, with its changes, and is not saved
    CHECK(window.tabs->indexOf(notesTab) >= 0);
    CHECK(notesTab->property("dirty").toBool());
    CHECK(readText(notes) == "Notes");
}

TEST_CASE("Book tabs: a changed file outside the book has a star in the Navigator, also when "
          "the book is saved, closed and opened again",
          "[gui][booktabs]") {
    OpenNovel novel(QStringLiteral("Rain"));
    const QString manifest = novel.pm.getManifestPath();
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    writeText(notes, "Notes");

    Window window;
    followBook(window);
    gui::EditorPanel* notesTab = window.writeFile(notes, QStringLiteral("Notes of the night"));
    const auto shown = [&window, &notes]() { return nameInNavigator(window, notes); };
    CHECK(shown() == "*notes.txt");

    // The book is saved without the file
    window.writeChapter(novel.chapterId, QStringLiteral("The storm came at night."));
    window.documents.onSaveBook();
    CHECK_FALSE(window.navigator.isChapterDirty(novel.chapterId));
    CHECK(shown() == "*notes.txt");

    // The book closes, and the Navigator lists the files outside it alone
    QStringList said;
    CHECK(answering([&]() { window.documents.onCloseDocument(); },
                    MessageDialog::Answer::Cancel, said) == 0);
    REQUIRE_FALSE(novel.pm.isProjectOpen());
    CHECK(shown() == "*notes.txt");

    // The book opens again, with the files outside it after its elements
    REQUIRE(novel.pm.openProject(manifest));
    CHECK(shown() == "*notes.txt");

    // Saved, the file has no star
    REQUIRE(window.documents.saveEditor(notesTab));
    CHECK(readText(notes) == "Notes of the night");
    CHECK(shown() == "notes.txt");

    // Changed again and closed without saving: the file in the list is the file on the disk
    test::write(*notesTab, QStringLiteral("Notes of the morning"));
    CHECK(shown() == "*notes.txt");
    window.documents.discardEditorChanges(notesTab);
    CHECK(shown() == "notes.txt");
}

// ----------------------------------------------------------------------------
// The Properties panel
// ----------------------------------------------------------------------------

TEST_CASE("Book tabs: without an editor in front, the Properties panel shows the book, not the "
          "editor",
          "[gui][booktabs]") {
    using Page = gui::PropertiesPanel::Page;
    OpenNovel novel(QStringLiteral("Rain"));
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    writeText(notes, "Notes");

    Window window;
    followBook(window);
    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
    REQUIRE(window.tabs->count() == 1);

    // The editor in front, then another tab (the Dashboard) or none: the page of the book
    window.properties->setActiveEditor(window.editor(0));
    CHECK(window.properties->currentPage() == Page::Editor);
    window.properties->setActiveEditor(nullptr);
    CHECK(window.properties->currentPage() == Page::Project);

    // The page of an element chosen in the Navigator stays
    window.properties->showChapterProperties(novel.chapterId);
    window.properties->setActiveEditor(nullptr);
    CHECK(window.properties->currentPage() == Page::Chapter);

    // Without a book: the page that says so
    novel.pm.closeProject(false);
    window.documents.openStandaloneFile(notes);
    REQUIRE(window.tabs->count() == 1);
    window.properties->setActiveEditor(window.editor(0));
    CHECK(window.properties->currentPage() == Page::Editor);
    window.properties->setActiveEditor(nullptr);
    CHECK(window.properties->currentPage() == Page::NoProject);
}

// ----------------------------------------------------------------------------
// The words of the book
// ----------------------------------------------------------------------------

TEST_CASE("Book tabs: the words of a chapter count in the book before it is saved, and its "
          "discarded changes no more",
          "[gui][booktabs]") {
    OpenNovel novel(QStringLiteral("Rain"));
    Window window;
    auto* chapterWords =
        window.properties->findChild<QLabel*>(QStringLiteral("chapterWordCountLabel"));
    auto* bookWords = window.properties->findChild<QLabel*>(QStringLiteral("projectWordsLabel"));
    REQUIRE(chapterWords != nullptr);
    REQUIRE(bookWords != nullptr);

    // A new chapter: its words show a moment after the writing, before a save
    window.writeChapter(novel.chapterId, QStringLiteral("The storm came at night."));
    window.properties->showChapterProperties(novel.chapterId);
    CHECK(waitFor([&]() { return chapterWords->text() == QStringLiteral("5"); }));
    CHECK(novel.pm.wordCount(novel.chapterId) == 5);
    window.properties->showProjectProperties();
    CHECK(bookWords->text() == QStringLiteral("5"));

    // Saved: the words of its file are the same
    window.documents.onSaveBook();
    CHECK_FALSE(window.navigator.isChapterDirty(novel.chapterId));
    CHECK(novel.pm.wordCount(novel.chapterId) == 5);

    // More words, then the tab closed without saving them: the words of the file again
    gui::EditorPanel* editor = window.editor(0);
    test::write(*editor, QStringLiteral("The storm came at night and it rained."));
    CHECK(waitFor([&]() { return bookWords->text() == QStringLiteral("8"); }));
    window.documents.discardEditorChanges(editor);
    window.tabs->removeTab(0);
    editor->deleteLater();
    CHECK(novel.pm.wordCount(novel.chapterId) == 5);
    CHECK(bookWords->text() == QStringLiteral("5"));
}

// ----------------------------------------------------------------------------
// Changes
// ----------------------------------------------------------------------------

namespace {

/// A chapter of the open book and a file outside it, each open in its tab
struct ChangedTabs {
    OpenNovel novel{QStringLiteral("Rain")};
    QTemporaryDir files;
    QString notes;
    Window window;
    gui::EditorPanel* notesTab = nullptr;
    gui::EditorPanel* chapterTab = nullptr;

    ChangedTabs() {
        REQUIRE(files.isValid());
        notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
        writeText(notes, "Notes");
        followBook(window);
        window.documents.openStandaloneFile(notes);
        notesTab = window.editor(window.tabs->currentIndex());
        window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
        chapterTab = window.editor(window.tabs->currentIndex());
        REQUIRE(notesTab != chapterTab);
    }

    /// Type @p letter in both tabs, where their cursors are
    void type(const QString& letter) const {
        notesTab->getBookEditor()->insertText(letter);
        chapterTab->getBookEditor()->insertText(letter);
    }

    /// Delete the letter before the cursor in both tabs
    void deleteLetter() const {
        notesTab->getBookEditor()->deleteBackward();
        chapterTab->getBookEditor()->deleteBackward();
    }

    /// The text of the tab of @p editor
    [[nodiscard]] QString tabText(const gui::EditorPanel* editor) const {
        return window.tabs->tabText(window.tabs->indexOf(editor));
    }

    /// Whether the tabs and the Navigator show changes ("*")
    [[nodiscard]] bool showChanges() const {
        return tabText(notesTab) == QStringLiteral("*notes.txt") &&
               tabText(chapterTab) == QStringLiteral("*The Storm") &&
               notesTab->property("dirty").toBool() &&
               window.navigator.isChapterDirty(novel.chapterId);
    }

    /// Whether the tabs and the Navigator show no changes
    [[nodiscard]] bool showNoChanges() const {
        return tabText(notesTab) == QStringLiteral("notes.txt") &&
               tabText(chapterTab) == QStringLiteral("The Storm") &&
               !notesTab->property("dirty").toBool() &&
               !window.navigator.isChapterDirty(novel.chapterId);
    }
};

} // namespace

TEST_CASE("Book tabs: a letter typed and deleted again leaves no changes to save or to ask about",
          "[gui][booktabs]") {
    ChangedTabs tabs;
    CHECK(tabs.showNoChanges());

    // A letter typed: the tabs and the Navigator show the changes, which the book and the file
    // have to save
    tabs.type(QStringLiteral("x"));
    CHECK(tabs.showChanges());
    CHECK(tabs.window.documents.unsavedDocumentNames() ==
          QStringList({QStringLiteral("Rain"), QStringLiteral("notes.txt")}));

    // Deleted again: the texts are the saved ones, so there is nothing to save, and Close Book
    // does not ask
    tabs.deleteLetter();
    CHECK(tabs.window.documents.unsavedDocumentNames().isEmpty());
    CHECK_FALSE(tabs.window.documents.isEditorDirty(tabs.chapterTab));
    CHECK_FALSE(tabs.window.documents.isEditorDirty(tabs.notesTab));
    CHECK(waitFor([&tabs]() { return tabs.showNoChanges(); }));
    QStringList said;
    CHECK(answering([&tabs]() { tabs.window.documents.onCloseDocument(); },
                    MessageDialog::Answer::Cancel, said) == 0);
    CHECK_FALSE(tabs.novel.pm.isProjectOpen());
    CHECK(readText(tabs.notes) == "Notes");
}

TEST_CASE("Book tabs: after a save, a change is a change of the text saved last",
          "[gui][booktabs]") {
    ChangedTabs tabs;
    tabs.type(QStringLiteral("x"));
    REQUIRE(tabs.showChanges());

    // Saved: the tabs show no changes
    tabs.window.documents.onSaveBook();
    tabs.window.tabs->setCurrentWidget(tabs.notesTab);
    tabs.window.documents.onSaveDocument();
    CHECK(tabs.showNoChanges());
    CHECK(tabs.window.documents.unsavedDocumentNames().isEmpty());
    CHECK(readText(tabs.notes) == "xNotes");

    // Another letter is a change, and deleted again it is not: the saved texts have the first
    // one
    tabs.type(QStringLiteral("y"));
    CHECK(tabs.showChanges());
    tabs.deleteLetter();
    CHECK(tabs.window.documents.unsavedDocumentNames().isEmpty());
    CHECK(waitFor([&tabs]() { return tabs.showNoChanges(); }));
}
