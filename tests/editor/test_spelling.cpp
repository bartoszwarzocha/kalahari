/// @file test_spelling.cpp
/// @brief Spelling as you type in the editor: waves under the misspelled words, checked in
///        short turns from the paragraphs in view, kept through the edits that leave their
///        words as they were, none under the word being typed

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/text_source_adapter.h>
#include "editor_test_utils.h"
#include "../benchmarks/test_document_generator.h"

#include <QElapsedTimer>
#include <QKeyEvent>
#include <QTextBlock>
#include <QTimer>

#include <algorithm>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// A service checking with the shipped English dictionary
std::unique_ptr<SpellCheckService> englishService() {
    auto service = std::make_unique<SpellCheckService>();
    REQUIRE(service->loadDictionary(QStringLiteral("en_US")));
    return service;
}

/// The words with a spelling wave in a paragraph, in their order
QStringList wavyWords(const BookEditor& editor, int paragraph) {
    const QString text = editor.textDocument()->findBlockByNumber(paragraph).text();
    QStringList words;
    for (const TextHighlight& highlight :
         QTextDocumentSource(editor.textDocument()).paragraphHighlights(paragraph)) {
        if (highlight.kind == HighlightKind::Spelling) {
            words.append(text.mid(highlight.start, highlight.length));
        }
    }
    return words;
}

/// An editor with the paragraphs, checked with @p spelling, shown like a small window
std::unique_ptr<BookEditor> shownEditor(const QStringList& paragraphs,
                                        SpellCheckService* spelling) {
    auto editor = std::make_unique<BookEditor>();
    editor->resize(600, 400);
    editor->fromKml(kmlOf(paragraphs));
    editor->setSpellCheckService(spelling);
    editor->show();
    return editor;
}

/// Run the event loop until the editor has checked what it has to
bool checked(const BookEditor& editor) {
    return waitUntil([&editor]() { return !editor.isSpellCheckPending(); }, 20000);
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

}  // namespace

TEST_CASE("Spelling: the misspelled words of a shown editor get waves", "[editor][spelling]") {
    auto spelling = englishService();
    auto editor = shownEditor({QStringLiteral("The speling is bad."),
                               QStringLiteral("Another mistakke here, and NATO is fine.")},
                              spelling.get());
    REQUIRE(checked(*editor));
    CHECK(wavyWords(*editor, 0) == QStringList{"speling"});
    CHECK(wavyWords(*editor, 1) == QStringList{"mistakke"});

    SECTION("a document loaded later is checked too") {
        editor->fromKml(kmlOf({QStringLiteral("Fresh txet")}));
        REQUIRE(checked(*editor));
        CHECK(wavyWords(*editor, 0) == QStringList{"txet"});
    }
}

TEST_CASE("Spelling: a hidden editor is checked when it is shown", "[editor][spelling]") {
    auto spelling = englishService();
    auto editor = std::make_unique<BookEditor>();
    editor->resize(600, 400);
    editor->setSpellCheckService(spelling.get());
    editor->fromKml(kmlOf({QStringLiteral("One mistakke.")}));
    runEventLoop(50);
    CHECK(wavyWords(*editor, 0).isEmpty());
    CHECK_FALSE(editor->isSpellCheckPending());

    editor->show();
    REQUIRE(checked(*editor));
    CHECK(wavyWords(*editor, 0) == QStringList{"mistakke"});
}

TEST_CASE("Spelling: the word being typed gets its wave when the cursor leaves it",
          "[editor][spelling]") {
    auto spelling = englishService();
    auto editor = shownEditor({QStringLiteral("The end.")}, spelling.get());
    REQUIRE(checked(*editor));
    editor->setCursorPosition({0, 8});

    type(*editor, QStringLiteral(" mistakke"));
    REQUIRE(checked(*editor));
    CHECK(wavyWords(*editor, 0).isEmpty());

    // A space after it: the cursor left it
    type(*editor, QStringLiteral(" and"));
    REQUIRE(checked(*editor));
    CHECK(wavyWords(*editor, 0) == QStringList{"mistakke"});

    // The cursor moved away from the word being typed
    type(*editor, QStringLiteral("zz"));
    REQUIRE(checked(*editor));
    CHECK(wavyWords(*editor, 0) == QStringList{"mistakke"});
    pressKey(*editor, Qt::Key_Home);
    REQUIRE(checked(*editor));
    CHECK(wavyWords(*editor, 0) == QStringList({"mistakke", "andzz"}));
}

TEST_CASE("Spelling: the waves of the words an edit leaves as they were stay",
          "[editor][spelling]") {
    auto spelling = englishService();
    // "mistakke" from 4, "anothr" from 17
    auto editor = shownEditor({QStringLiteral("One mistakke and anothr one.")}, spelling.get());
    REQUIRE(checked(*editor));
    REQUIRE(wavyWords(*editor, 0) == QStringList({"mistakke", "anothr"}));

    // Each time at once, before the paragraphs are checked again, and after it
    SECTION("text put in before them") {
        editor->setCursorPosition({0, 0});
        editor->insertText(QStringLiteral("Now "));
        CHECK(wavyWords(*editor, 0) == QStringList({"mistakke", "anothr"}));
        REQUIRE(checked(*editor));
        CHECK(wavyWords(*editor, 0) == QStringList({"mistakke", "anothr"}));
    }

    SECTION("text put in between them") {
        editor->setCursorPosition({0, 13});
        editor->insertText(QStringLiteral("big "));
        CHECK(wavyWords(*editor, 0) == QStringList({"mistakke", "anothr"}));
        REQUIRE(checked(*editor));
        CHECK(wavyWords(*editor, 0) == QStringList({"mistakke", "anothr"}));
    }

    SECTION("a new paragraph between them") {
        editor->setCursorPosition({0, 13});
        pressKey(*editor, Qt::Key_Return);
        CHECK(wavyWords(*editor, 0) == QStringList{"mistakke"});
        CHECK(wavyWords(*editor, 1) == QStringList{"anothr"});
        REQUIRE(checked(*editor));
        CHECK(wavyWords(*editor, 0) == QStringList{"mistakke"});
        CHECK(wavyWords(*editor, 1) == QStringList{"anothr"});
    }

    SECTION("a new paragraph before them") {
        editor->setCursorPosition({0, 0});
        pressKey(*editor, Qt::Key_Return);
        CHECK(wavyWords(*editor, 0).isEmpty());
        CHECK(wavyWords(*editor, 1) == QStringList({"mistakke", "anothr"}));
        REQUIRE(checked(*editor));
        CHECK(wavyWords(*editor, 1) == QStringList({"mistakke", "anothr"}));
    }

    SECTION("an edit in a misspelled word takes its wave off until it is checked") {
        editor->setCursorPosition({0, 8});
        editor->insertText(QStringLiteral("x"));
        CHECK(wavyWords(*editor, 0) == QStringList{"anothr"});
        REQUIRE(checked(*editor));
        CHECK(wavyWords(*editor, 0) == QStringList({"mistxakke", "anothr"}));

        // A word made right loses its wave
        editor->undo();
        editor->setCursorPosition({0, 22});
        editor->insertText(QStringLiteral("e"));
        REQUIRE(checked(*editor));
        CHECK(wavyWords(*editor, 0) == QStringList{"mistakke"});
    }

    SECTION("joined to a letter, a word is another word") {
        editor->setCursorPosition({0, 23});
        editor->insertText(QStringLiteral("s"));
        CHECK(wavyWords(*editor, 0) == QStringList{"mistakke"});
        REQUIRE(checked(*editor));
        CHECK(wavyWords(*editor, 0) == QStringList({"mistakke", "anothrs"}));
    }
}

TEST_CASE("Spelling: the writer's own words lose their waves in every editor",
          "[editor][spelling]") {
    auto spelling = englishService();
    auto first = shownEditor({QStringLiteral("One mistakke here.")}, spelling.get());
    auto second = shownEditor({QStringLiteral("Another mistakke and anothr.")}, spelling.get());
    REQUIRE(checked(*first));
    REQUIRE(checked(*second));

    // Add to Dictionary
    spelling->addToUserDictionary(QStringLiteral("mistakke"));
    REQUIRE(checked(*first));
    REQUIRE(checked(*second));
    CHECK(wavyWords(*first, 0).isEmpty());
    CHECK(wavyWords(*second, 0) == QStringList{"anothr"});

    // Ignore All
    spelling->ignoreWord(QStringLiteral("anothr"));
    REQUIRE(checked(*second));
    CHECK(wavyWords(*second, 0).isEmpty());

    // Taken out of the dictionary again
    spelling->removeFromUserDictionary(QStringLiteral("mistakke"));
    REQUIRE(checked(*first));
    CHECK(wavyWords(*first, 0) == QStringList{"mistakke"});
}

TEST_CASE("Spelling: turned off, the waves go at once", "[editor][spelling]") {
    auto spelling = englishService();
    auto editor = shownEditor({QStringLiteral("One mistakke.")}, spelling.get());
    REQUIRE(checked(*editor));
    REQUIRE(wavyWords(*editor, 0) == QStringList{"mistakke"});

    SECTION("the checking off and on") {
        spelling->setEnabled(false);
        CHECK(wavyWords(*editor, 0).isEmpty());
        CHECK_FALSE(editor->isSpellCheckPending());
        spelling->setEnabled(true);
        REQUIRE(checked(*editor));
        CHECK(wavyWords(*editor, 0) == QStringList{"mistakke"});
    }

    SECTION("a language without a dictionary") {
        spelling->unloadDictionary();
        CHECK(wavyWords(*editor, 0).isEmpty());
    }

    SECTION("the editor without a dictionary") {
        editor->setSpellCheckService(nullptr);
        CHECK(wavyWords(*editor, 0).isEmpty());
        CHECK(editor->spellCheckService() == nullptr);
    }

    SECTION("the dictionary gone") {
        spelling.reset();
        CHECK(wavyWords(*editor, 0).isEmpty());
        CHECK(editor->spellCheckService() == nullptr);
        editor->setCursorPosition({0, 0});
        editor->insertText(QStringLiteral("Still "));
        runEventLoop(50);
        CHECK(wavyWords(*editor, 0).isEmpty());
    }
}

TEST_CASE("Spelling: the paragraphs in view are checked first", "[editor][spelling]") {
    auto spelling = englishService();
    QStringList paragraphs;
    for (int i = 0; i < 6000; ++i) {
        paragraphs << QStringLiteral("Paragraph %1 has one mistakke and nothing more to say.")
                          .arg(i);
    }
    auto editor = shownEditor(paragraphs, nullptr);
    editor->setCursorPosition({5000, 0});
    editor->ensureCursorVisible();
    runEventLoop(20);

    // Checked in short turns: the view first, then the text from its start
    editor->setSpellCheckService(spelling.get());
    REQUIRE(waitUntil([&editor]() { return !wavyWords(*editor, 5000).isEmpty(); }, 20000));
    CHECK(wavyWords(*editor, 4000).isEmpty());
    CHECK(editor->isSpellCheckPending());

    REQUIRE(checked(*editor));
    CHECK(wavyWords(*editor, 0) == QStringList{"mistakke"});
    CHECK(wavyWords(*editor, 4000) == QStringList{"mistakke"});
    CHECK(wavyWords(*editor, 5999) == QStringList{"mistakke"});
}

// =============================================================================
// Benchmark (hidden): run with "[benchmark][spelling]"
// =============================================================================

TEST_CASE("Spelling benchmark: a 150k-word chapter", "[.][benchmark][spelling]") {
    QStringList lines;

    QElapsedTimer clock;
    SpellCheckService spelling;
    clock.start();
    REQUIRE(spelling.loadDictionary(QStringLiteral("pl_PL")));
    lines << QStringLiteral("pl_PL loaded now: %1 ms").arg(clock.elapsed());
    clock.restart();
    REQUIRE(spelling.loadDictionary(QStringLiteral("en_US")));
    lines << QStringLiteral("en_US loaded now: %1 ms").arg(clock.elapsed());

    kalahari::benchmarks::TestDocumentGenerator::Config config;
    config.targetWordCount = 150000;
    kalahari::benchmarks::TestDocumentGenerator generator(config);
    QString kml = generator.generateKml();

    // A misspelled word now and then: every twentieth "and" (the generator's words are right)
    int found = 0;
    for (qsizetype at = kml.indexOf(QStringLiteral(" and ")); at >= 0;
         at = kml.indexOf(QStringLiteral(" and "), at + 5)) {
        if (++found % 20 == 0) {
            kml.replace(at, 5, QStringLiteral(" adn "));
        }
    }
    auto editor = std::make_unique<BookEditor>();
    editor->resize(1000, 800);
    editor->fromKml(kml);
    editor->show();
    runEventLoop(50);

    // The longest time the event loop waits, without the check and while the text is checked
    QElapsedTimer sinceTick;
    qint64 longestGap = 0;
    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [&sinceTick, &longestGap]() {
        longestGap = std::max(longestGap, sinceTick.restart());
    });
    ticker.start(1);
    sinceTick.start();
    runEventLoop(1000);
    lines << QStringLiteral("longest wait of the event loop without the check: %1 ms")
                 .arg(longestGap);

    longestGap = 0;
    sinceTick.restart();
    clock.restart();
    editor->setSpellCheckService(&spelling);
    REQUIRE(checked(*editor));
    const qint64 whole = clock.elapsed();
    ticker.stop();
    const int paragraphs = static_cast<int>(editor->paragraphCount());
    int waves = 0;
    for (int i = 0; i < paragraphs; ++i) {
        waves += static_cast<int>(wavyWords(*editor, i).size());
    }
    lines << QStringLiteral("whole chapter checked: %1 ms (%2 words, %3 paragraphs, %4 waves); "
                            "longest wait of the event loop %5 ms")
                 .arg(whole)
                 .arg(generator.lastWordCount())
                 .arg(paragraphs)
                 .arg(waves)
                 .arg(longestGap);

    // Typing: a key at a time, the spelling checked after it
    editor->setCursorPosition({paragraphs / 2, 0});
    editor->ensureCursorVisible();
    REQUIRE(checked(*editor));
    qint64 longestKey = 0;
    for (const QChar c : QStringLiteral("Some new words typd in the middle of the chapter. ")) {
        clock.restart();
        type(*editor, QString(c));
        longestKey = std::max(longestKey, clock.nsecsElapsed() / 1000);
        QCoreApplication::processEvents();
    }
    REQUIRE(checked(*editor));
    lines << QStringLiteral("longest key press while typing: %1 us").arg(longestKey);

    WARN(lines.join(QLatin1Char('\n')).toStdString());
}
