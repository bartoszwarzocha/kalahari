/// @file test_text_links.cpp
/// @brief Links in the text: KML, Insert > Link, typing next to a link, the context menu,
///        Ctrl+click, the clipboard, the look and autocorrect of typed addresses

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/auto_correct.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/editor/text_link.h>
#include <kalahari/editor/text_source_adapter.h>
#include "editor_test_utils.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QTextBlock>
#include <memory>
#include <string>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

const QString PAGE = QStringLiteral("https://example.com");

/// KML with the paragraphs given
QString kml(const char* paragraphs) {
    return QStringLiteral("<kml>") + QString::fromUtf8(paragraphs) + QStringLiteral("</kml>");
}

/// For readable failures
std::string utf8(const QString& text) {
    return text.toStdString();
}

/// An editor with KML loaded
std::unique_ptr<BookEditor> editorWith(const QString& content) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(content);
    return editor;
}

/// A key pressed as the keyboard does it, with the text it types
void press(BookEditor& editor, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QCoreApplication::sendEvent(&editor, &event);
}

/// Type the text as the keyboard does, a character at a time
void type(BookEditor& editor, const QString& text) {
    for (const QChar ch : text) {
        press(editor, 0, Qt::NoModifier, QString(ch));
    }
}

/// The links of a paragraph
std::vector<TextLink> linksOf(const BookEditor& editor, int paragraph) {
    return linksIn(editor.textDocument()->findBlockByNumber(paragraph));
}

/// A point on the character at @p offset of the first paragraph
QPointF pointOn(BookEditor& editor, int offset) {
    editor.setCursorPosition({0, offset});
    const QRect caret = editor.inputMethodQuery(Qt::ImCursorRectangle).toRect();
    return QPointF(caret.center()) + QPointF(3.0, 0.0);
}

/// A mouse press at a point, with the modifiers
void click(BookEditor& editor, const QPointF& point, Qt::KeyboardModifiers modifiers) {
    QMouseEvent pressed(QEvent::MouseButtonPress, point, editor.mapToGlobal(point),
                        Qt::LeftButton, Qt::LeftButton, modifiers);
    QCoreApplication::sendEvent(&editor, &pressed);
    QMouseEvent released(QEvent::MouseButtonRelease, point, editor.mapToGlobal(point),
                         Qt::LeftButton, Qt::NoButton, modifiers);
    QCoreApplication::sendEvent(&editor, &released);
}

}  // anonymous namespace

TEST_CASE("Links: KML keeps them as <a href>", "[editor][links][kml]") {
    SECTION("Read and written again, also across formats") {
        const QString content = kml(
            "<p>See <a href=\"https://example.com\">the </a><a href=\"https://example.com\"><b>"
            "page</b></a>.</p>");
        auto editor = editorWith(content);
        CHECK(utf8(editor->toKml()) == utf8(content));
        // One link of the two pieces with different formats
        const std::vector<TextLink> links = linksOf(*editor, 0);
        REQUIRE(links.size() == 1);
        CHECK(links.front().from == 4);
        CHECK(links.front().to == 12);
        CHECK(links.front().address == PAGE);
    }

    SECTION("Its address escaped, inside an annotation's anchor") {
        const QString content = kml("<p><a href=\"https://example.com/?a=1&amp;b=&quot;2&quot;\">"
                                    "x</a></p>");
        auto editor = editorWith(content);
        CHECK(linksOf(*editor, 0).front().address ==
              QStringLiteral("https://example.com/?a=1&b=\"2\""));
        CHECK(utf8(editor->toKml()) == utf8(content));
    }

    SECTION("Without an address the text is plain; spaces around one go") {
        auto editor = editorWith(kml("<p><a>plain</a> <a href=\" https://example.com \">x</a></p>"));
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p>plain <a href=\"https://example.com\">x</a></p>")));
    }

    SECTION("The text of a link is the paragraph's (an element the editor reads)") {
        CHECK(KmlFormatRegistry::isInlineTextTag(QStringLiteral("a")));
    }
}

TEST_CASE("Links: the addresses the program keeps and opens", "[editor][links]") {
    CHECK(normalizedLinkAddress(QStringLiteral("  www.example.com ")) ==
          QStringLiteral("https://www.example.com"));
    CHECK(normalizedLinkAddress(QStringLiteral("jan@example.com")) ==
          QStringLiteral("mailto:jan@example.com"));
    CHECK(normalizedLinkAddress(QStringLiteral("http://example.com")) ==
          QStringLiteral("http://example.com"));
    CHECK(normalizedLinkAddress(QStringLiteral("mailto:jan@example.com")) ==
          QStringLiteral("mailto:jan@example.com"));
    CHECK(normalizedLinkAddress(QStringLiteral("localhost:8080/a")) ==
          QStringLiteral("https://localhost:8080/a"));
    CHECK(normalizedLinkAddress(QStringLiteral("#chapter")) == QStringLiteral("#chapter"));
    CHECK(normalizedLinkAddress(QStringLiteral("   ")).isEmpty());

    CHECK(isOpenableLinkAddress(QStringLiteral("https://example.com/a?b=c")));
    CHECK(isOpenableLinkAddress(QStringLiteral("http://example.com")));
    CHECK(isOpenableLinkAddress(QStringLiteral("ftp://example.com/file")));
    CHECK(isOpenableLinkAddress(QStringLiteral("mailto:jan@example.com")));
    CHECK_FALSE(isOpenableLinkAddress(QStringLiteral("file:///C:/Windows/notepad.exe")));
    CHECK_FALSE(isOpenableLinkAddress(QStringLiteral("javascript:alert(1)")));
    CHECK_FALSE(isOpenableLinkAddress(QStringLiteral("https://")));
    CHECK_FALSE(isOpenableLinkAddress(QStringLiteral("mailto:")));
    CHECK_FALSE(isOpenableLinkAddress(QStringLiteral("#chapter")));
}

TEST_CASE("Links: text typed after a link is not a part of it, inside it is",
          "[editor][links]") {
    auto editor = editorWith(kml("<p>A <a href=\"https://example.com\">link</a> here</p>"));
    editor->setCursorPosition({0, 6});  // after "link"
    type(*editor, QStringLiteral("s"));
    CHECK(utf8(editor->toKml()) ==
          utf8(kml("<p>A <a href=\"https://example.com\">link</a>s here</p>")));

    editor->setCursorPosition({0, 2});  // before "link"
    type(*editor, QStringLiteral("x"));
    CHECK(utf8(editor->toKml()) ==
          utf8(kml("<p>A x<a href=\"https://example.com\">link</a>s here</p>")));

    editor->setCursorPosition({0, 4});  // l|ink
    type(*editor, QStringLiteral("o"));
    CHECK(utf8(editor->toKml()) ==
          utf8(kml("<p>A x<a href=\"https://example.com\">loink</a>s here</p>")));

    // Enter after it: the new paragraph is not a link either
    editor->setCursorPosition({0, 8});
    press(*editor, Qt::Key_Return);
    type(*editor, QStringLiteral("y"));
    CHECK(linksOf(*editor, 1).empty());
}

TEST_CASE("Links: what Insert > Link changes at the cursor", "[editor][links]") {
    auto editor = editorWith(kml("<p>One <a href=\"https://example.com\">two</a> three</p>"
                                 "<p>Four</p>"));

    SECTION("On a link, at its end too: the link") {
        for (const int offset : {4, 5, 7}) {
            INFO(offset);
            editor->setCursorPosition({0, offset});
            const LinkTarget target = editor->linkTarget();
            CHECK(target == LinkTarget{4, 7, QStringLiteral("two"), PAGE, true, true});
        }
        // A selection inside it
        editor->setSelection({{0, 5}, {0, 6}});
        CHECK(editor->linkTarget().existing);
    }

    SECTION("A selection: its text becomes the link") {
        editor->setSelection({{0, 0}, {0, 3}});
        CHECK(editor->linkTarget() == LinkTarget{0, 3, QStringLiteral("One"), {}, false, true});
        // Over paragraphs: its text stays as it is
        editor->setSelection({{0, 8}, {1, 2}});
        const LinkTarget over = editor->linkTarget();
        CHECK(over.text == QStringLiteral("three Fo"));
        CHECK_FALSE(over.textEditable);
        CHECK_FALSE(over.existing);
    }

    SECTION("Elsewhere: a new link at the cursor") {
        editor->setCursorPosition({0, 2});
        CHECK(editor->linkTarget() == LinkTarget{2, 2, {}, {}, false, true});
    }
}

TEST_CASE("Links: Insert > Link makes, edits and removes them as one undo step each",
          "[editor][links]") {
    SECTION("At the cursor: the text, or the address as written, in the format typed there") {
        auto editor = editorWith(kml("<p><b>Ab</b></p>"));
        editor->setCursorPosition({0, 1});
        REQUIRE(editor->setLink(editor->linkTarget(), QStringLiteral("page"),
                                QStringLiteral("www.example.com")));
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p><b>A</b><a href=\"https://www.example.com\"><b>page</b></a><b>b</b>"
                       "</p>")));
        CHECK(editor->cursorPosition() == CursorPosition{0, 5});  // after the link
        editor->undo();
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><b>Ab</b></p>")));
        CHECK(editor->cursorPosition() == CursorPosition{0, 1});
        editor->redo();
        CHECK(linksOf(*editor, 0).size() == 1);

        editor->fromKml(kml("<p>Ab</p>"));
        editor->setCursorPosition({0, 2});
        REQUIRE(editor->setLink(editor->linkTarget(), QString(), QStringLiteral(" jan@example.com")));
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p>Ab<a href=\"mailto:jan@example.com\">jan@example.com</a></p>")));
    }

    SECTION("A selection whose text stays keeps its formatting") {
        auto editor = editorWith(kml("<p>One <i>two</i> three</p>"));
        editor->setSelection({{0, 0}, {0, 7}});
        const LinkTarget target = editor->linkTarget();
        REQUIRE(editor->setLink(target, target.text, PAGE));
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p><a href=\"https://example.com\">One </a><a href=\"https://example.com\">"
                       "<i>two</i></a> three</p>")));
        CHECK_FALSE(editor->hasSelection());
        editor->undo();
        CHECK(utf8(editor->toKml()) == utf8(kml("<p>One <i>two</i> three</p>")));
        CHECK(editor->hasSelection());  // as it was made
    }

    SECTION("A new text in place of the selection, in the format of its first character") {
        auto editor = editorWith(kml("<p>One <i>two</i> three</p>"));
        editor->setSelection({{0, 4}, {0, 7}});
        REQUIRE(editor->setLink(editor->linkTarget(), QStringLiteral("2"), PAGE));
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p>One <a href=\"https://example.com\"><i>2</i></a> three</p>")));
    }

    SECTION("A selection over paragraphs: each paragraph's text becomes a link") {
        auto editor = editorWith(kml("<p>One</p><p>Two</p>"));
        editor->selectAll();
        const LinkTarget target = editor->linkTarget();
        REQUIRE(editor->setLink(target, QStringLiteral("ignored"), PAGE));
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p><a href=\"https://example.com\">One</a></p>"
                       "<p><a href=\"https://example.com\">Two</a></p>")));
    }

    SECTION("A link edited: its address, its text; removed, its text stays") {
        auto editor = editorWith(kml("<p>A <a href=\"https://example.com\"><b>link</b></a>.</p>"));
        editor->setCursorPosition({0, 3});
        REQUIRE(editor->setLink(editor->linkTarget(), QStringLiteral("link"),
                                QStringLiteral("https://kalahari.example")));
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p>A <a href=\"https://kalahari.example\"><b>link</b></a>.</p>")));

        editor->setCursorPosition({0, 3});
        REQUIRE(editor->setLink(editor->linkTarget(), QStringLiteral("page"),
                                QStringLiteral("https://kalahari.example")));
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p>A <a href=\"https://kalahari.example\"><b>page</b></a>.</p>")));

        editor->setCursorPosition({0, 3});
        REQUIRE(editor->removeLink(editor->linkTarget()));
        CHECK(utf8(editor->toKml()) == utf8(kml("<p>A <b>page</b>.</p>")));
        editor->undo();
        CHECK(linksOf(*editor, 0).size() == 1);
        // Nothing to remove: no step
        editor->setCursorPosition({0, 0});
        CHECK_FALSE(editor->removeLink(editor->linkTarget()));
    }

    SECTION("No address, or a read-only text: nothing changes") {
        auto editor = editorWith(kml("<p>Ab</p>"));
        CHECK_FALSE(editor->setLink(editor->linkTarget(), QStringLiteral("x"), QStringLiteral(" ")));
        editor->setReadOnly(true);
        int refused = 0;
        QObject::connect(editor.get(), &BookEditor::changeRefused, [&refused]() { ++refused; });
        CHECK_FALSE(editor->setLink(editor->linkTarget(), QStringLiteral("x"), PAGE));
        CHECK(refused == 1);
        CHECK(utf8(editor->toKml()) == utf8(kml("<p>Ab</p>")));
    }
}

TEST_CASE("Links: the context menu of a link, Ctrl+click and the link under the mouse",
          "[editor][links][menu]") {
    auto editor = editorWith(kml("<p>A <a href=\"https://example.com\">link</a> here</p>"));
    editor->setCursorPosition({0, 3});
    QStringList texts;
    {
        PopupMenuReader reader(texts, QString());
        editor->showContextMenuAtCursor();
    }
    for (const char* text : {"Edit &Hyperlink...", "&Open Hyperlink", "Cop&y Hyperlink",
                             "Remove Hyperl&ink"}) {
        INFO(text);
        CHECK(texts.contains(QString::fromLatin1(text)));
    }

    int dialogAsked = 0;
    QObject::connect(editor.get(), &BookEditor::linkDialogAsked,
                     [&dialogAsked]() { ++dialogAsked; });
    QStringList opened;
    QObject::connect(editor.get(), &BookEditor::linkOpenAsked,
                     [&opened](const QString& address) { opened.append(address); });
    {
        PopupMenuReader reader(texts, QStringLiteral("Edit &Hyperlink..."));
        editor->showContextMenuAtCursor();
    }
    CHECK(dialogAsked == 1);
    {
        PopupMenuReader reader(texts, QStringLiteral("&Open Hyperlink"));
        editor->showContextMenuAtCursor();
    }
    REQUIRE(opened.size() == 1);
    CHECK(opened.front() == PAGE);
    {
        PopupMenuReader reader(texts, QStringLiteral("Cop&y Hyperlink"));
        editor->showContextMenuAtCursor();
    }
    CHECK(QGuiApplication::clipboard()->text() == PAGE);
    {
        PopupMenuReader reader(texts, QStringLiteral("Remove Hyperl&ink"));
        editor->showContextMenuAtCursor();
    }
    CHECK(linksOf(*editor, 0).empty());
    editor->undo();

    // Not on a link: none of them
    editor->setCursorPosition({0, 0});
    texts.clear();
    {
        PopupMenuReader reader(texts, QString());
        editor->showContextMenuAtCursor();
    }
    CHECK_FALSE(texts.contains(QStringLiteral("&Open Hyperlink")));

    // The link under a point; Ctrl+click opens it, a click places the cursor
    const QPointF onLink = pointOn(*editor, 3);
    REQUIRE(editor->linkAt(onLink).has_value());
    CHECK(editor->linkAt(onLink)->address == PAGE);
    CHECK_FALSE(editor->linkAt(pointOn(*editor, 0)).has_value());
    editor->setCursorPosition({0, 0});
    click(*editor, onLink, Qt::ControlModifier);
    CHECK(opened.size() == 2);
    CHECK(editor->cursorPosition() == CursorPosition{0, 0});  // the cursor stays
    click(*editor, onLink, Qt::NoModifier);
    CHECK(opened.size() == 2);
    CHECK(editor->cursorPosition() != CursorPosition{0, 0});
}

TEST_CASE("Links: the clipboard keeps them", "[editor][links][paste]") {
    auto editor = editorWith(kml("<p>A <a href=\"https://example.com\">link</a>.</p>"));
    editor->selectAll();
    const std::unique_ptr<QMimeData> copied = editor->createMimeDataFromSelection();
    REQUIRE(copied);
    CHECK(utf8(copied->text()) == "A link.");
    CHECK(utf8(copied->html()) ==
          utf8(ClipboardHandler::htmlDocument(
              QStringLiteral("<p>A <a href=\"https://example.com\">link</a>.</p>"))));

    // A link that only Kalahari keeps: its text for the other programs
    CHECK(utf8(ClipboardHandler::kmlToHtml(kml("<p><a href=\"file:///x.exe\">x</a></p>"))) ==
          "<p>x</p>");

    // Pasted from another program: links to pages and e-mail addresses, not to a place in the
    // page or a script; Keep Text Only keeps none
    const QString html = QStringLiteral(
        "<p><a href=\"https://example.com\">page</a> <a href=\"#top\">top</a> "
        "<a href=\"javascript:alert(1)\">script</a> <a href=\"mailto:jan@example.com\">mail</a>"
        "</p>");
    CHECK(utf8(ClipboardHandler::htmlToKml(html, PasteMode::Merge)) ==
          utf8(kml("<p><a href=\"https://example.com\">page</a> top script "
                   "<a href=\"mailto:jan@example.com\">mail</a></p>")));
    CHECK(utf8(ClipboardHandler::htmlToKml(html, PasteMode::Text)) ==
          utf8(kml("<p>page top script mail</p>")));

    // Copied and pasted in Kalahari: the link with its text
    editor->clearSelection();
    editor->setCursorPosition({0, 7});
    editor->insertFromMimeData(copied.get());
    CHECK(utf8(editor->toKml()) ==
          utf8(kml("<p>A <a href=\"https://example.com\">link</a>.A <a href=\"https://example.com\">"
                   "link</a>.</p>")));
}

TEST_CASE("Links: drawn in the link color of the paper, underlined", "[editor][links][render]") {
    EditorColors colors;
    CHECK(colors.linkColor(EditorColorMode::Light) == QColor(QStringLiteral("#0563c1")));
    CHECK(colors.linkColor(EditorColorMode::Dark) == QColor(QStringLiteral("#8ab4f8")));

    // The text source gives the pipeline the link's run
    auto editor = editorWith(kml("<p>A <a href=\"https://example.com\">link</a></p>"));
    QTextDocumentSource source(const_cast<QTextDocument*>(editor->textDocument()));
    const std::vector<ColoredText> runs = source.paragraphColors(0);
    REQUIRE(runs.size() == 1);
    CHECK(runs.front().start == 2);
    CHECK(runs.front().length == 4);
    CHECK(runs.front().link);
    CHECK_FALSE(runs.front().color.isValid());
}

TEST_CASE("Autocorrect: an Internet address typed in the text becomes a link",
          "[editor][links][autocorrect]") {
    const AutoCorrectOptions options;

    SECTION("The addresses it knows, without the marks around them") {
        CHECK(autoLink(QStringLiteral("See www.example.com"), options) ==
              AutoLink{4, 19, QStringLiteral("https://www.example.com")});
        CHECK(autoLink(QStringLiteral("(https://example.com/a?b=1)."), options) ==
              AutoLink{1, 26, QStringLiteral("https://example.com/a?b=1")});
        CHECK(autoLink(QStringLiteral("„jan@example.com”,"), options) ==
              AutoLink{1, 16, QStringLiteral("mailto:jan@example.com")});
        CHECK(autoLink(QStringLiteral("ftp://files.example.com"), options).has_value());
        // A bracket the address opened stays
        CHECK(autoLink(QStringLiteral("https://en.wikipedia.org/wiki/Kalahari_(desert)"), options)
                  ->end == 47);
        CHECK_FALSE(autoLink(QStringLiteral("www.example"), options).has_value());
        CHECK_FALSE(autoLink(QStringLiteral("example.com"), options).has_value());
        CHECK_FALSE(autoLink(QStringLiteral("e.g."), options).has_value());
        CHECK_FALSE(autoLink(QStringLiteral("http://"), options).has_value());
        AutoCorrectOptions off;
        off.links = false;
        CHECK_FALSE(autoLink(QStringLiteral("www.example.com"), off).has_value());
    }

    SECTION("No capital at the start of an address") {
        for (const char* typed : {"www.", "https:", "jan@example."}) {
            INFO(typed);
            const QString text = QString::fromUtf8(typed);
            CHECK_FALSE(autoCorrection(text, options, [] { return QuoteMarks{}; }).has_value());
        }
        // A sentence goes on as before
        CHECK(autoCorrection(QStringLiteral("ala."), options, [] { return QuoteMarks{}; })
                  .has_value());
    }

    SECTION("Typed with a space or Enter after it, as an undo step of its own") {
        auto editor = editorWith(kml("<p></p>"));
        editor->setAutoCorrect(options);
        type(*editor, QStringLiteral("www.example.com is it. "));
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p><a href=\"https://www.example.com\">www.example.com</a> is it. </p>")));
        editor->fromKml(kml("<p></p>"));
        type(*editor, QStringLiteral("Write to jan@example.com "));
        CHECK(linksOf(*editor, 0).size() == 1);
        editor->undo();  // right after it: the address as typed
        CHECK(linksOf(*editor, 0).empty());
        CHECK(editor->plainText() == QStringLiteral("Write to jan@example.com "));

        editor->fromKml(kml("<p></p>"));
        type(*editor, QStringLiteral("https://example.com"));
        press(*editor, Qt::Key_Return);
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p><a href=\"https://example.com\">https://example.com</a></p><p></p>")));

        // Off: the address stays text
        AutoCorrectOptions off = options;
        off.links = false;
        editor->setAutoCorrect(off);
        editor->fromKml(kml("<p></p>"));
        type(*editor, QStringLiteral("www.example.com "));
        CHECK(linksOf(*editor, 0).empty());
    }

    SECTION("A link the writer made stays as it is") {
        auto editor = editorWith(kml("<p><a href=\"https://kalahari.example\">www.example.com</a></p>"));
        editor->setAutoCorrect(options);
        editor->setCursorPosition({0, 15});
        type(*editor, QStringLiteral(" "));
        REQUIRE(linksOf(*editor, 0).size() == 1);
        CHECK(linksOf(*editor, 0).front().address == QStringLiteral("https://kalahari.example"));
    }
}
