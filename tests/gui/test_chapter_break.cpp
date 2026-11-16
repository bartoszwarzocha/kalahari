// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_chapter_break.cpp
/// @brief Book > Chapter Break: the text from the cursor goes to a new text of the book right
///        after the text it was in, with its notes and annotations, both saved; the title of
///        the new text, when the text cannot be split, and Undo

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "document_window.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/chapter_break_coordinator.h"
#include "kalahari/gui/read_only_book.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <functional>

using namespace kalahari;
using editor::BookEditor;
using editor::CursorPosition;
using gui::ChapterBreakCoordinator;
using test::OpenNovel;
using test::Window;

namespace {

/// Save a text of the book in its file
void save(OpenNovel& book, const QString& id, const QString& kml) {
    book.pm.setChapterContent(id, kml);
    REQUIRE(book.pm.saveChapterContent(id));
}

/// The text of a text of the book as its file on the disk has it
QString fileOf(const OpenNovel& book, const QString& id) {
    const core::ProjectElement* element = book.pm.findElement(id);
    REQUIRE(element != nullptr);
    QFile file(book.pm.filePathOf(*element));
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QJsonDocument::fromJson(file.readAll())
        .object()
        .value(QStringLiteral("content"))
        .toObject()
        .value(QStringLiteral("kml"))
        .toString();
}

/// @p kml as the editor writes it
QString asWritten(const QString& kml) {
    BookEditor editor;
    editor.fromKml(kml);
    return editor.toKml();
}

/// The editor of a text open in a window
BookEditor* openText(Window& window, const QString& id) {
    for (int i = 0; i < window.tabs->count(); ++i) {
        if (window.tabs->widget(i)->property("elementId").toString() == id) {
            return window.editor(i)->getBookEditor();
        }
    }
    return nullptr;
}

/// Open a text of the book in its tab
BookEditor* open(Window& window, const QString& id) {
    const core::ProjectElement* element = test::book().findElement(id);
    REQUIRE(element != nullptr);
    window.navigator.onElementSelected(id, element->title);
    BookEditor* text = openText(window, id);
    REQUIRE(text != nullptr);
    return text;
}

/// The ids of the elements of the main part of the book, in order
QStringList mainIds(const OpenNovel& book) {
    QStringList ids;
    for (const core::ProjectElement& element :
         book.pm.project()->elementsIn(core::BookPlace::Main)) {
        ids << element.id;
    }
    return ids;
}

/// The paragraph of @p text whose text is @p paragraph
int paragraphOf(const BookEditor& text, const QString& paragraph) {
    int found = -1;
    for (QTextBlock block = text.textDocument()->begin(); block.isValid() && found < 0;
         block = block.next()) {
        if (block.text() == paragraph) {
            found = block.blockNumber();
        }
    }
    // A failed REQUIRE ends the test; nothing after it is unreachable for the compilers
    INFO("The paragraph " << paragraph.toStdString());
    REQUIRE(found >= 0);
    return found;
}

/// A chapter with an annotation, a heading and a note after its first two paragraphs
const QString STORM_TEXT = QStringLiteral(
    "<kml><annotations><annotation id=\"a1\" kind=\"comment\">Too slow?</annotation>"
    "</annotations><p>The storm came.</p><p>It rained <b>all</b> night.</p>"
    "<h1>The Morning</h1><p>The sun<footnote id=\"f1\">At six.</footnote> rose "
    "<anchor ref=\"a1\">slowly</anchor>.</p></kml>");

/// Its first two paragraphs, which stay
const QString STORM_BEFORE =
    QStringLiteral("<kml><p>The storm came.</p><p>It rained <b>all</b> night.</p></kml>");

/// The rest, which goes to the new text
const QString STORM_AFTER = QStringLiteral(
    "<kml><annotations><annotation id=\"a1\" kind=\"comment\">Too slow?</annotation>"
    "</annotations><h1>The Morning</h1><p>The sun<footnote id=\"f1\">At six.</footnote> rose "
    "<anchor ref=\"a1\">slowly</anchor>.</p></kml>");

/// A novel of two chapters, "The Storm" with STORM_TEXT and "The Calm" after it
struct SplitBook {
    OpenNovel book{QStringLiteral("Break")};
    QString storm = book.chapterId;
    QString calm;

    SplitBook() {
        calm = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Main),
                                  QStringLiteral("The Calm"), core::BookPlace::Main);
        REQUIRE_FALSE(calm.isEmpty());
        save(book, storm, STORM_TEXT);
        save(book, calm, QStringLiteral("<kml><p>Calm.</p></kml>"));
    }
};

/// Chapter Break over the tabs of a window, as the main window has it; the notices under the
/// new text are kept with their buttons
struct Break {
    BookEditor* keys = nullptr;  ///< The text with the keys
    QStringList notices;
    QList<std::function<void()>> undoButtons;
    ChapterBreakCoordinator coordinator;

    explicit Break(Window& window)
        : coordinator(
              test::book(), test::commands(), window.tabs, &window.navigator,
              [this]() { return keys; },
              [this](gui::EditorPanel*, const QString& message, const QString& actionText,
                     const QString&, const std::function<void()>& action) {
                  notices.append(message + QLatin1Char('|') + actionText);
                  undoButtons.append(action);
              },
              &window.statusBar, &window.window) {}
};

} // namespace

TEST_CASE("Chapter Break: the text from the cursor goes to a new chapter right after it",
          "[gui][chapterbreak]") {
    SplitBook split;
    OpenNovel& book = split.book;
    REQUIRE(book.pm.setStatus(split.storm, QStringLiteral("revision")));
    Window window;
    Break chapterBreak(window);
    BookEditor* text = open(window, split.storm);
    const core::KindRef chapter = book.pm.kindOf(*book.pm.findElement(split.storm));
    const QStringList before = mainIds(book);

    // At the start of the heading
    text->setCursorPosition({paragraphOf(*text, QStringLiteral("The Morning")), 0});
    CHECK(chapterBreak.coordinator.whyNotBreak(text).isEmpty());
    CHECK(chapterBreak.coordinator.canBreak(*text));
    const QString added = chapterBreak.coordinator.breakText(text);
    REQUIRE_FALSE(added.isEmpty());

    // Right after it, of its kind and status, named after the heading
    const core::ProjectElement* element = book.pm.findElement(added);
    REQUIRE(element != nullptr);
    CHECK(element->title == QStringLiteral("The Morning"));
    CHECK(book.pm.kindOf(*element).reference() == chapter.reference());
    CHECK(element->status == QStringLiteral("revision"));
    QStringList expected = before;
    expected.insert(before.indexOf(split.storm) + 1, added);
    CHECK(mainIds(book) == expected);

    // Both saved: the text keeps what was before the cursor, the new text has the rest with
    // its note and its annotation
    CHECK(fileOf(book, split.storm) == asWritten(STORM_BEFORE));
    CHECK(fileOf(book, added) == asWritten(STORM_AFTER));
    CHECK_FALSE(fileOf(book, split.storm).contains(QStringLiteral("Too slow?")));
    CHECK(fileOf(book, added).contains(QStringLiteral(">Too slow?</annotation>")));
    CHECK(fileOf(book, added).contains(QStringLiteral(">At six.")));
    CHECK(text->toKml() == asWritten(STORM_BEFORE));
    CHECK_FALSE(window.navigator.isChapterDirty(split.storm));

    // The new text is in front, with the cursor at its start
    BookEditor* addedText = openText(window, added);
    REQUIRE(addedText != nullptr);
    CHECK(window.tabs->currentWidget()->property("elementId").toString() == added);
    CHECK(addedText->cursorPosition() == CursorPosition{0, 0});
    CHECK(addedText->toKml() == asWritten(STORM_AFTER));
    CHECK_FALSE(window.navigator.isChapterDirty(added));
    CHECK(chapterBreak.notices ==
          QStringList{QStringLiteral(
              "The text from the cursor went to \"The Morning\", right after \"The Storm\".|"
              "&Undo")});

    // Undo, while neither text changed: the new text goes from the book with its file and
    // its tab, the text is whole again and saved
    QCoreApplication::processEvents();
    CHECK(chapterBreak.coordinator.canUndo());
    const QString addedFile = book.pm.filePathOf(*element);
    REQUIRE(QFile::exists(addedFile));
    REQUIRE(chapterBreak.undoButtons.size() == 1);
    chapterBreak.undoButtons.first()();
    CHECK(book.pm.findElement(added) == nullptr);
    CHECK_FALSE(QFile::exists(addedFile));
    QCoreApplication::processEvents();  // the tab closed is deleted later
    CHECK(openText(window, added) == nullptr);
    CHECK(mainIds(book) == before);
    CHECK(text->toKml() == asWritten(STORM_TEXT));
    CHECK(fileOf(book, split.storm) == asWritten(STORM_TEXT));
    CHECK_FALSE(window.navigator.isChapterDirty(split.storm));
    CHECK(window.tabs->currentWidget()->property("elementId").toString() == split.storm);
    CHECK(window.statusBar.currentMessage() == QStringLiteral("\"The Storm\" is whole again."));
    CHECK_FALSE(chapterBreak.coordinator.canUndo());

    // Once more it cannot be undone
    CHECK_FALSE(chapterBreak.coordinator.undo());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("This can no longer be undone: the texts changed since."));
    CHECK(mainIds(book) == before);
}

TEST_CASE("Chapter Break: in a paragraph, at a selection, and after the last text",
          "[gui][chapterbreak]") {
    SplitBook split;
    OpenNovel& book = split.book;
    Window window;
    Break chapterBreak(window);
    BookEditor* text = open(window, split.storm);
    const core::KindRef chapter = book.pm.kindOf(*book.pm.findElement(split.storm));

    SECTION("in a paragraph: the new text is named as a new chapter") {
        const QString title = book.pm.defaultTitle(chapter);
        REQUIRE_FALSE(title.isEmpty());
        // "It rained |all night."
        text->setCursorPosition({1, 10});
        const QString added = chapterBreak.coordinator.breakText(text);
        REQUIRE_FALSE(added.isEmpty());
        CHECK(book.pm.findElement(added)->title == title);
        CHECK(text->toKml() ==
              asWritten(QStringLiteral("<kml><p>The storm came.</p><p>It rained </p></kml>")));
        CHECK(fileOf(book, added) ==
              asWritten(QStringLiteral(
                  "<kml><annotations><annotation id=\"a1\" kind=\"comment\">Too slow?"
                  "</annotation></annotations><p><b>all</b> night.</p><h1>The Morning</h1>"
                  "<p>The sun<footnote id=\"f1\">At six.</footnote> rose <anchor ref=\"a1\">"
                  "slowly</anchor>.</p></kml>")));
        CHECK(chapterBreak.coordinator.undo());
        CHECK(text->toKml() == asWritten(STORM_TEXT));
    }

    SECTION("at a selection: from its start, the selected text going too") {
        text->setSelection({{1, 3}, {2, 4}});
        REQUIRE(text->hasSelection());
        const QString added = chapterBreak.coordinator.breakText(text);
        REQUIRE_FALSE(added.isEmpty());
        CHECK(text->toKml() ==
              asWritten(QStringLiteral("<kml><p>The storm came.</p><p>It </p></kml>")));
        CHECK(fileOf(book, added).contains(QStringLiteral("<p>rained <b>all</b> night.</p>")));
        CHECK_FALSE(text->hasSelection());
    }

    SECTION("after the last chapter: the new text is the last") {
        BookEditor* calm = open(window, split.calm);
        test::write(*window.editor(window.tabs->currentIndex()), QStringLiteral("Calm.\nThe end."));
        calm->setCursorPosition({1, 0});
        const QString added = chapterBreak.coordinator.breakText(calm);
        REQUIRE_FALSE(added.isEmpty());
        CHECK(mainIds(book).constLast() == added);
        // The changes of the text were saved first, then the text without the rest
        CHECK(fileOf(book, split.calm) == asWritten(QStringLiteral("<kml><p>Calm.</p></kml>")));
        CHECK(fileOf(book, added) == asWritten(QStringLiteral("<kml><p>The end.</p></kml>")));
    }
}

TEST_CASE("Chapter Break: when the text cannot be split, the reason", "[gui][chapterbreak]") {
    SECTION("no book") {
        REQUIRE_FALSE(test::book().isProjectOpen());
        ChapterBreakCoordinator coordinator(test::book(), test::commands(), nullptr, nullptr, {}, {},
                                            nullptr, nullptr);
        CHECK(coordinator.whyNotBreak(nullptr) == QStringLiteral("No book is open"));
    }

    SplitBook split;
    OpenNovel& book = split.book;
    Window window;
    Break chapterBreak(window);
    BookEditor* text = open(window, split.storm);
    const auto reason = [&chapterBreak](const BookEditor* of) {
        return chapterBreak.coordinator.whyNotBreak(of).toStdString();
    };

    SECTION("no text, or a text outside the book's") {
        CHECK(reason(nullptr) == "The cursor is not in a text of the book");
        BookEditor outside;
        outside.fromKml(STORM_TEXT);
        outside.setCursorPosition({1, 0});
        CHECK(reason(&outside) == "The cursor is not in a text of the book");

        // A text of the workshop is not a text of the book
        const core::KindRef note = book.pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                                QStringLiteral("work_note"));
        REQUIRE(note);
        const QString ideas =
            book.pm.addElement(note, QStringLiteral("Ideas"), core::BookPlace::Workshop);
        REQUIRE_FALSE(ideas.isEmpty());
        save(book, ideas, QStringLiteral("<kml><p>One.</p><p>Two.</p></kml>"));
        BookEditor* workshop = open(window, ideas);
        workshop->setCursorPosition({1, 0});
        CHECK(reason(workshop) == "The cursor is not in a text of the book");
    }

    SECTION("a text that cannot change") {
        text->setCursorPosition({1, 0});
        text->setReadOnly(true);
        CHECK(reason(text) == gui::readOnlyReason().toStdString());
        text->setReadOnly(false);
        CHECK(reason(text).empty());
    }

    SECTION("a kind the book has as many texts of as it can have") {
        const core::KindRef prologue = book.pm.bookTypes().findKind(
            QStringLiteral("kalahari.novel"), QStringLiteral("prologue"));
        REQUIRE(prologue);
        const QString id =
            book.pm.addElement(prologue, QStringLiteral("Prologue"), core::BookPlace::Main);
        REQUIRE_FALSE(id.isEmpty());
        save(book, id, QStringLiteral("<kml><p>Long ago.</p><p>Very long ago.</p></kml>"));
        BookEditor* first = open(window, id);
        first->setCursorPosition({1, 0});
        CHECK(reason(first) == "No prologue can be added after this text");
    }

    SECTION("a table") {
        save(book, split.storm,
             QStringLiteral("<kml><p>Before.</p><table><tr><td><p>cell</p></td></tr></table>"
                            "<p>After.</p></kml>"));
        window.navigator.closeTabsOf(*book.pm.findElement(split.storm));
        QCoreApplication::processEvents();
        BookEditor* tabled = open(window, split.storm);
        tabled->setCursorPosition({paragraphOf(*tabled, QStringLiteral("cell")), 2});
        REQUIRE(tabled->isInTable());
        CHECK(reason(tabled) == "The cursor is in a table, which cannot be split");
        tabled->setCursorPosition({paragraphOf(*tabled, QStringLiteral("After.")), 0});
        CHECK(reason(tabled).empty());
    }

    SECTION("nothing before or after the cursor") {
        save(book, split.storm,
             QStringLiteral("<kml><p></p><p>  Only text.  </p><p> </p></kml>"));
        window.navigator.closeTabsOf(*book.pm.findElement(split.storm));
        QCoreApplication::processEvents();
        BookEditor* only = open(window, split.storm);
        REQUIRE(only->textDocument()->findBlockByNumber(1).text() ==
                QStringLiteral("  Only text.  "));
        only->setCursorPosition({0, 0});
        CHECK(reason(only) == "There is no text before the cursor");
        only->setCursorPosition({1, 2});
        CHECK(reason(only) == "There is no text before the cursor");
        only->setCursorPosition({1, 3});
        CHECK(reason(only).empty());
        only->setCursorPosition({1, 12});
        CHECK(reason(only) == "There is no text after the cursor");
        only->setCursorPosition({2, 1});
        CHECK(reason(only) == "There is no text after the cursor");

        // The command says why in the status bar, and the book stays as it is
        const QStringList before = mainIds(book);
        chapterBreak.keys = only;
        CHECK(chapterBreak.coordinator.breakText(only).isEmpty());
        CHECK(window.statusBar.currentMessage() ==
              QStringLiteral("There is no text after the cursor"));
        CHECK(mainIds(book) == before);
        CHECK(chapterBreak.notices.isEmpty());
    }
}

TEST_CASE("Chapter Break: Undo is gone once either text changes or closes",
          "[gui][chapterbreak]") {
    SplitBook split;
    OpenNovel& book = split.book;
    Window window;
    Break chapterBreak(window);
    BookEditor* text = open(window, split.storm);
    text->setCursorPosition({paragraphOf(*text, QStringLiteral("The Morning")), 0});
    const QString added = chapterBreak.coordinator.breakText(text);
    REQUIRE_FALSE(added.isEmpty());
    BookEditor* addedText = openText(window, added);
    REQUIRE(addedText != nullptr);
    REQUIRE(chapterBreak.coordinator.canUndo());

    SECTION("the new text changes") {
        addedText->insertText(QStringLiteral("At dawn. "));
        CHECK_FALSE(chapterBreak.coordinator.canUndo());
    }
    SECTION("the text changes") {
        text->setCursorPosition({0, 0});
        text->insertText(QStringLiteral("Then "));
        CHECK_FALSE(chapterBreak.coordinator.canUndo());
    }
    SECTION("the new text closes") {
        window.navigator.closeTabsOf(*book.pm.findElement(added));
        QCoreApplication::processEvents();
        CHECK_FALSE(chapterBreak.coordinator.canUndo());
    }
    SECTION("the book closes") {
        book.pm.closeProject(false);
        CHECK_FALSE(chapterBreak.coordinator.canUndo());
        return;
    }

    // Undo says it can no longer be done, and both texts stay
    CHECK_FALSE(chapterBreak.coordinator.undo());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("This can no longer be undone: the texts changed since."));
    CHECK(book.pm.findElement(added) != nullptr);
    CHECK(book.pm.findElement(split.storm) != nullptr);
}

TEST_CASE("Chapter Break: the title of the new text is a heading at its start",
          "[gui][chapterbreak]") {
    BookEditor editor;
    const auto titleAt = [&editor](const QString& kml, const CursorPosition& at) {
        editor.fromKml(kml);
        const QTextBlock block = editor.textDocument()->findBlockByNumber(at.paragraph);
        return gui::chapterBreakTitle(*editor.textDocument(), block.position() + at.offset)
            .toStdString();
    };

    CHECK(titleAt(QStringLiteral("<kml><p>a</p><h2>The Night</h2><p>b</p></kml>"), {1, 0}) ==
          "The Night");
    // Empty paragraphs before it, its line breaks, tabs and the marks of notes on one line
    CHECK(titleAt(QStringLiteral("<kml><p>a</p><p></p><p> </p><h1>The <i>Long</i><br/>Night\tOne"
                                 "<footnote id=\"f1\">Note.</footnote></h1><p>b</p></kml>"),
                  {1, 0}) == "The Long Night One");
    // From the cursor in the heading: its rest
    CHECK(titleAt(QStringLiteral("<kml><h1>Part One: The Night</h1><p>b</p></kml>"), {0, 10}) ==
          "The Night");
    // From the end of the paragraph before it
    CHECK(titleAt(QStringLiteral("<kml><p>a</p><h1>The Night</h1></kml>"), {0, 1}) ==
          "The Night");

    // No heading first: no title
    CHECK(titleAt(QStringLiteral("<kml><p>a</p><p>The night.</p><h1>Later</h1></kml>"),
                  {1, 0}).empty());
    CHECK(titleAt(QStringLiteral("<kml><p>a</p><p></p><table><tr><td><h1>In a cell</h1></td>"
                                 "</tr></table><p>b</p></kml>"),
                  {1, 0}).empty());
    CHECK(titleAt(QStringLiteral("<kml><p>a</p><p><img src=\"images/a.png\"/></p>"
                                 "<h1>After the image</h1></kml>"),
                  {1, 0}).empty());
}
