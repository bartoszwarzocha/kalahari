/// @file test_note_checking.cpp
/// @brief The texts of the notes in the tools of the text: the search and the replacement,
///        the spelling and the grammar (a LanguageTool stand-in on localhost)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/find_replace_bar.h>
#include <kalahari/editor/grammar_check_service.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/text_footnotes.h>
#include <kalahari/editor/text_issue.h>
#include <kalahari/gui/utils/access_keys.h>
#include "editor_test_utils.h"
#include "fake_language_tool.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QImage>
#include <QMenu>
#include <QRect>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// An editor with @p kml loaded, every paragraph laid out
std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(700, 500));
    EditorAppearance appearance;
    appearance.cursor.blinking = false;
    editor->setAppearance(appearance);
    editor->fromKml(kml);
    return editor;
}

/// The text of a note of an editor (empty: no such note)
QString contentOf(const BookEditor& editor, const QString& id) {
    const std::optional<NotePlace> place = findNote(*editor.textDocument(), id);
    return place ? place->note.content : QString();
}

/// A text with notes: "burza" in its text, in a footnote twice and in the second paragraph of
/// an endnote
const QString STORM = QStringLiteral(
    "<kml><p>Burza nad miastem<footnote id=\"f1\"><b>Burza</b> z roku 1931. Wielka burza."
    "</footnote> i burza.</p><p>Koniec<endnote id=\"e1\"><p>Pierwszy</p><p>Burza</p>"
    "</endnote></p></kml>");

}  // namespace

// =============================================================================
// Search and replace
// =============================================================================

TEST_CASE("Notes: the search finds the texts of the notes, at their numbers",
          "[editor][footnotes][search]") {
    auto editor = editorWith(STORM);
    SearchEngine engine;
    engine.setDocument(editor->textDocument());
    engine.setSearchText(QStringLiteral("burza"));

    // In the order of the text: a note's text at its number
    const std::vector<SearchMatch> matches = engine.findAll();
    REQUIRE(matches.size() == 5);
    CHECK_FALSE(matches[0].inNote());
    CHECK(matches[0].start == 0);
    CHECK(matches[1].noteId == QStringLiteral("f1"));
    CHECK(matches[1].start == 17);  // the number's character
    CHECK(matches[1].end() == 18);
    CHECK(matches[1].paragraph == 0);
    CHECK(matches[1].paragraphOffset == 17);
    CHECK(matches[1].noteOffset == 0);
    CHECK(matches[1].length == 5);
    CHECK(matches[1].matchedText == QStringLiteral("Burza"));
    CHECK(matches[2].noteId == QStringLiteral("f1"));
    CHECK(matches[2].noteOffset == 26);
    CHECK(matches[2].matchedText == QStringLiteral("burza"));
    CHECK_FALSE(matches[3].inNote());
    CHECK(matches[3].start == 21);
    // In a note's second paragraph: after the first one and its end
    CHECK(matches[4].noteId == QStringLiteral("e1"));
    CHECK(matches[4].start == 34);
    CHECK(matches[4].paragraph == 1);
    CHECK(matches[4].paragraphOffset == 6);
    CHECK(matches[4].noteOffset == 9);

    SECTION("with the options of the search") {
        engine.setOptions({.caseSensitive = true});
        CHECK(engine.totalMatchCount() == 2);
        engine.setOptions({.wholeWord = true});
        engine.setSearchText(QStringLiteral("Burz"));
        CHECK(engine.totalMatchCount() == 0);
        // Paragraph by paragraph in a note too
        engine.setOptions({.useRegex = true});
        engine.setSearchText(QStringLiteral("Pierwszy\\sBurza"));
        CHECK(engine.totalMatchCount() == 0);
        engine.setSearchText(QStringLiteral("19\\d1"));
        CHECK(engine.totalMatchCount() == 1);
    }

    SECTION("a note's new text, searched again") {
        REQUIRE(editor->setNoteContent(QStringLiteral("f1"), QStringLiteral("Cisza")));
        CHECK(engine.totalMatchCount() == 3);
        editor->undo();
        CHECK(engine.totalMatchCount() == 5);
    }

    SECTION("on from a range of a note's text the selected number stands for") {
        engine.setOrigin(17, 18, NoteRange{QStringLiteral("f1"), 0, 5});
        CHECK(engine.currentMatchIndex() == 1);
        engine.setOrigin(17, 18, NoteRange{QStringLiteral("f1"), 3, 4});  // not a match
        CHECK(engine.currentMatchIndex() == -1);
        CHECK(engine.nextMatch().noteOffset == 26);
        engine.setOrigin(17, 18, NoteRange{QStringLiteral("f1"), 3, 4});
        CHECK(engine.previousMatch().noteOffset == 0);
        // The cursor before the number: the note's text after it; after the number: not
        engine.setOrigin(17, 17);
        CHECK(engine.nextMatch().noteOffset == 0);
        engine.setOrigin(18, 18);
        CHECK(engine.nextMatch().start == 21);
        engine.setOrigin(18, 18);
        CHECK(engine.previousMatch().noteOffset == 26);
    }

    SECTION("Replace: in the note's text, in its formatting, as one undo step") {
        const QString before = contentOf(*editor, QStringLiteral("f1"));
        engine.setReplaceText(QStringLiteral("Wichura"));
        REQUIRE(engine.setCurrentMatchIndex(1));
        REQUIRE(engine.replaceCurrent());
        CHECK(contentOf(*editor, QStringLiteral("f1")) ==
              QStringLiteral("<b>Wichura</b> z roku 1931. Wielka burza."));
        CHECK(engine.replacementEnd() == 18);  // after the number
        // On with the next match: the other one in the note
        CHECK(engine.currentMatch().noteOffset == 28);
        REQUIRE(engine.replaceCurrent());
        CHECK(contentOf(*editor, QStringLiteral("f1")) ==
              QStringLiteral("<b>Wichura</b> z roku 1931. Wielka Wichura."));
        CHECK(engine.currentMatch().start == 21);
        editor->undo();
        editor->undo();
        CHECK(contentOf(*editor, QStringLiteral("f1")) == before);
    }

    SECTION("Replace All: the text and the notes, as one undo step") {
        engine.setReplaceText(QStringLiteral("Wichura"));
        CHECK(engine.replaceAll() == 5);
        CHECK(editor->textDocument()->begin().text().startsWith(QStringLiteral("Wichura nad")));
        CHECK(contentOf(*editor, QStringLiteral("f1")) ==
              QStringLiteral("<b>Wichura</b> z roku 1931. Wielka Wichura."));
        CHECK(contentOf(*editor, QStringLiteral("e1")) ==
              QStringLiteral("<p>Pierwszy</p><p>Wichura</p>"));
        CHECK(engine.totalMatchCount() == 0);
        editor->undo();
        CHECK(engine.totalMatchCount() == 5);
        CHECK(contentOf(*editor, QStringLiteral("e1")) ==
              QStringLiteral("<p>Pierwszy</p><p>Burza</p>"));
    }
}

TEST_CASE("Notes: Find Next goes through the texts of the notes, shown selected",
          "[editor][footnotes][search]") {
    auto editor = editorWith(STORM);
    editor->showFind();
    auto* bar = editor->findChild<FindReplaceBar*>();
    REQUIRE(bar != nullptr);
    bar->setSearchText(QStringLiteral("burza"));
    REQUIRE(editor->searchEngine()->totalMatchCount() == 5);
    editor->setCursorPosition({0, 0});

    editor->findNext();
    CHECK(editor->selection().normalized().start == CursorPosition{0, 0});
    CHECK_FALSE(editor->noteSelection().has_value());

    // In the footnote: its number selected, the match shown selected in its text
    editor->findNext();
    CHECK(editor->selection().normalized().start == CursorPosition{0, 17});
    CHECK(editor->selection().normalized().end == CursorPosition{0, 18});
    REQUIRE(editor->noteSelection().has_value());
    CHECK(*editor->noteSelection() == NoteRange{QStringLiteral("f1"), 0, 5});
    CHECK(editor->searchEngine()->currentMatchIndex() == 1);
    editor->findNext();
    CHECK(*editor->noteSelection() == NoteRange{QStringLiteral("f1"), 26, 5});
    editor->findNext();
    CHECK(editor->selection().normalized().start == CursorPosition{0, 21});
    CHECK_FALSE(editor->noteSelection().has_value());
    editor->findPrevious();
    CHECK(*editor->noteSelection() == NoteRange{QStringLiteral("f1"), 26, 5});

    // The search text taken from the note's text, not from its number
    editor->showFind();
    CHECK(bar->searchText() == QStringLiteral("burza"));

    // Elsewhere: the note's text is no longer selected
    editor->clearSelection();
    editor->setCursorPosition({1, 0});
    CHECK_FALSE(editor->noteSelection().has_value());

    // Replace at the match Find Next went to: in the note
    editor->showFindReplace();
    editor->setCursorPosition({0, 0});
    editor->findNext();
    editor->findNext();
    REQUIRE(editor->noteSelection().has_value());
    editor->searchEngine()->setReplaceText(QStringLiteral("Wichura"));
    REQUIRE(editor->searchEngine()->replaceCurrent());
    CHECK(contentOf(*editor, QStringLiteral("f1")).startsWith(QStringLiteral("<b>Wichura</b>")));
}

TEST_CASE("Notes: the matches in a note's text are drawn in it",
          "[editor][footnotes][search][layout]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p>Tekst<footnote id=\"f1\">Ala ma kota i burzę.</footnote>.</p></kml>"));
    KalahariTextDocumentLayout* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    layout->layoutPendingBlocks();

    // The place of a match in the note's text, and the note's text under a point
    const std::optional<QRectF> word = layout->noteRangeRect({QStringLiteral("f1"), 14, 6});
    REQUIRE(word.has_value());
    CHECK(word->width() > 0.0);
    const std::optional<NoteRange> at = layout->notePositionAt(word->center());
    REQUIRE(at.has_value());
    CHECK(at->id == QStringLiteral("f1"));
    CHECK(at->start >= 14);
    CHECK(at->start <= 20);
    CHECK_FALSE(layout->noteRangeRect({QStringLiteral("none"), 0, 1}).has_value());

    // Drawn with the color of the current match under it, the selection over it
    editor->showFind();
    auto* bar = editor->findChild<FindReplaceBar*>();
    REQUIRE(bar != nullptr);
    bar->setSearchText(QStringLiteral("xyz"));
    const QRect note = editor->noteRect(QStringLiteral("f1")).toAlignedRect();
    REQUIRE_FALSE(note.isEmpty());
    const QImage plain = editor->grab(note).toImage();
    bar->setSearchText(QStringLiteral("burzę"));
    const QImage matched = editor->grab(note).toImage();
    CHECK(matched != plain);
    editor->setCursorPosition({0, 0});
    editor->findNext();
    REQUIRE(editor->noteSelection().has_value());
    const QImage current = editor->grab(note).toImage();
    CHECK(current != matched);
    CHECK(current != plain);

    // Gone elsewhere: the match without the selection
    editor->clearSelection();
    editor->setCursorPosition({0, 0});
    CHECK_FALSE(editor->noteSelection().has_value());
    CHECK(editor->grab(note).toImage() != current);
}

// =============================================================================
// Spelling and grammar
// =============================================================================

namespace {

/// A text with misspelled words in the texts of its notes: "wrnog" and "teh" in a footnote,
/// "Secnod" in the second paragraph of an endnote
const QString TYPOS = QStringLiteral(
    "<kml><p>The storm<footnote id=\"f1\">A wrnog word in <b>teh</b> note.</footnote> came."
    "</p><p>End<endnote id=\"e1\"><p>First</p><p>Secnod line</p></endnote> here.</p></kml>");

/// The issues of a text, in the order of the list: "kind paragraph:start text", in a note's
/// text "kind paragraph:number note:start text"
QStringList issuesOf(const BookEditor& editor) {
    QStringList list;
    for (const TextIssue& issue : editor.textIssues()) {
        const QString kind = issue.kind == TextIssueKind::Spelling ? QStringLiteral("spelling")
                                                                   : QStringLiteral("grammar");
        if (issue.inNote()) {
            list << QStringLiteral("%1 %2:%3 %4:%5 %6")
                        .arg(kind, QString::number(issue.paragraph),
                             QString::number(issue.numberOffset), issue.noteId,
                             QString::number(issue.start), issue.text);
        } else {
            list << QStringLiteral("%1 %2:%3 %4")
                        .arg(kind, QString::number(issue.paragraph), QString::number(issue.start),
                             issue.text);
        }
    }
    return list;
}

/// Run the event loop until the editor has checked the spelling and the grammar it has to
bool checked(const BookEditor& editor) {
    return waitUntil(
        [&editor]() { return !editor.isSpellCheckPending() && !editor.isGrammarCheckPending(); },
        DICTIONARY_WAIT_MS);
}

/// What a context menu offered, and the item chosen
struct MenuSeen {
    QStringList texts;  ///< Its items, without their access keys ("-": a separator)
    QList<bool> bold;   ///< Whether they are in bold
    QString chosen;     ///< The item chosen (none: the menu closed)
};

/// Open the context menu of the editor, from the keyboard at the cursor (no point) or with
/// the mouse at a point; choose its first item in bold (a word to put in place) if asked
MenuSeen contextMenu(BookEditor& editor, const std::optional<QPoint>& point,
                     bool chooseFirstBold) {
    MenuSeen seen;
    int ticks = 0;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&seen, &ticks, &poll, chooseFirstBold]() {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu == nullptr) {
            if (++ticks > 400) {
                poll.stop();
            }
            return;
        }
        poll.stop();
        QAction* chosen = nullptr;
        for (QAction* action : menu->actions()) {
            const QString text = kalahari::gui::withoutAccessKey(action->text());
            seen.texts << (action->isSeparator() ? QStringLiteral("-") : text);
            seen.bold << action->font().bold();
            if (chooseFirstBold && chosen == nullptr && action->font().bold()) {
                chosen = action;
                seen.chosen = text;
            }
        }
        menu->close();
        if (chosen != nullptr) {
            chosen->trigger();
        }
    });
    poll.start(5);
    if (point) {
        QContextMenuEvent event(QContextMenuEvent::Mouse, *point, editor.mapToGlobal(*point));
        QCoreApplication::sendEvent(&editor, &event);
    } else {
        QContextMenuEvent event(QContextMenuEvent::Keyboard, QPoint(), QPoint());
        QCoreApplication::sendEvent(&editor, &event);
    }
    return seen;
}

/// A point of the widget over a range of a note's text (none: not in view)
std::optional<QPoint> pointOverNoteText(const BookEditor& editor, const NoteRange& range) {
    const QRect note = editor.noteRect(range.id).toAlignedRect();
    for (int y = note.top(); y <= note.bottom(); y += 2) {
        for (int x = note.left(); x <= note.right(); x += 2) {
            const std::optional<NoteRange> place = editor.notePlaceAt(QPointF(x, y));
            if (place && place->id == range.id && place->start > range.start &&
                place->start < range.start + range.length) {
                return QPoint(x, y);
            }
        }
    }
    return std::nullopt;
}

}  // namespace

TEST_CASE("Notes: the misspelled words of the notes' texts, waved, listed and gone to",
          "[editor][footnotes][spelling]") {
    SpellCheckService spelling;
    REQUIRE(spelling.loadDictionary(QStringLiteral("en_US")));
    auto editor = editorWith(TYPOS);
    editor->show();
    const QRect note = editor->noteRect(QStringLiteral("f1")).toAlignedRect();
    REQUIRE_FALSE(note.isEmpty());
    const QImage plain = editor->grab(note).toImage();

    // Checked with the text, a paragraph of a note at a time; listed at the notes' numbers
    editor->setSpellCheckService(&spelling);
    REQUIRE(checked(*editor));
    const QStringList all{QStringLiteral("spelling 0:9 f1:2 wrnog"),
                          QStringLiteral("spelling 0:9 f1:16 teh"),
                          QStringLiteral("spelling 1:3 e1:6 Secnod")};
    CHECK(issuesOf(*editor) == all);
    CHECK(editor->checkProgress().spellingChecked == 2);
    CHECK(editor->grab(note).toImage() != plain);  // the waves under the words

    SECTION("Next Spelling or Grammar Issue: the notes' words, in the order of the text") {
        editor->setCursorPosition({0, 0});
        REQUIRE(editor->goToNextIssue() == BookEditor::IssueSearch::Found);
        CHECK(editor->noteSelection() == NoteRange{QStringLiteral("f1"), 2, 5});
        CHECK(editor->selection().normalized().start == CursorPosition{0, 9});
        REQUIRE(editor->goToNextIssue() == BookEditor::IssueSearch::Found);
        CHECK(editor->noteSelection() == NoteRange{QStringLiteral("f1"), 16, 3});
        REQUIRE(editor->goToNextIssue() == BookEditor::IssueSearch::Found);
        CHECK(editor->noteSelection() == NoteRange{QStringLiteral("e1"), 6, 6});
        REQUIRE(editor->goToNextIssue() == BookEditor::IssueSearch::Found);  // round the text
        CHECK(editor->noteSelection() == NoteRange{QStringLiteral("f1"), 2, 5});

        // The cursor right after a note's number: its text is behind it
        editor->clearSelection();
        editor->setCursorPosition({0, 10});
        REQUIRE(editor->goToNextIssue() == BookEditor::IssueSearch::Found);
        CHECK(editor->noteSelection() == NoteRange{QStringLiteral("e1"), 6, 6});
    }

    SECTION("A note's new text is checked again, and its words before it after Undo") {
        REQUIRE(editor->setNoteContent(QStringLiteral("f1"), QStringLiteral("All right.")));
        REQUIRE(checked(*editor));
        CHECK(issuesOf(*editor) == QStringList{all.at(2)});
        editor->undo();
        REQUIRE(checked(*editor));
        CHECK(issuesOf(*editor) == all);
    }

    SECTION("From the list: selected, and put right in the note") {
        const std::vector<TextIssue> issues = editor->textIssues();
        REQUIRE(issues.size() == 3);
        REQUIRE(editor->selectIssue(issues[1]));
        CHECK(editor->noteSelection() == NoteRange{QStringLiteral("f1"), 16, 3});
        REQUIRE(editor->replaceIssue(issues[1], QStringLiteral("the")));
        CHECK(contentOf(*editor, QStringLiteral("f1")) ==
              QStringLiteral("A wrnog word in <b>the</b> note."));
        CHECK_FALSE(editor->selectIssue(issues[1]));  // the word is no longer there
        REQUIRE(checked(*editor));
        CHECK(issuesOf(*editor) == QStringList({all.at(0), all.at(2)}));
        editor->undo();
        CHECK(contentOf(*editor, QStringLiteral("f1")) ==
              QStringLiteral("A wrnog word in <b>teh</b> note."));

        // Not in a read-only text
        editor->setReadOnly(true);
        CHECK_FALSE(editor->replaceIssue(issues[0], QStringLiteral("wrong")));
        CHECK(contentOf(*editor, QStringLiteral("f1")).startsWith(QStringLiteral("A wrnog")));
    }

    SECTION("The context menu at a note's word: the words for it, put in the note") {
        // From the keyboard, at the word gone to
        editor->setCursorPosition({0, 0});
        REQUIRE(editor->goToNextIssue() == BookEditor::IssueSearch::Found);
        MenuSeen seen = contextMenu(*editor, std::nullopt, true);
        REQUIRE_FALSE(seen.chosen.isEmpty());
        CHECK(seen.texts.contains(QStringLiteral("Ignore All")));
        CHECK(seen.texts.contains(QStringLiteral("Add to Dictionary")));
        CHECK(seen.texts.contains(QStringLiteral("Edit Footnote")));  // and the note's items
        CHECK(contentOf(*editor, QStringLiteral("f1")) ==
              QStringLiteral("A %1 word in <b>teh</b> note.").arg(seen.chosen));

        // With the mouse, on a word of an endnote's second paragraph
        const std::optional<QPoint> word =
            pointOverNoteText(*editor, {QStringLiteral("e1"), 6, 6});
        REQUIRE(word.has_value());
        seen = contextMenu(*editor, word, true);
        REQUIRE_FALSE(seen.chosen.isEmpty());
        CHECK(seen.texts.contains(QStringLiteral("Edit Endnote")));
        CHECK(contentOf(*editor, QStringLiteral("e1")) ==
              QStringLiteral("<p>First</p><p>%1 line</p>").arg(seen.chosen));

        // Elsewhere in a note's text: the note's menu alone
        const std::optional<QPoint> right =
            pointOverNoteText(*editor, {QStringLiteral("e1"), 0, 5});
        REQUIRE(right.has_value());
        seen = contextMenu(*editor, right, false);
        CHECK_FALSE(seen.texts.contains(QStringLiteral("Ignore All")));
        CHECK(seen.texts.contains(QStringLiteral("Edit Endnote")));
    }

    SECTION("Without the checking, no waves") {
        editor->setSpellCheckService(nullptr);
        CHECK(editor->textIssues().empty());
        CHECK(editor->grab(note).toImage() == plain);
    }
}

TEST_CASE("Notes: the grammar of the notes' texts, a paragraph at a time",
          "[editor][footnotes][grammar]") {
    FakeLanguageTool server;
    server.addIssue(FakeIssue(QStringLiteral("very unique"), QStringLiteral("UNIQUE"),
                              QStringLiteral("Unique cannot be more or less."),
                              {QStringLiteral("unique")}));
    auto grammar = std::make_unique<GrammarCheckService>();
    grammar->setServer(server.url());
    auto editor = editorWith(QStringLiteral(
        "<kml><p>A storm<footnote id=\"f1\">It was very unique.</footnote> came.</p>"
        "<p>End<endnote id=\"e1\"><p>First</p><p>So very unique.</p></endnote>.</p></kml>"));
    editor->setGrammarCheckService(grammar.get());
    editor->show();
    REQUIRE(checked(*editor));

    // Each paragraph of a note on its own, as those of the text
    const QStringList sent = server.texts();
    CHECK(sent.contains(QStringLiteral("It was very unique.")));
    CHECK(sent.contains(QStringLiteral("First")));
    CHECK(sent.contains(QStringLiteral("So very unique.")));
    CHECK(std::none_of(sent.cbegin(), sent.cend(),
                       [](const QString& text) { return text.contains(QLatin1Char('\n')); }));
    const QStringList all{QStringLiteral("grammar 0:7 f1:7 very unique"),
                          QStringLiteral("grammar 1:3 e1:9 very unique")};
    CHECK(issuesOf(*editor) == all);
    CHECK(editor->checkProgress().grammarChecked == 2);

    SECTION("The context menu at the issue gone to: what is wrong, put right in the note") {
        editor->setCursorPosition({0, 0});
        REQUIRE(editor->goToNextIssue() == BookEditor::IssueSearch::Found);
        CHECK(editor->noteSelection() == NoteRange{QStringLiteral("f1"), 7, 11});
        const MenuSeen seen = contextMenu(*editor, std::nullopt, true);
        REQUIRE(seen.texts.size() > 4);
        CHECK(seen.texts.mid(0, 4) == QStringList({"Unique cannot be more or less.", "unique", "-",
                                                   "Ignore This Rule"}));
        CHECK(seen.chosen == QStringLiteral("unique"));
        CHECK(contentOf(*editor, QStringLiteral("f1")) == QStringLiteral("It was unique."));
        REQUIRE(checked(*editor));
        CHECK(issuesOf(*editor) == QStringList{all.at(1)});
    }

    SECTION("A rule ignored: its issues leave the notes' texts too") {
        grammar->ignoreRule(QStringLiteral("UNIQUE"));
        CHECK(issuesOf(*editor).isEmpty());
    }

    SECTION("A paragraph of a note changed: it is sent again, the note's other one not") {
        REQUIRE(editor->setNoteContent(QStringLiteral("e1"),
                                       QStringLiteral("<p>First</p><p>Very good.</p>")));
        REQUIRE(checked(*editor));
        CHECK(server.texts().count(QStringLiteral("Very good.")) == 1);
        CHECK(server.texts().count(QStringLiteral("First")) == 1);
        CHECK(server.texts().count(QStringLiteral("It was very unique.")) == 1);
        CHECK(issuesOf(*editor) == QStringList{all.at(0)});
    }
}
