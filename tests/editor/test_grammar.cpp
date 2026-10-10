/// @file test_grammar.cpp
/// @brief Grammar as you type in the editor: waves under the issues the writer's LanguageTool
///        server (a stand-in on localhost) finds in the paragraphs around the view, sent when
///        the writer stops typing, kept through the edits that do not touch them

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/grammar_check_service.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/text_source_adapter.h>
#include "editor_test_utils.h"
#include "fake_language_tool.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QTextBlock>
#include <QTimer>

#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// The texts with a grammar wave in a paragraph, in their order
QStringList grammarWaves(const BookEditor& editor, int paragraph) {
    const QString text = editor.textDocument()->findBlockByNumber(paragraph).text();
    QStringList waves;
    for (const TextHighlight& highlight :
         QTextDocumentSource(editor.textDocument()).paragraphHighlights(paragraph)) {
        if (highlight.kind == HighlightKind::Grammar) {
            waves.append(text.mid(highlight.start, highlight.length));
        }
    }
    return waves;
}

/// Where the grammar waves of a paragraph start
QList<int> grammarWaveStarts(const BookEditor& editor, int paragraph) {
    QList<int> starts;
    for (const TextHighlight& highlight :
         QTextDocumentSource(editor.textDocument()).paragraphHighlights(paragraph)) {
        if (highlight.kind == HighlightKind::Grammar) {
            starts.append(highlight.start);
        }
    }
    return starts;
}

/// An editor with the paragraphs, checked with @p grammar, shown like a small window
std::unique_ptr<BookEditor> shownEditor(const QStringList& paragraphs,
                                        GrammarCheckService* grammar) {
    auto editor = std::make_unique<BookEditor>();
    editor->resize(600, 400);
    editor->fromKml(kmlOf(paragraphs));
    editor->setGrammarCheckService(grammar);
    editor->show();
    return editor;
}

/// The service checking on the server
std::unique_ptr<GrammarCheckService> serviceOn(const FakeLanguageTool& server) {
    auto service = std::make_unique<GrammarCheckService>();
    service->setServer(server.url());
    return service;
}

/// Run the event loop until the editor has checked what it has to
bool checked(const BookEditor& editor) {
    return waitUntil([&editor]() { return !editor.isGrammarCheckPending(); }, 20000);
}

/// Type text as the keyboard does, a key at a time
void type(BookEditor& editor, const QString& text) {
    for (const QChar c : text) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(c));
        QCoreApplication::sendEvent(&editor, &press);
    }
}

void pressKey(BookEditor& editor, int key) {
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &press);
}

/// What the context menu at the cursor offered
struct MenuSeen {
    QStringList texts;  ///< Its items ("-": a separator)
    QList<bool> bold;   ///< Whether they are in bold
};

/// Open the context menu at the cursor (as the menu key does) and choose an item of it
/// (none: the menu closes)
MenuSeen contextMenuAtCursor(BookEditor& editor, const QString& choice = QString()) {
    MenuSeen seen;
    int ticks = 0;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&seen, &ticks, &poll, &choice]() {
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
            seen.texts << (action->isSeparator() ? QStringLiteral("-") : action->text());
            seen.bold << action->font().bold();
            if (!choice.isEmpty() && action->text() == choice) {
                chosen = action;
            }
        }
        menu->close();
        if (chosen != nullptr) {
            chosen->trigger();
        }
    });
    poll.start(5);
    QContextMenuEvent event(QContextMenuEvent::Keyboard, QPoint(), QPoint());
    QCoreApplication::sendEvent(&editor, &event);
    return seen;
}

}  // namespace

TEST_CASE("Grammar: the issues of a shown editor get waves", "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A")});
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The kat sat."), QStringLiteral("All is right."),
                               QString(), QStringLiteral("Another kat and a kat.")},
                              grammar.get());
    REQUIRE(checked(*editor));
    CHECK(grammarWaves(*editor, 0) == QStringList{"kat"});
    CHECK(grammarWaves(*editor, 1).isEmpty());
    CHECK(grammarWaves(*editor, 3) == QStringList({"kat", "kat"}));

    // An empty paragraph is not sent
    CHECK(server.requests() == 3);

    SECTION("a document loaded later is checked too") {
        editor->fromKml(kmlOf({QStringLiteral("A new kat.")}));
        REQUIRE(checked(*editor));
        CHECK(grammarWaves(*editor, 0) == QStringList{"kat"});
    }
}

TEST_CASE("Grammar: a hidden editor is checked when it is shown", "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A")});
    auto grammar = serviceOn(server);
    auto editor = std::make_unique<BookEditor>();
    editor->resize(600, 400);
    editor->setGrammarCheckService(grammar.get());
    editor->fromKml(kmlOf({QStringLiteral("The kat sat.")}));
    runEventLoop(100);
    CHECK(server.requests() == 0);
    CHECK_FALSE(editor->isGrammarCheckPending());

    editor->show();
    REQUIRE(checked(*editor));
    CHECK(grammarWaves(*editor, 0) == QStringList{"kat"});
}

TEST_CASE("Grammar: nothing is sent while the writer types", "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A")});
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The end.")}, grammar.get());
    REQUIRE(checked(*editor));
    REQUIRE(server.requests() == 1);
    editor->setCursorPosition({0, 8});

    // A key now and then: the paragraph waits until the typing stops
    for (const QChar c : QStringLiteral(" A kat.")) {
        type(*editor, QString(c));
        runEventLoop(100);
    }
    CHECK(server.requests() == 1);
    REQUIRE(checked(*editor));
    CHECK(server.requests() == 2);
    CHECK(server.texts().last() == QStringLiteral("The end. A kat."));
    CHECK(grammarWaves(*editor, 0) == QStringList{"kat"});
}

TEST_CASE("Grammar: the waves of the issues an edit does not touch stay",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A")});
    server.addIssue({QStringLiteral("dogz"), QStringLiteral("RULE_B")});
    // "kat" from 4, "dogz" from 16
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("One kat and one dogz here.")}, grammar.get());
    REQUIRE(checked(*editor));
    REQUIRE(grammarWaves(*editor, 0) == QStringList({"kat", "dogz"}));

    // Each time at once, and after the paragraph is checked again
    SECTION("text put in before them") {
        editor->setCursorPosition({0, 0});
        editor->insertText(QStringLiteral("Now "));
        CHECK(grammarWaves(*editor, 0) == QStringList({"kat", "dogz"}));
        CHECK(grammarWaveStarts(*editor, 0) == QList<int>({8, 20}));
        REQUIRE(checked(*editor));
        CHECK(grammarWaves(*editor, 0) == QStringList({"kat", "dogz"}));
    }

    SECTION("a new paragraph between them") {
        editor->setCursorPosition({0, 12});
        pressKey(*editor, Qt::Key_Return);
        CHECK(grammarWaves(*editor, 0) == QStringList{"kat"});
        CHECK(grammarWaves(*editor, 1) == QStringList{"dogz"});
        REQUIRE(checked(*editor));
        CHECK(grammarWaves(*editor, 0) == QStringList{"kat"});
        CHECK(grammarWaves(*editor, 1) == QStringList{"dogz"});
    }

    SECTION("an edit in an issue takes its wave off") {
        editor->setCursorPosition({0, 5});
        editor->insertText(QStringLiteral("x"));
        CHECK(grammarWaves(*editor, 0) == QStringList{"dogz"});
        REQUIRE(checked(*editor));
        CHECK(grammarWaves(*editor, 0) == QStringList{"dogz"});
    }
}

TEST_CASE("Grammar: an answer for a paragraph edited since it was sent does not apply",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A")});
    server.hold();
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The kat sat.")}, grammar.get());
    REQUIRE(waitUntil([&server]() { return server.heldAnswers() == 1; }, 5000));

    editor->setCursorPosition({0, 0});
    editor->insertText(QStringLiteral("Now "));
    server.release();
    REQUIRE(checked(*editor));
    CHECK(server.texts().last() == QStringLiteral("Now The kat sat."));
    CHECK(grammarWaveStarts(*editor, 0) == QList<int>{8});
}

TEST_CASE("Grammar: an issue of a sentence not finished waits for its end",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A"), QString(), QStringList(),
                     QStringLiteral("GRAMMAR"), true});
    auto grammar = serviceOn(server);
    auto editor =
        shownEditor({QStringLiteral("The kat sat"), QStringLiteral("The kat sat.")},
                    grammar.get());
    REQUIRE(checked(*editor));
    CHECK(grammarWaves(*editor, 0).isEmpty());
    CHECK(grammarWaves(*editor, 1) == QStringList{"kat"});

    editor->setCursorPosition({0, 11});
    editor->insertText(QStringLiteral("!"));
    REQUIRE(checked(*editor));
    CHECK(grammarWaves(*editor, 0) == QStringList{"kat"});
}

TEST_CASE("Grammar: an ignored rule loses its waves in every editor", "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A")});
    server.addIssue({QStringLiteral("dogz"), QStringLiteral("RULE_B")});
    auto grammar = serviceOn(server);
    auto first = shownEditor({QStringLiteral("One kat here.")}, grammar.get());
    auto second = shownEditor({QStringLiteral("A kat and a dogz.")}, grammar.get());
    REQUIRE(checked(*first));
    REQUIRE(checked(*second));
    REQUIRE(grammarWaves(*second, 0) == QStringList({"kat", "dogz"}));

    grammar->ignoreRule(QStringLiteral("RULE_A"));
    CHECK(grammarWaves(*first, 0).isEmpty());
    CHECK(grammarWaves(*second, 0) == QStringList{"dogz"});

    // Not reported again
    first->setCursorPosition({0, 0});
    first->insertText(QStringLiteral("Yet "));
    REQUIRE(checked(*first));
    CHECK(grammarWaves(*first, 0).isEmpty());
}

TEST_CASE("Grammar: turned off, the waves go at once", "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A")});
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("One kat.")}, grammar.get());
    REQUIRE(checked(*editor));
    REQUIRE(grammarWaves(*editor, 0) == QStringList{"kat"});

    SECTION("the checking off and on") {
        grammar->setEnabled(false);
        CHECK(grammarWaves(*editor, 0).isEmpty());
        CHECK_FALSE(editor->isGrammarCheckPending());
        grammar->setEnabled(true);
        REQUIRE(checked(*editor));
        CHECK(grammarWaves(*editor, 0) == QStringList{"kat"});
    }

    SECTION("another language: checked again in it") {
        grammar->setLanguage(QStringLiteral("de_DE"));
        CHECK(grammarWaves(*editor, 0).isEmpty());
        REQUIRE(checked(*editor));
        CHECK(server.lastLanguage() == QStringLiteral("de-DE"));
        CHECK(grammarWaves(*editor, 0) == QStringList{"kat"});
    }

    SECTION("no server") {
        grammar->setServer(QString());
        CHECK(grammarWaves(*editor, 0).isEmpty());
        CHECK_FALSE(editor->isGrammarCheckPending());
    }

    SECTION("the editor without the service") {
        editor->setGrammarCheckService(nullptr);
        CHECK(grammarWaves(*editor, 0).isEmpty());
        CHECK(editor->grammarCheckService() == nullptr);
    }

    SECTION("the service gone") {
        grammar.reset();
        CHECK(grammarWaves(*editor, 0).isEmpty());
        CHECK(editor->grammarCheckService() == nullptr);
        editor->setCursorPosition({0, 0});
        editor->insertText(QStringLiteral("Still "));
        runEventLoop(50);
        CHECK(grammarWaves(*editor, 0).isEmpty());
    }
}

TEST_CASE("Grammar: the paragraphs are sent again when the server checks again",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A")});
    server.setFailure(500);
    auto grammar = serviceOn(server);
    grammar->setRetryDelay(200);
    int told = 0;
    QObject::connect(grammar.get(), &GrammarCheckService::serverError, grammar.get(),
                     [&told]() { ++told; });
    auto editor = shownEditor({QStringLiteral("The kat sat.")}, grammar.get());
    REQUIRE(waitUntil([&told]() { return told == 1; }, 5000));
    CHECK(grammarWaves(*editor, 0).isEmpty());

    server.setFailure(0);
    REQUIRE(waitUntil([&editor]() { return grammarWaves(*editor, 0) == QStringList{"kat"}; },
                      5000));
    CHECK(told == 1);
}

TEST_CASE("Grammar: the context menu of an issue", "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A"),
                     QStringLiteral("Did you mean a cat?"),
                     {QStringLiteral("cat"), QStringLiteral("hat")}});
    const QString longMessage = QStringLiteral("A long explanation ").repeated(10).trimmed();
    server.addIssue({QStringLiteral("dogz"), QStringLiteral("RULE_B"), longMessage});
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The kat and the dogz.")}, grammar.get());
    REQUIRE(checked(*editor));
    REQUIRE(grammarWaves(*editor, 0) == QStringList({"kat", "dogz"}));

    SECTION("what is wrong, the replacements in bold and Ignore This Rule on top") {
        editor->setCursorPosition({0, 5});
        const MenuSeen menu = contextMenuAtCursor(*editor);
        REQUIRE(menu.texts.size() > 5);
        CHECK(menu.texts.mid(0, 5) ==
              QStringList({"Did you mean a cat?", "cat", "hat", "-", "Ignore This Rule"}));
        CHECK(menu.bold.mid(0, 3) == QList<bool>({false, true, true}));
        CHECK(menu.texts.contains(QStringLiteral("Paste")));
    }

    SECTION("a replacement chosen") {
        editor->setCursorPosition({0, 5});
        contextMenuAtCursor(*editor, QStringLiteral("hat"));
        CHECK(editor->textDocument()->begin().text() == QStringLiteral("The hat and the dogz."));
        CHECK(grammarWaves(*editor, 0) == QStringList{"dogz"});
    }

    SECTION("Ignore This Rule chosen") {
        editor->setCursorPosition({0, 5});
        contextMenuAtCursor(*editor, QStringLiteral("Ignore This Rule"));
        CHECK(grammar->isRuleIgnored(QStringLiteral("RULE_A")));
        CHECK(grammarWaves(*editor, 0) == QStringList{"dogz"});
    }

    SECTION("a long message whole, in lines (a keyboard has no tool tip)") {
        editor->setCursorPosition({0, 17});
        const MenuSeen menu = contextMenuAtCursor(*editor);
        const QStringList lines = menu.texts.mid(0, menu.texts.indexOf(QStringLiteral("-")));
        CHECK(lines.size() > 1);
        CHECK(lines.join(QLatin1Char(' ')) == longMessage);
    }

    SECTION("a misspelled word in an issue: the spelling's menu") {
        SpellCheckService spelling;
        REQUIRE(spelling.loadDictionary(QStringLiteral("en_US")));
        editor->setSpellCheckService(&spelling);
        REQUIRE(waitUntil([&editor]() { return !editor->isSpellCheckPending(); }, 20000));
        editor->setCursorPosition({0, 17});
        const MenuSeen menu = contextMenuAtCursor(*editor);
        CHECK(menu.texts.contains(QStringLiteral("Add to Dictionary")));
        CHECK_FALSE(menu.texts.contains(QStringLiteral("Ignore This Rule")));
        editor->setSpellCheckService(nullptr);
    }
}

TEST_CASE("Grammar: Next Spelling or Grammar Issue goes to the grammar issues too",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A"),
                     QStringLiteral("Did you mean a cat?"), {QStringLiteral("cat")}});
    server.addIssue({QStringLiteral("dogz"), QStringLiteral("RULE_B"),
                     QStringLiteral("Too many dogs.")});
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The kat and the dogz."),
                               QStringLiteral("A plain line."), QStringLiteral("One more kat.")},
                              grammar.get());
    REQUIRE(checked(*editor));
    REQUIRE(grammarWaves(*editor, 2) == QStringList{"kat"});

    SECTION("in turn, round the text") {
        editor->setCursorPosition({0, 0});
        REQUIRE(editor->goToNextIssue());
        CHECK(editor->selectedText() == QStringLiteral("kat"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 7});
        REQUIRE(editor->goToNextIssue());
        CHECK(editor->selectedText() == QStringLiteral("dogz"));
        REQUIRE(editor->goToNextIssue());
        CHECK(editor->cursorPosition() == CursorPosition{2, 12});
        REQUIRE(editor->goToNextIssue());
        CHECK(editor->cursorPosition() == CursorPosition{0, 7});
    }

    SECTION("its menu: what is wrong and what to do") {
        editor->setCursorPosition({0, 8});
        REQUIRE(editor->goToNextIssue());
        CHECK(editor->selectedText() == QStringLiteral("dogz"));
        const MenuSeen menu = contextMenuAtCursor(*editor);
        CHECK(menu.texts.mid(0, 3) == QStringList({"Too many dogs.", "-", "Ignore This Rule"}));
    }

    SECTION("with the spelling: in the order of the text, at one place the misspelled word") {
        SpellCheckService spelling;
        REQUIRE(spelling.loadDictionary(QStringLiteral("en_US")));
        editor->setSpellCheckService(&spelling);
        editor->setCursorPosition({0, 0});
        REQUIRE(editor->goToNextIssue());
        CHECK(editor->selectedText() == QStringLiteral("kat"));  // a word of the dictionary
        CHECK(contextMenuAtCursor(*editor).texts.value(0) == QStringLiteral("Did you mean a cat?"));
        REQUIRE(editor->goToNextIssue());
        CHECK(editor->selectedText() == QStringLiteral("dogz"));
        const MenuSeen menu = contextMenuAtCursor(*editor);
        CHECK(menu.texts.contains(QStringLiteral("Add to Dictionary")));
        CHECK_FALSE(menu.texts.contains(QStringLiteral("Ignore This Rule")));

        // Its grammar issue is passed over with it
        REQUIRE(editor->goToNextIssue());
        CHECK(editor->cursorPosition() == CursorPosition{2, 12});
        editor->setSpellCheckService(nullptr);
    }

    SECTION("none in a text without issues") {
        editor->fromKml(kmlOf({QStringLiteral("All good here.")}));
        REQUIRE(checked(*editor));
        CHECK_FALSE(editor->goToNextIssue());
        CHECK_FALSE(editor->hasSelection());
    }

    SECTION("none while the checking is off") {
        grammar->setEnabled(false);
        CHECK_FALSE(editor->goToNextIssue());
        CHECK_FALSE(editor->hasSelection());
    }
}

TEST_CASE("Grammar: an issue in a longer one is gone to as well, with its own menu",
          "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat and the dogz"), QStringLiteral("RULE_C"),
                     QStringLiteral("A long one.")});
    server.addIssue({QStringLiteral("dogz"), QStringLiteral("RULE_B"),
                     QStringLiteral("A short one.")});
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The kat and the dogz.")}, grammar.get());
    REQUIRE(checked(*editor));
    REQUIRE(grammarWaves(*editor, 0) == QStringList({"kat and the dogz", "dogz"}));

    editor->setCursorPosition({0, 0});
    REQUIRE(editor->goToNextIssue());
    CHECK(editor->selectedText() == QStringLiteral("kat and the dogz"));
    CHECK(contextMenuAtCursor(*editor).texts.value(0) == QStringLiteral("A long one."));

    // The cursor is at the end of both: the menu is the selected one's
    REQUIRE(editor->goToNextIssue());
    CHECK(editor->selectedText() == QStringLiteral("dogz"));
    CHECK(contextMenuAtCursor(*editor).texts.value(0) == QStringLiteral("A short one."));

    REQUIRE(editor->goToNextIssue());
    CHECK(editor->selectedText() == QStringLiteral("kat and the dogz"));
}

TEST_CASE("Grammar: the paragraphs around the view are checked", "[editor][grammar]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("kat"), QStringLiteral("RULE_A")});
    auto grammar = serviceOn(server);
    QStringList paragraphs;
    for (int i = 0; i < 400; ++i) {
        paragraphs << QStringLiteral("Paragraph %1 has a kat in it.").arg(i);
    }
    auto editor = shownEditor(paragraphs, grammar.get());
    REQUIRE(checked(*editor));
    CHECK(grammarWaves(*editor, 0) == QStringList{"kat"});
    CHECK(server.requests() < 100);
    CHECK(grammarWaves(*editor, 300).isEmpty());

    // The view moved: the paragraphs in it now
    editor->setCursorPosition({300, 0});
    editor->ensureCursorVisible();
    REQUIRE(waitUntil([&editor]() { return grammarWaves(*editor, 300) == QStringList{"kat"}; },
                      10000));
    CHECK(grammarWaves(*editor, 200).isEmpty());
}
