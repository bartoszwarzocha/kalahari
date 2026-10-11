/// @file test_book_tabs.cpp
/// @brief The tabs of the open book: one file is edited in one tab, Save As and Close Book
/// with a book open, the Properties panel without an editor in front, and the words of the
/// book while the changes of its chapters are not saved

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/project_manager.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/panels/editor_panel.h>
#include <kalahari/gui/panels/properties_panel.h>

#include "document_window.h"

#include <QAbstractButton>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>

#include <functional>

using namespace kalahari;
using gui::dialogs::MessageDialog;
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

/// @brief Run @p command, answering each window it shows once the system shows it: the
/// program's windows with the button of @p answer, the window that saves a file with the file
/// @p file, and its question whether to replace the file with Yes. The title and the texts of
/// each window go to @p said, one window a line.
/// @return The number of windows the command showed
int answering(const std::function<void()>& command, MessageDialog::Answer answer,
              QStringList& said, const QString& file = QString()) {
    int shown = 0;
    int waited = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        QDialog* dialog = test::shownDialog();
        if (!dialog || dialog->property("handled").toBool()) {
            return;  // A window answered closes in its loop
        }
        if (!test::onScreen(*dialog) && ++waited < test::SHOWN_TICKS) {
            return;
        }
        waited = 0;
        dialog->setProperty("handled", true);
        ++shown;
        if (auto* files = qobject_cast<QFileDialog*>(dialog)) {
            said << files->windowTitle();
            if (auto* name = files->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"))) {
                name->setText(file);
            } else {
                files->selectFile(file);
            }
            QMetaObject::invokeMethod(files, "accept", Qt::QueuedConnection);
            return;
        }
        QAbstractButton* button = nullptr;
        if (auto* question = qobject_cast<QMessageBox*>(dialog)) {
            said << question->text();
            button = question->button(QMessageBox::Yes);
        } else if (auto* message = qobject_cast<MessageDialog*>(dialog)) {
            said << test::textsOf(*message);
            button = answer == MessageDialog::Answer::Accept        ? message->acceptButton()
                     : answer == MessageDialog::Answer::Alternative ? message->alternativeButton()
                                                                    : message->cancelButton();
        } else {
            said << test::textsOf(*dialog);
        }
        if (button) {
            QMetaObject::invokeMethod(button, "click", Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(dialog, "reject", Qt::QueuedConnection);
        }
    });
    timer.start();
    command();
    return shown;
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
    editor->setText(QStringLiteral("The storm came at night and it rained."));
    CHECK(waitFor([&]() { return bookWords->text() == QStringLiteral("8"); }));
    window.documents.discardEditorChanges(editor);
    window.tabs->removeTab(0);
    editor->deleteLater();
    CHECK(novel.pm.wordCount(novel.chapterId) == 5);
    CHECK(bookWords->text() == QStringLiteral("5"));
}
