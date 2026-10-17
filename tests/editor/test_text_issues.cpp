/// @file test_text_issues.cpp
/// @brief The misspelled words and grammar issues of a text as one list (the Spelling and
///        Grammar panel): in the order of the text, told when they change, selected and put
///        right from the list, the grammar of the whole text checked when asked, the waves in
///        the theme's colors

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/theme.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/grammar_check_service.h>
#include <kalahari/editor/render_context.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/text_issue.h>
#include <kalahari/gui/panels/annotation_colors.h>
#include "editor_test_utils.h"
#include "fake_language_tool.h"

#include <QFile>
#include <QImage>
#include <QTextBlock>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// A service checking with the shipped English dictionary
std::unique_ptr<SpellCheckService> englishService() {
    auto service = std::make_unique<SpellCheckService>();
    REQUIRE(service->loadDictionary(QStringLiteral("en_US")));
    return service;
}

/// The grammar service checking on the server
std::unique_ptr<GrammarCheckService> serviceOn(const FakeLanguageTool& server) {
    auto service = std::make_unique<GrammarCheckService>();
    service->setServer(server.url());
    return service;
}

/// An editor with the paragraphs, checked with the services given (none: nullptr), shown like
/// a small window
std::unique_ptr<BookEditor> shownEditor(const QStringList& paragraphs, SpellCheckService* spelling,
                                        GrammarCheckService* grammar) {
    auto editor = std::make_unique<BookEditor>();
    editor->resize(600, 400);
    editor->fromKml(kmlOf(paragraphs));
    editor->setSpellCheckService(spelling);
    editor->setGrammarCheckService(grammar);
    editor->show();
    return editor;
}

/// Run the event loop until the editor has checked the spelling and the grammar it has to
bool checked(const BookEditor& editor) {
    return waitUntil(
        [&editor]() { return !editor.isSpellCheckPending() && !editor.isGrammarCheckPending(); },
        DICTIONARY_WAIT_MS);
}

/// The issues of the text, in the order of the list: "kind paragraph:start text"
QStringList issueList(const BookEditor& editor) {
    QStringList list;
    for (const TextIssue& issue : editor.textIssues()) {
        list << QStringLiteral("%1 %2:%3 %4")
                    .arg(issue.kind == TextIssueKind::Spelling ? QStringLiteral("spelling")
                                                               : QStringLiteral("grammar"))
                    .arg(issue.paragraph)
                    .arg(issue.start)
                    .arg(issue.text);
    }
    return list;
}

/// The paragraphs with a grammar issue, in their order
QList<int> grammarParagraphs(const BookEditor& editor) {
    QList<int> numbers;
    for (const TextIssue& issue : editor.textIssues()) {
        if (issue.kind == TextIssueKind::Grammar) {
            numbers << issue.paragraph;
        }
    }
    return numbers;
}

/// Image of the editor with one pixel per logical pixel (see test_editor_stage2.cpp)
QImage editorImage(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

/// Pixels of an area that differ between two images of the same text and are near a color:
/// the glyphs cancel out, the waves stay
int wavePixels(const QImage& waved, const QImage& plain, const QRect& area, const QColor& color) {
    int count = 0;
    const QRect clipped = area & waved.rect();
    for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
        for (int x = clipped.left(); x <= clipped.right(); ++x) {
            if (waved.pixel(x, y) == plain.pixel(x, y)) {
                continue;
            }
            const QColor pixel = QColor::fromRgb(waved.pixel(x, y));
            if (std::abs(pixel.red() - color.red()) + std::abs(pixel.green() - color.green()) +
                    std::abs(pixel.blue() - color.blue()) <
                90) {
                ++count;
            }
        }
    }
    return count;
}

/// Where the wave of a range of the first paragraph may be: under its text, from the top of
/// its line to a little under it
QRect waveArea(const BookEditor& editor, int start, int end) {
    const QRect first = editor.placeRect(start, false).toAlignedRect();
    const QRect last = editor.placeRect(end, true).toAlignedRect();
    return QRect(QPoint(first.right() + 1, first.top()),
                 QPoint(last.left() - 1, first.bottom() + 8));
}

}  // namespace

// =============================================================================
// The list
// =============================================================================

TEST_CASE("Issues: the misspelled words and the grammar issues of a text, in its order",
          "[editor][issues]") {
    auto spelling = englishService();
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("speling of"), QStringLiteral("ODD_PHRASE"),
                     QStringLiteral("An odd phrase."), {QStringLiteral("spelling of")}});
    server.addIssue({QStringLiteral("a apple"), QStringLiteral("EN_A_VS_AN"),
                     QStringLiteral("Use 'an' instead of 'a'."), {QStringLiteral("an apple")}});
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The speling of a apple."),
                               QStringLiteral("All good here."), QString(),
                               QStringLiteral("Another mistakke and a apple.")},
                              spelling.get(), grammar.get());
    REQUIRE(checked(*editor));

    // At one place, the misspelled word before the grammar issue, as Next Spelling or Grammar
    // Issue goes
    CHECK(issueList(*editor) == QStringList{"spelling 0:4 speling", "grammar 0:4 speling of",
                                            "grammar 0:15 a apple", "spelling 3:8 mistakke",
                                            "grammar 3:21 a apple"});

    // What the grammar issues are; a misspelled word's suggestions come from the dictionary
    const std::vector<TextIssue> issues = editor->textIssues();
    REQUIRE(issues.size() == 5);
    CHECK(issues[2].length == 7);
    CHECK(issues[2].message == QStringLiteral("Use 'an' instead of 'a'."));
    CHECK(issues[2].ruleId == QStringLiteral("EN_A_VS_AN"));
    CHECK(issues[2].suggestions == QStringList{QStringLiteral("an apple")});
    CHECK(issues[0].length == 7);
    CHECK(issues[0].message.isEmpty());
    CHECK(issues[0].suggestions.isEmpty());

    SECTION("an edit before them moves them with the text at once") {
        editor->setCursorPosition({3, 0});
        editor->insertText(QStringLiteral("Yes. "));
        const QStringList moved{"spelling 0:4 speling", "grammar 0:4 speling of",
                                "grammar 0:15 a apple", "spelling 3:13 mistakke",
                                "grammar 3:26 a apple"};
        CHECK(issueList(*editor) == moved);
        REQUIRE(checked(*editor));
        CHECK(issueList(*editor) == moved);
    }

    SECTION("an edit in one takes it off the list until its paragraph is checked again") {
        editor->setCursorPosition({3, 12});
        editor->insertText(QStringLiteral("x"));
        CHECK(issueList(*editor) == QStringList{"spelling 0:4 speling", "grammar 0:4 speling of",
                                                "grammar 0:15 a apple", "grammar 3:22 a apple"});
        REQUIRE(checked(*editor));
        CHECK(issueList(*editor) == QStringList{"spelling 0:4 speling", "grammar 0:4 speling of",
                                                "grammar 0:15 a apple", "spelling 3:8 mistxakke",
                                                "grammar 3:22 a apple"});
    }

    SECTION("none of a check turned off") {
        spelling->setEnabled(false);
        CHECK(issueList(*editor) == QStringList{"grammar 0:4 speling of", "grammar 0:15 a apple",
                                                "grammar 3:21 a apple"});
        grammar->setEnabled(false);
        CHECK(editor->textIssues().empty());
    }
}

TEST_CASE("Issues: the editor tells when they change, once for the changes made together",
          "[editor][issues]") {
    auto spelling = englishService();
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("a apple"), QStringLiteral("EN_A_VS_AN")});
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The speling of a apple."),
                               QStringLiteral("Another mistakke.")},
                              spelling.get(), grammar.get());
    int told = 0;
    QObject::connect(editor.get(), &BookEditor::issuesChanged, [&told]() { ++told; });
    REQUIRE(checked(*editor));
    runEventLoop(20);  // what the last turn of the checks tells
    CHECK(told > 0);   // the issues found
    REQUIRE(issueList(*editor) == QStringList{"spelling 0:4 speling", "grammar 0:15 a apple",
                                              "spelling 1:8 mistakke"});
    told = 0;

    SECTION("edits: once, and again when their paragraph is checked") {
        editor->setCursorPosition({1, 0});
        editor->insertText(QStringLiteral("So "));
        editor->insertText(QStringLiteral("yes, "));
        CHECK(told == 0);  // told after the changes made together
        REQUIRE(waitUntil([&told]() { return told > 0; }));
        CHECK(told == 1);
        REQUIRE(checked(*editor));
        CHECK(told > 1);
        CHECK(issueList(*editor) == QStringList{"spelling 0:4 speling", "grammar 0:15 a apple",
                                                "spelling 1:16 mistakke"});
    }

    SECTION("a word and a rule ignored") {
        spelling->ignoreWord(QStringLiteral("speling"));
        REQUIRE(checked(*editor));
        CHECK(told > 0);
        CHECK(issueList(*editor) ==
              QStringList{"grammar 0:15 a apple", "spelling 1:8 mistakke"});

        told = 0;
        grammar->ignoreRule(QStringLiteral("EN_A_VS_AN"));
        REQUIRE(waitUntil([&told]() { return told > 0; }));
        CHECK(issueList(*editor) == QStringList{"spelling 1:8 mistakke"});
    }

    SECTION("another text") {
        editor->fromKml(kmlOf({QStringLiteral("All good here.")}));
        CHECK(editor->textIssues().empty());
        REQUIRE(waitUntil([&told]() { return told > 0; }));
    }

    SECTION("not when nothing changed") {
        editor->setCursorPosition({0, 3});
        runEventLoop(50);
        CHECK(told == 0);
    }
}

TEST_CASE("Issues: the editor tells how much of the text is checked", "[editor][issues]") {
    auto spelling = englishService();
    FakeLanguageTool server;
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The first line."), QString(), QStringLiteral("   "),
                               QStringLiteral("The second line."), QStringLiteral("The third.")},
                              nullptr, nullptr);

    // The blank paragraphs have nothing to check
    BookEditor::CheckProgress progress = editor->checkProgress();
    CHECK(progress.paragraphs == 3);
    CHECK(progress.spellingChecked == 0);
    CHECK(progress.grammarChecked == 0);

    // The spelling at once, the grammar when the server answers
    server.hold();
    editor->setSpellCheckService(spelling.get());
    editor->setGrammarCheckService(grammar.get());
    REQUIRE(waitUntil(
        [&editor, &server]() {
            return !editor->isSpellCheckPending() && server.heldAnswers() > 0;
        },
        DICTIONARY_WAIT_MS));
    progress = editor->checkProgress();
    CHECK(progress.paragraphs == 3);
    CHECK(progress.spellingChecked == 3);
    CHECK(progress.grammarChecked == 0);
    server.release();
    REQUIRE(checked(*editor));
    CHECK(editor->checkProgress().grammarChecked == 3);

    // An edited paragraph is checked again
    editor->setCursorPosition({3, 0});
    editor->insertText(QStringLiteral("And "));
    progress = editor->checkProgress();
    CHECK(progress.spellingChecked == 2);
    CHECK(progress.grammarChecked == 2);
    REQUIRE(checked(*editor));
    progress = editor->checkProgress();
    CHECK(progress.spellingChecked == 3);
    CHECK(progress.grammarChecked == 3);
}

// =============================================================================
// The grammar of the whole text
// =============================================================================

TEST_CASE("Issues: the grammar of the whole text is checked when asked", "[editor][issues]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("a apple"), QStringLiteral("EN_A_VS_AN")});
    auto grammar = serviceOn(server);
    constexpr int PARAGRAPHS = 120;
    QStringList paragraphs;
    for (int i = 0; i < PARAGRAPHS; ++i) {
        paragraphs << QStringLiteral("Paragraph %1 has a apple.").arg(i + 1);
    }
    auto editor = shownEditor(paragraphs, nullptr, grammar.get());
    REQUIRE(checked(*editor));

    // The paragraphs in view and around them only
    CHECK_FALSE(editor->isWholeTextGrammar());
    const int aroundView = editor->checkProgress().grammarChecked;
    CHECK(aroundView > 0);
    CHECK(aroundView < PARAGRAPHS);
    CHECK(grammarParagraphs(*editor).size() == aroundView);
    CHECK(server.requests() == aroundView);

    SECTION("on: the rest of the text too, each paragraph once") {
        editor->setWholeTextGrammar(true);
        CHECK(editor->isWholeTextGrammar());
        REQUIRE(checked(*editor));
        CHECK(editor->checkProgress().grammarChecked == PARAGRAPHS);
        CHECK(grammarParagraphs(*editor).size() == PARAGRAPHS);
        CHECK(server.requests() == PARAGRAPHS);
    }

    SECTION("on, in the middle of the text: the paragraphs before the view too") {
        editor->setCursorPosition({100, 0});
        editor->ensureCursorVisible();
        editor->setWholeTextGrammar(true);
        REQUIRE(checked(*editor));
        CHECK(editor->checkProgress().grammarChecked == PARAGRAPHS);
        CHECK(grammarParagraphs(*editor).front() == 0);
        CHECK(grammarParagraphs(*editor).size() == PARAGRAPHS);
    }

    SECTION("off again: the paragraphs far from the view are not sent again; their issues "
            "stay") {
        editor->setWholeTextGrammar(true);
        REQUIRE(checked(*editor));
        editor->setWholeTextGrammar(false);
        editor->requestGrammarCheck();
        REQUIRE(checked(*editor));
        CHECK(server.requests() == PARAGRAPHS + aroundView);
        CHECK(editor->checkProgress().grammarChecked == aroundView);
        CHECK(grammarParagraphs(*editor).size() == PARAGRAPHS);
    }
}

// =============================================================================
// Selected and put right
// =============================================================================

TEST_CASE("Issues: an issue of the list is selected in the text", "[editor][issues]") {
    auto spelling = englishService();
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("a apple"), QStringLiteral("EN_A_VS_AN")});
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The speling of a apple."),
                               QStringLiteral("All good here."),
                               QStringLiteral("Another mistakke and a apple.")},
                              spelling.get(), grammar.get());
    REQUIRE(checked(*editor));
    const std::vector<TextIssue> issues = editor->textIssues();
    REQUIRE(issueList(*editor) == QStringList{"spelling 0:4 speling", "grammar 0:15 a apple",
                                              "spelling 2:8 mistakke", "grammar 2:21 a apple"});

    SECTION("with the cursor at its end") {
        REQUIRE(editor->selectIssue(issues[3]));
        CHECK(editor->selectedText() == QStringLiteral("a apple"));
        CHECK(editor->cursorPosition() == CursorPosition{2, 28});
        REQUIRE(editor->selectIssue(issues[0]));
        CHECK(editor->selectedText() == QStringLiteral("speling"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 11});
    }

    SECTION("not when the text there is not the issue's any more") {
        editor->setCursorPosition({0, 7});
        editor->insertText(QStringLiteral("l"));  // "spelling"
        editor->setCursorPosition({1, 0});
        CHECK_FALSE(editor->selectIssue(issues[0]));
        CHECK_FALSE(editor->selectIssue(issues[1]));  // moved: the list has it elsewhere now
        CHECK_FALSE(editor->hasSelection());
        CHECK(editor->cursorPosition() == CursorPosition{1, 0});

        // As the list has it now
        const std::vector<TextIssue> now = editor->textIssues();
        REQUIRE(issueList(*editor).front() == QStringLiteral("grammar 0:16 a apple"));
        CHECK(editor->selectIssue(now.front()));
        CHECK(editor->selectedText() == QStringLiteral("a apple"));
    }

    SECTION("not one outside the text") {
        TextIssue outside = issues[0];
        outside.paragraph = 7;
        CHECK_FALSE(editor->selectIssue(outside));
        outside = issues[0];
        outside.start = 20;
        CHECK_FALSE(editor->selectIssue(outside));
        outside = issues[0];
        outside.length = 0;
        outside.text.clear();
        CHECK_FALSE(editor->selectIssue(outside));
        CHECK_FALSE(editor->hasSelection());
    }
}

TEST_CASE("Issues: an issue far from the view is brought into it when selected",
          "[editor][issues]") {
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("a apple"), QStringLiteral("EN_A_VS_AN")});
    auto grammar = serviceOn(server);
    QStringList paragraphs;
    for (int i = 0; i < 100; ++i) {
        paragraphs << (i == 90 ? QStringLiteral("At last a apple.")
                               : QStringLiteral("A line without an issue."));
    }
    auto editor = shownEditor(paragraphs, nullptr, grammar.get());
    editor->setWholeTextGrammar(true);
    REQUIRE(checked(*editor));
    const std::vector<TextIssue> issues = editor->textIssues();
    REQUIRE(issues.size() == 1);
    REQUIRE(issues.front().paragraph == 90);

    REQUIRE(editor->scrollOffset() == 0.0);  // the view at the start of the text
    REQUIRE(editor->selectIssue(issues.front()));
    CHECK(editor->selectedText() == QStringLiteral("a apple"));
    CHECK(editor->scrollOffset() > 0.0);
    const QRect caret = editor->inputMethodQuery(Qt::ImCursorRectangle).toRect();
    CHECK(editor->rect().contains(caret.center()));
}

TEST_CASE("Issues: a suggestion is put in the place of an issue, as one undo step",
          "[editor][issues]") {
    auto spelling = englishService();
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("a apple"), QStringLiteral("EN_A_VS_AN"), QString(),
                     {QStringLiteral("an apple")}});
    auto grammar = serviceOn(server);
    auto editor = shownEditor({QStringLiteral("The speling of a apple.")}, spelling.get(),
                              grammar.get());
    REQUIRE(checked(*editor));
    const std::vector<TextIssue> issues = editor->textIssues();
    REQUIRE(issueList(*editor) == QStringList{"spelling 0:4 speling", "grammar 0:15 a apple"});

    SECTION("a grammar issue: its wave goes, the word before it keeps its own") {
        REQUIRE(editor->replaceIssue(issues[1], issues[1].suggestions.front()));
        CHECK(editor->plainText() == QStringLiteral("The speling of an apple."));
        CHECK(editor->cursorPosition() == CursorPosition{0, 23});
        CHECK_FALSE(editor->hasSelection());
        CHECK(issueList(*editor) == QStringList{"spelling 0:4 speling"});

        editor->undo();
        CHECK(editor->plainText() == QStringLiteral("The speling of a apple."));
        CHECK_FALSE(editor->canUndo());
    }

    SECTION("a misspelled word, with a word of the dictionary: the issue after it moves") {
        REQUIRE(spelling->suggestions(issues[0].text).contains(QStringLiteral("spelling")));
        REQUIRE(editor->replaceIssue(issues[0], QStringLiteral("spelling")));
        CHECK(editor->plainText() == QStringLiteral("The spelling of a apple."));
        CHECK(issueList(*editor) == QStringList{"grammar 0:16 a apple"});
        REQUIRE(checked(*editor));
        CHECK(issueList(*editor) == QStringList{"grammar 0:16 a apple"});
    }

    SECTION("not when the text there is not the issue's any more") {
        editor->setCursorPosition({0, 15});
        editor->insertText(QStringLiteral("x"));
        CHECK_FALSE(editor->replaceIssue(issues[1], QStringLiteral("an apple")));
        CHECK(editor->plainText() == QStringLiteral("The speling of xa apple."));
    }

    SECTION("not in a read-only text, which says so; the issue can still be selected") {
        editor->setReadOnly(true);
        int refused = 0;
        QObject::connect(editor.get(), &BookEditor::changeRefused, [&refused]() { ++refused; });
        CHECK_FALSE(editor->replaceIssue(issues[1], QStringLiteral("an apple")));
        CHECK(refused == 1);
        CHECK(editor->plainText() == QStringLiteral("The speling of a apple."));
        CHECK(editor->selectIssue(issues[1]));
        CHECK(editor->selectedText() == QStringLiteral("a apple"));
    }
}

// =============================================================================
// Colors
// =============================================================================

TEST_CASE("Issues: the waves have the colors of the editor's appearance, on each paper",
          "[editor][issues]") {
    auto spelling = englishService();
    FakeLanguageTool server;
    server.addIssue({QStringLiteral("a apple"), QStringLiteral("EN_A_VS_AN")});
    auto grammar = serviceOn(server);
    const QStringList text{QStringLiteral("The speling of a apple.")};
    auto waved = shownEditor(text, spelling.get(), grammar.get());
    auto plain = shownEditor(text, nullptr, nullptr);
    REQUIRE(checked(*waved));
    REQUIRE(issueList(*waved) == QStringList{"spelling 0:4 speling", "grammar 0:15 a apple"});
    const QRect spellingArea = waveArea(*waved, 4, 11);
    const QRect grammarArea = waveArea(*waved, 15, 22);
    REQUIRE_FALSE(spellingArea.intersects(grammarArea));

    // The images of the editors with the same appearance and the caret at the same place
    EditorAppearance appearance = waved->appearance();
    QImage wavedImage;
    QImage plainImage;
    const auto paint = [&]() {
        waved->setAppearance(appearance);
        plain->setAppearance(appearance);
        waved->setCursorPosition({0, 0});
        plain->setCursorPosition({0, 0});
        wavedImage = editorImage(*waved);
        plainImage = editorImage(*plain);
    };

    const RenderColors before;
    appearance.colors.issuesLight = {QColor(0, 160, 0), QColor(170, 0, 170)};
    appearance.colors.issuesDark = {QColor(0, 220, 220), QColor(230, 230, 0)};

    SECTION("on the dark paper") {
        appearance.colorMode = EditorColorMode::Dark;
        paint();
        CHECK(wavePixels(wavedImage, plainImage, spellingArea, QColor(0, 220, 220)) > 0);
        CHECK(wavePixels(wavedImage, plainImage, grammarArea, QColor(230, 230, 0)) > 0);
        CHECK(wavePixels(wavedImage, plainImage, spellingArea, QColor(230, 230, 0)) == 0);
        CHECK(wavePixels(wavedImage, plainImage, waved->rect(), before.spellError) == 0);
        CHECK(wavePixels(wavedImage, plainImage, waved->rect(), before.grammarWarning) == 0);
    }

    SECTION("on the light paper") {
        appearance.colorMode = EditorColorMode::Light;
        paint();
        CHECK(wavePixels(wavedImage, plainImage, spellingArea, QColor(0, 160, 0)) > 0);
        CHECK(wavePixels(wavedImage, plainImage, grammarArea, QColor(170, 0, 170)) > 0);
        CHECK(wavePixels(wavedImage, plainImage, waved->rect(), QColor(0, 220, 220)) == 0);
    }

    SECTION("none given: the editor's own colors") {
        appearance.colors.issuesLight = {};
        appearance.colorMode = EditorColorMode::Light;
        paint();
        CHECK(wavePixels(wavedImage, plainImage, spellingArea, before.spellError) > 0);
        CHECK(wavePixels(wavedImage, plainImage, grammarArea, before.grammarWarning) > 0);
    }
}

TEST_CASE("Issues: the themes have colors for the waves, seen on both papers",
          "[editor][issues]") {
    const EditorColors papers;
    for (const QString& name : {QStringLiteral("Light"), QStringLiteral("Dark")}) {
        QFile file(QStringLiteral(KALAHARI_SOURCE_DIR "/resources/themes/%1.json").arg(name));
        REQUIRE(file.open(QIODevice::ReadOnly));
        const kalahari::core::Theme theme = kalahari::core::Theme::fromJson(
            nlohmann::json::parse(file.readAll().toStdString()));
        for (const bool darkPaper : {false, true}) {
            const QColor paper = darkPaper ? papers.continuous.backgroundDark
                                           : papers.continuous.backgroundLight;
            for (const TextIssueKind kind : {TextIssueKind::Spelling, TextIssueKind::Grammar}) {
                const std::string key = issueColorKey(kind, darkPaper);
                INFO(name.toStdString() << " " << key);
                const auto color = theme.editor.find(key);
                REQUIRE(color != theme.editor.end());
                REQUIRE(color->second.isValid());
                // As other marks that are not text (WCAG): 3 to 1 at least
                CHECK(kalahari::gui::contrastRatio(color->second, paper) >= 3.0);
            }
        }
    }
}
