/// @file test_save_all.cpp
/// @brief "Save All" saves the book and every open file outside it; closing the book asks only
/// about the changes of the book; a book of a newer version of Kalahari has its own window; a
/// plain text file asks before it loses the formatting or the annotations of its text, and its
/// tab loses them too

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/standalone_file.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/panels/editor_panel.h>

#include "../core/other_program.h"
#include "document_window.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTimer>

using namespace kalahari;
using gui::dialogs::MessageDialog;
using test::accepting;
using test::answering;
using test::OpenNovel;
using test::RecentBooks;
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

} // namespace

TEST_CASE("Save All: the book and every open file outside it are saved", "[gui][save]") {
    OpenNovel novel(QStringLiteral("Save Test"));
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    const QString ideas = QDir(files.path()).filePath(QStringLiteral("ideas.txt"));
    writeText(notes, "Notes");
    writeText(ideas, "Ideas");

    Window window;
    window.writeChapter(novel.chapterId, QStringLiteral("The storm came at night."));
    gui::EditorPanel* notesTab = window.writeFile(notes, QStringLiteral("Notes of the night"));
    window.documents.openStandaloneFile(ideas);  // open, and not changed

    CHECK(accepting([&] { window.documents.onSaveAll(); }) == 0);
    CHECK(window.statusBar.currentMessage() == QStringLiteral("All changes saved"));
    CHECK(novel.chapterFile().contains("The storm came at night."));
    CHECK_FALSE(window.navigator.isChapterDirty(novel.chapterId));
    CHECK(readText(notes) == "Notes of the night");
    CHECK_FALSE(notesTab->property("dirty").toBool());
    CHECK(readText(ideas) == "Ideas");
    CHECK(window.documents.unsavedDocumentNames().isEmpty());

    // Nothing left to save: the status bar says so
    window.statusBar.clearMessage();
    window.documents.onSaveAll();
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("Nothing to save: all changes are saved"));

    // Without a book, the files outside it are saved too
    novel.pm.closeProject(false);
    test::write(*notesTab, QStringLiteral("Notes of the morning"));
    window.documents.onSaveAll();
    CHECK(readText(notes) == "Notes of the morning");
    CHECK(window.statusBar.currentMessage() == QStringLiteral("All changes saved"));
}

TEST_CASE("Save All: a file that cannot be saved does not stop the others", "[gui][save]") {
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QDir root(files.path());
    REQUIRE(root.mkdir(QStringLiteral("gone")));
    const QString lost = root.filePath(QStringLiteral("gone/lost.txt"));
    const QString kept = root.filePath(QStringLiteral("kept.txt"));
    writeText(lost, "Lost");
    writeText(kept, "Kept");

    Window window;
    gui::EditorPanel* lostTab = window.writeFile(lost, QStringLiteral("Lost text"));
    gui::EditorPanel* keptTab = window.writeFile(kept, QStringLiteral("Kept text"));

    // The folder of the first file is gone: its window says so, and the second file is saved
    REQUIRE(QDir(root.filePath(QStringLiteral("gone"))).removeRecursively());
    QStringList said;
    CHECK(accepting([&] { window.documents.onSaveAll(); }, &said) == 1);
    INFO(said.join(QLatin1Char('\n')).toStdString());
    CHECK(lostTab->property("dirty").toBool());
    CHECK(readText(kept) == "Kept text");
    CHECK_FALSE(keptTab->property("dirty").toBool());
    CHECK(window.statusBar.currentMessage() != QStringLiteral("All changes saved"));
}

TEST_CASE("Opening another book asks only about the changes of the open book",
          "[gui][save]") {
    const RecentBooks recent;
    auto& pm = core::ProjectManager::getInstance();
    QTemporaryDir otherFolder;
    REQUIRE(otherFolder.isValid());
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(otherFolder.path(), QStringLiteral("Other Book"),
                             QStringLiteral("Author"), QStringLiteral("en"), false,
                             QStringLiteral("kalahari.novel")));
    const QString otherBook = pm.getManifestPath();
    pm.closeProject(false);

    OpenNovel novel(QStringLiteral("Open Book"));
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    writeText(notes, "Notes");

    // The book has no changes; a file outside it has
    Window window;
    gui::EditorPanel* notesTab = window.writeFile(notes, QStringLiteral("Notes of the night"));

    // The question is whether to close the book, not whether to save it, and the file stays
    // open with its changes
    QStringList said;
    CHECK(accepting([&] { window.documents.onOpenRecentFile(otherBook); }, &said) == 1);
    INFO(said.join(QLatin1Char('\n')).toStdString());
    REQUIRE(said.size() == 1);
    CHECK(said.first().startsWith(QStringLiteral("Close Book? ")));
    CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(otherBook));
    CHECK(window.tabs->indexOf(notesTab) >= 0);
    CHECK(notesTab->property("dirty").toBool());
    CHECK(readText(notes) == "Notes");
}

TEST_CASE("Opening a book of a newer version of Kalahari says so, and the open book stays open",
          "[gui][save]") {
    const RecentBooks recent;
    auto& pm = core::ProjectManager::getInstance();
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString future = QDir(files.path()).filePath(QStringLiteral("Future.klh"));
    writeText(future, QByteArray("{ \"format\": ") +
                          QByteArray::number(core::BookProject::FORMAT + 1) +
                          QByteArray(", \"id\": \"p\" }"));
    const QString broken = QDir(files.path()).filePath(QStringLiteral("Broken.klh"));
    writeText(broken, "{ \"format\": 2, ");

    const QString newerTitle = QStringLiteral("Book of a Newer Version ");
    const QString newerText = QStringLiteral("This book was saved in a newer version of Kalahari. "
                                             "Update the program to open it.");

    SECTION("Without an open book") {
        if (pm.isProjectOpen()) {
            pm.closeProject(false);
        }
        Window window;
        QStringList said;
        CHECK(accepting([&] { window.documents.onOpenRecentFile(future); }, &said) == 1);
        INFO(said.join(QLatin1Char('\n')).toStdString());
        REQUIRE(said.size() == 1);
        CHECK(said.first().startsWith(newerTitle));
        CHECK(said.first().contains(newerText));
        CHECK_FALSE(pm.isProjectOpen());

        // A damaged book is an error of opening, not a book of a newer version
        said.clear();
        CHECK(accepting([&] { window.documents.onOpenRecentFile(broken); }, &said) == 1);
        REQUIRE(said.size() == 1);
        CHECK(said.first().startsWith(QStringLiteral("Book Cannot Be Read ")));
        CHECK(said.first().contains(QStringLiteral(
            "The book file \"Broken.klh\" is damaged or incomplete, so the program cannot read "
            "it.")));
        CHECK_FALSE(said.first().contains(newerText));
    }

    SECTION("With an open book") {
        OpenNovel novel(QStringLiteral("Open Book"));
        const QString open = pm.getManifestPath();
        Window window;

        // The window comes before the question about closing the open book, which stays open
        QStringList said;
        CHECK(accepting([&] { window.documents.onOpenRecentFile(future); }, &said) == 1);
        INFO(said.join(QLatin1Char('\n')).toStdString());
        REQUIRE(said.size() == 1);
        CHECK(said.first().startsWith(newerTitle));
        CHECK(said.first().contains(newerText));
        CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(open));
    }
}

// ----------------------------------------------------------------------------
// Plain text files
// ----------------------------------------------------------------------------

TEST_CASE("Save: a plain text file with formatting asks first, as it keeps the text only",
          "[gui][save]") {
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString notes = QDir(files.path()).filePath(QStringLiteral("notes.txt"));
    writeText(notes, "Notes");

    Window window;
    gui::EditorPanel* tab = window.writeFile(notes, QStringLiteral("Notes of the night"));
    QStringList said;

    SECTION("Its text alone is saved without a question") {
        CHECK(answering([&] { window.documents.onSaveDocument(); },
                        MessageDialog::Answer::Cancel, said) == 0);
        CHECK(readText(notes) == "Notes of the night");
        CHECK_FALSE(tab->property("dirty").toBool());
    }

    SECTION("Bold text: the question says what the file loses") {
        tab->getBookEditor()->selectAll();
        tab->getBookEditor()->toggleBold();

        // Cancel: nothing is written, and the changes wait in the tab
        CHECK(answering([&] { window.documents.onSaveDocument(); },
                        MessageDialog::Answer::Cancel, said) == 1);
        INFO(said.join(QLatin1Char('\n')).toStdString());
        REQUIRE(said.size() == 1);
        CHECK(said.first().contains(QStringLiteral("Save as Plain Text?")));
        CHECK(said.first().contains(QStringLiteral(
            "A plain text file (.txt) keeps only the text, so the formatting (such as bold) "
            "will not be saved in \"notes.txt\".")));
        CHECK(said.first().contains(QStringLiteral(
            "To keep everything, choose Cancel, then File > Save As... and the type \"Kalahari "
            "Documents (*.kchapter)\".")));
        CHECK(readText(notes) == "Notes");
        CHECK(tab->property("dirty").toBool());

        // Save as Text: the text is written without its formatting, which the tab loses too
        CHECK(answering([&] { window.documents.onSaveDocument(); },
                        MessageDialog::Answer::Accept, said) == 1);
        CHECK(readText(notes) == "Notes of the night");
        CHECK_FALSE(tab->property("dirty").toBool());
        CHECK(tab->getText() == QStringLiteral("Notes of the night"));
        CHECK_FALSE(core::StandaloneFile::lostAsText(tab->getContent()).formatting);

        // Undo brings the formatting back, as a change not saved
        tab->getBookEditor()->undo();
        CHECK(core::StandaloneFile::lostAsText(tab->getContent()).formatting);
        CHECK(tab->isModified());
    }

    SECTION("A line break: the tab shows the lines of the file, the cursor stays in its place") {
        editor::BookEditor* bookEditor = tab->getBookEditor();
        bookEditor->replaceWithKml(QStringLiteral("<kml><p>Notes of<br/>the night</p></kml>"));
        bookEditor->setCursorPosition({0, 11});  // "th|e night"
        CHECK(answering([&] { window.documents.onSaveDocument(); },
                        MessageDialog::Answer::Accept, said) == 1);
        CHECK(readText(notes) == "Notes of\nthe night");
        CHECK(bookEditor->textDocument()->blockCount() == 2);
        CHECK(bookEditor->cursorPosition() == editor::CursorPosition{1, 2});
        CHECK_FALSE(tab->property("dirty").toBool());
    }
}

TEST_CASE("Save As: a chapter with annotations saved as a plain text file asks first",
          "[gui][save]") {
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QDir folder(files.path());
    const QString chapterPath = folder.filePath(QStringLiteral("storm.kchapter"));
    const QString textPath = folder.filePath(QStringLiteral("storm.txt"));
    core::ChapterDocument chapter;
    chapter.setTitle(QStringLiteral("Storm"));
    chapter.setKml(QStringLiteral(
        "<kml><annotations><annotation id=\"c1\" kind=\"comment\">Which day?</annotation>"
        "</annotations><p>They met on <anchor ref=\"c1\">Tuesday</anchor>.</p></kml>"));
    REQUIRE(chapter.save(chapterPath));

    Window window;
    window.documents.openStandaloneFile(chapterPath);
    gui::EditorPanel* tab = window.editor(window.tabs->currentIndex());
    REQUIRE(tab->property("standaloneFilePath").toString() == chapterPath);

    // Cancel: no file is written, and the tab keeps its chapter
    const test::OwnFileDialogs own;
    QStringList said;
    CHECK(answering([&] { window.documents.onSaveAsDocument(); },
                    MessageDialog::Answer::Cancel, said, textPath) == 2);
    INFO(said.join(QLatin1Char('\n')).toStdString());
    REQUIRE(said.size() == 2);
    CHECK(said.at(0) == QStringLiteral("Save File As"));
    CHECK(said.at(1).contains(QStringLiteral(
        "A plain text file (.txt) keeps only the text, so the annotations will not be saved in "
        "\"storm.txt\".")));
    CHECK_FALSE(QFileInfo::exists(textPath));
    CHECK(tab->property("standaloneFilePath").toString() == chapterPath);

    // Save as Text: the text without the annotation, and the tab takes the text file, without
    // the annotation too
    said.clear();
    CHECK(answering([&] { window.documents.onSaveAsDocument(); },
                    MessageDialog::Answer::Accept, said, textPath) == 2);
    CHECK(readText(textPath) == "They met on Tuesday.");
    CHECK(tab->property("standaloneFilePath").toString() == textPath);
    CHECK(tab->getText() == QStringLiteral("They met on Tuesday."));
    CHECK_FALSE(core::StandaloneFile::lostAsText(tab->getContent()).annotations);
    CHECK_FALSE(tab->property("dirty").toBool());
}

TEST_CASE("Opening a book that another Kalahari program has open says so, with Try Again",
          "[gui][save]") {
    const RecentBooks recent;
    auto& pm = core::ProjectManager::getInstance();
    QTemporaryDir otherFolder;
    REQUIRE(otherFolder.isValid());
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(otherFolder.path(), QStringLiteral("Storm"),
                             QStringLiteral("Author"), QStringLiteral("en"), false,
                             QStringLiteral("kalahari.novel")));
    const QString storm = pm.getManifestPath();
    pm.closeProject(false);
    test::OtherProgram other;
    REQUIRE(other.pid() != 0);
    REQUIRE(test::lockBook(QFileInfo(storm).absolutePath(), other.pid()));

    const QString title = QStringLiteral("Book Already Open ");
    const QString text = QStringLiteral("The book \"Storm\" is already open in another window "
                                        "of Kalahari. Close it there and click Try Again.");

    SECTION("Close: the open book stays open") {
        OpenNovel novel(QStringLiteral("Open Book"));
        const QString open = pm.getManifestPath();
        Window window;
        QStringList said;
        CHECK(answering([&] { window.documents.onOpenRecentFile(storm); },
                        MessageDialog::Answer::Cancel, said) == 1);
        INFO(said.join(QLatin1Char('\n')).toStdString());
        REQUIRE(said.size() == 1);
        CHECK(said.first().startsWith(title));
        CHECK(said.first().contains(text));
        CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(open));
    }

    SECTION("Try Again, after the book is closed in the other program: it opens") {
        if (pm.isProjectOpen()) {
            pm.closeProject(false);
        }
        Window window;
        QStringList said;
        QTimer timer;
        timer.setInterval(10);
        QObject::connect(&timer, &QTimer::timeout, [&]() {
            auto* message = qobject_cast<MessageDialog*>(test::shownDialog());
            if (!message || message->property("handled").toBool()) {
                return;
            }
            message->setProperty("handled", true);
            said << test::textsOf(*message);
            // Try Again once; a window after it would only be closed
            other.stop();
            QMetaObject::invokeMethod(said.size() == 1 ? message->acceptButton()
                                                       : message->cancelButton(),
                                      "click", Qt::QueuedConnection);
        });
        timer.start();
        window.documents.onOpenRecentFile(storm);

        // Tried again once the window is closed
        QCoreApplication::processEvents();
        timer.stop();
        INFO(said.join(QLatin1Char('\n')).toStdString());
        REQUIRE(said.size() == 1);
        CHECK(said.first().startsWith(title));
        CHECK(pm.isProjectOpen());
        CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(storm));
        pm.closeProject(false);
    }
}
