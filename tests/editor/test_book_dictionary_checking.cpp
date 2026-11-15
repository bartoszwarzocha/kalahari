// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_book_dictionary_checking.cpp
/// @brief The spelling of a chapter checked with the book's dictionary too: its names and words
///        are right, a place written otherwise than its rules want gets a wave of its own, and
///        the context menu offers the rule's form and the dictionary's items

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/affix_expander.h>
#include <kalahari/core/book_dictionary.h>
#include <kalahari/core/name_declension.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/text_highlight.h>
#include <kalahari/editor/text_issue.h>
#include <kalahari/editor/text_source_adapter.h>

#include "editor_test_utils.h"

#include <QContextMenuEvent>
#include <QTextBlock>

#include <algorithm>
#include <memory>
#include <vector>

using kalahari::editor::BookEditor;
using kalahari::editor::HighlightKind;
using kalahari::editor::QTextDocumentSource;
using kalahari::editor::SpellCheckService;
using kalahari::editor::SpellErrorInfo;
using kalahari::editor::TextHighlight;
using kalahari::editor::TextIssue;
using kalahari::editor::TextIssueKind;
using kalahari::test::DICTIONARY_WAIT_MS;
using kalahari::test::kmlOf;
using kalahari::test::runPopupMenu;
using kalahari::test::waitUntil;
using kalahari::core::BookDictionary;
using kalahari::core::NameDeclension;
using kalahari::core::NameKind;
using kalahari::core::SpellingRule;

namespace {

/// A service checking with the shipped English dictionary
std::unique_ptr<SpellCheckService> englishService() {
    auto service = std::make_unique<SpellCheckService>();
    REQUIRE(service->loadDictionary(QStringLiteral("en_US")));
    return service;
}

/// A book's dictionary: the name Kirran Vael, the word "zorblat", and the rule "e-mail", not
/// "email"
BookDictionary sampleDictionary() {
    BookDictionary dictionary;
    dictionary.putName(kalahari::core::makeName(QStringLiteral("Kirran Vael"), NameKind::Character,
                                                NameDeclension(QStringLiteral("en"))));
    dictionary.addWord(QStringLiteral("zorblat"));
    SpellingRule rule;
    rule.right = QStringLiteral("e-mail");
    rule.wrong = {QStringLiteral("email")};
    rule.allForms = false;
    rule.note = QStringLiteral("As the publisher writes it.");
    dictionary.putRule(rule);
    return dictionary;
}

/// The texts with a wave of a kind in a paragraph, in their order
QStringList waves(const BookEditor& editor, int paragraph, HighlightKind kind) {
    const QString text = editor.textDocument()->findBlockByNumber(paragraph).text();
    QStringList found;
    for (const TextHighlight& highlight :
         QTextDocumentSource(editor.textDocument()).paragraphHighlights(paragraph)) {
        if (highlight.kind == kind) {
            found.append(text.mid(highlight.start, highlight.length));
        }
    }
    return found;
}

/// An editor with a paragraph, checked with @p spelling, shown like a small window
std::unique_ptr<BookEditor> shownEditor(const QString& kml, SpellCheckService* spelling) {
    auto editor = std::make_unique<BookEditor>();
    editor->resize(600, 400);
    editor->fromKml(kml);
    editor->setSpellCheckService(spelling);
    editor->show();
    return editor;
}

/// Run the event loop until the editor has checked what it has to
bool checked(const BookEditor& editor) {
    return waitUntil([&editor]() { return !editor.isSpellCheckPending(); }, DICTIONARY_WAIT_MS);
}

/// The context menu at the cursor, as the menu key opens it: its items' texts; the one with
/// the text @p choice is chosen
QStringList menuAtCursor(BookEditor& editor, const QString& choice = QString()) {
    return runPopupMenu(
        [&editor]() {
            QContextMenuEvent event(QContextMenuEvent::Keyboard, QPoint(), QPoint());
            QCoreApplication::sendEvent(&editor, &event);
        },
        choice);
}

}  // namespace

TEST_CASE("Book dictionary: the spelling service takes the words of a filter for right",
          "[editor][spelling][dictionary]") {
    auto spelling = englishService();
    const SpellCheckService::WordFilter book = [](const QString& word) {
        return word == QStringLiteral("Kirran") || word == QStringLiteral("Vael");
    };
    const auto misspelled = [&spelling](const QString& text,
                                        const SpellCheckService::WordFilter& alsoRight) {
        QStringList words;
        for (const SpellErrorInfo& error : spelling->checkParagraph(text, alsoRight)) {
            words.append(error.word);
        }
        return words;
    };

    CHECK_FALSE(spelling->isCorrect(QStringLiteral("Kirran")));
    CHECK(spelling->isCorrect(QStringLiteral("Kirran"), book));
    CHECK(spelling->isCorrect(QStringLiteral("Kirran-Vael"), book));  // by its parts
    CHECK_FALSE(spelling->isCorrect(QStringLiteral("Kirran-Zorblat"), book));

    const QString text = QStringLiteral("Kirran met the Zorblat-Vael.");
    CHECK(misspelled(text, {}) ==
          QStringList{QStringLiteral("Kirran"), QStringLiteral("Zorblat"), QStringLiteral("Vael")});
    CHECK(misspelled(text, book) == QStringList{QStringLiteral("Zorblat")});
}

TEST_CASE("Book dictionary: the forms of the words of the spelling dictionary in use",
          "[editor][spelling][dictionary]") {
    const SpellCheckService none;
    CHECK(none.affixExpander() == nullptr);
    CHECK(none.relatedForms(QStringLiteral("dragon")).isEmpty());

    const auto spelling = englishService();
    const kalahari::core::AffixExpander* forms = spelling->affixExpander();
    REQUIRE(forms != nullptr);
    CHECK(spelling->affixExpander() == forms);  // read once
    CHECK(forms->relatedForms(QStringLiteral("dragon")).contains(QStringLiteral("dragons")));
    CHECK(spelling->relatedForms(QStringLiteral("dragon")).contains(QStringLiteral("dragons")));
    CHECK(spelling->relatedForms(QString()).isEmpty());
}

TEST_CASE("Book dictionary: a chapter checked with it", "[editor][spelling][dictionary]") {
    auto spelling = englishService();
    const BookDictionary dictionary = sampleDictionary();
    auto editor = shownEditor(
        kmlOf({QStringLiteral("Kirran sent an email to Vael about the zorblat and the qwertz.")}),
        spelling.get());
    REQUIRE(checked(*editor));
    REQUIRE(waves(*editor, 0, HighlightKind::Spelling).contains(QStringLiteral("Kirran")));

    editor->setBookDictionary(&dictionary, true);
    CHECK(editor->bookDictionary() == &dictionary);
    CHECK(editor->isBookDictionaryEditable());
    REQUIRE(checked(*editor));

    SECTION("its names and words are right, a place breaking a rule has a wave of its own") {
        CHECK(waves(*editor, 0, HighlightKind::Spelling) == QStringList{QStringLiteral("qwertz")});
        CHECK(waves(*editor, 0, HighlightKind::BookRule) == QStringList{QStringLiteral("email")});

        // In the list of the issues: what the rule wants, its note and its form
        const std::vector<TextIssue> issues = editor->textIssues();
        REQUIRE(issues.size() == 2);
        const TextIssue& rule = issues.front();
        CHECK(rule.kind == TextIssueKind::BookRule);
        CHECK(rule.text == QStringLiteral("email"));
        CHECK(rule.ruleId == dictionary.rules().front().id);
        CHECK(rule.message.startsWith(QStringLiteral("Write ")));
        CHECK(rule.message.contains(QStringLiteral("e-mail")));
        CHECK(rule.ruleDescription == QStringLiteral("As the publisher writes it."));
        CHECK(rule.suggestions == QStringList{QStringLiteral("e-mail")});
        CHECK(issues.back().kind == TextIssueKind::Spelling);
        CHECK(issues.back().suggestions.isEmpty());
    }

    SECTION("Ignore All: the place is not marked until the program closes") {
        spelling->ignoreWord(QStringLiteral("email"));
        REQUIRE(checked(*editor));
        CHECK(waves(*editor, 0, HighlightKind::BookRule).isEmpty());
    }

    SECTION("without the dictionary again: its words are misspelled, the rule not marked") {
        editor->setBookDictionary(nullptr, false);
        REQUIRE(checked(*editor));
        CHECK(waves(*editor, 0, HighlightKind::BookRule).isEmpty());
        CHECK(waves(*editor, 0, HighlightKind::Spelling).contains(QStringLiteral("zorblat")));
    }

    SECTION("the places breaking its rules not marked: its words are still right") {
        editor->setBookRulesMarked(false);
        editor->setBookRulesMarked(false);  // no change: nothing checked again
        REQUIRE(checked(*editor));
        CHECK(waves(*editor, 0, HighlightKind::BookRule).isEmpty());
        CHECK(waves(*editor, 0, HighlightKind::Spelling) == QStringList{QStringLiteral("qwertz")});
        editor->setBookRulesMarked(true);
        REQUIRE(checked(*editor));
        CHECK(waves(*editor, 0, HighlightKind::BookRule) == QStringList{QStringLiteral("email")});
    }

    SECTION("the menu at the place: the rule's form, the rule and its note, Ignore All, "
            "Remove Rule") {
        editor->setCursorPosition({.paragraph = 0, .offset = 16});  // in "email"
        QStringList removed;
        QObject::connect(editor.get(), &BookEditor::removeBookRuleAsked,
                         [&removed](const QString& ruleId) { removed.append(ruleId); });
        QStringList texts = menuAtCursor(*editor, QStringLiteral("&Remove Rule"));
        REQUIRE(texts.size() >= 5);
        CHECK(texts.at(0) == QStringLiteral("e-mail"));
        CHECK(texts.at(1).startsWith(QStringLiteral("Book rule: write ")));
        CHECK(texts.at(2) == QStringLiteral("As the publisher writes it."));
        CHECK(texts.contains(QStringLiteral("&Ignore All")));
        CHECK_FALSE(texts.contains(QStringLiteral("Add to &Book Dictionary")));
        CHECK(removed == QStringList{dictionary.rules().front().id});

        // Edit Rule... opens the window of the rule
        QStringList edited;
        QObject::connect(editor.get(), &BookEditor::editBookRuleAsked,
                         [&edited](const QString& ruleId) { edited.append(ruleId); });
        editor->setCursorPosition({.paragraph = 0, .offset = 16});
        texts = menuAtCursor(*editor, QStringLiteral("&Edit Rule..."));
        CHECK(edited == QStringList{dictionary.rules().front().id});

        // The rule's form put in its place
        editor->setCursorPosition({.paragraph = 0, .offset = 16});
        menuAtCursor(*editor, QStringLiteral("e-mail"));
        CHECK(editor->textDocument()->findBlockByNumber(0).text().contains(
            QStringLiteral("sent an e-mail to")));

        // A dictionary not to be changed (a book open read-only): no Remove Rule
        editor->setBookDictionary(&dictionary, false);
        REQUIRE(checked(*editor));
        editor->fromKml(kmlOf({QStringLiteral("An email.")}));
        REQUIRE(checked(*editor));
        editor->setCursorPosition({.paragraph = 0, .offset = 4});
        texts = menuAtCursor(*editor);
        CHECK(texts.value(0) == QStringLiteral("e-mail"));
        CHECK_FALSE(texts.contains(QStringLiteral("&Remove Rule")));
        CHECK_FALSE(texts.contains(QStringLiteral("&Edit Rule...")));
    }

    SECTION("the menu at a misspelled word: Add to Book Dictionary, Add to Your Words") {
        editor->setCursorPosition({.paragraph = 0, .offset = 57});  // in "qwertz"
        QStringList added;
        QObject::connect(editor.get(), &BookEditor::addToBookDictionaryAsked,
                         [&added](const QString& word) { added.append(word); });
        QStringList texts = menuAtCursor(*editor, QStringLiteral("Add to &Book Dictionary"));
        CHECK(texts.contains(QStringLiteral("&Ignore All")));
        CHECK(texts.contains(QStringLiteral("Add to Your &Words (All Books)")));
        const auto book = texts.indexOf(QStringLiteral("Add to &Book Dictionary"));
        CHECK(book >= 0);
        CHECK(book < texts.indexOf(QStringLiteral("Add to Your &Words (All Books)")));
        CHECK(added == QStringList{QStringLiteral("qwertz")});
        CHECK_FALSE(spelling->isInUserDictionary(QStringLiteral("qwertz")));

        // Add Name... opens the window of a new name with the word
        QStringList names;
        QObject::connect(editor.get(), &BookEditor::addNameAsked,
                         [&names](const QString& word) { names.append(word); });
        editor->setCursorPosition({.paragraph = 0, .offset = 57});
        texts = menuAtCursor(*editor, QStringLiteral("Add Nam&e..."));
        CHECK(texts.indexOf(QStringLiteral("Add Nam&e...")) > book);
        CHECK(names == QStringList{QStringLiteral("qwertz")});

        // Not to be changed: the writer's words only
        editor->setBookDictionary(&dictionary, false);
        editor->setCursorPosition({.paragraph = 0, .offset = 57});
        texts = menuAtCursor(*editor);
        CHECK_FALSE(texts.contains(QStringLiteral("Add to &Book Dictionary")));
        CHECK_FALSE(texts.contains(QStringLiteral("Add Nam&e...")));
        CHECK(texts.contains(QStringLiteral("Add to Your &Words (All Books)")));
    }
}

TEST_CASE("Book dictionary: a name written small gets its own spelling first",
          "[editor][spelling][dictionary]") {
    auto spelling = englishService();
    const BookDictionary dictionary = sampleDictionary();
    auto editor = shownEditor(kmlOf({QStringLiteral("Then kirran left.")}), spelling.get());
    editor->setBookDictionary(&dictionary, true);
    REQUIRE(checked(*editor));
    REQUIRE(waves(*editor, 0, HighlightKind::Spelling) == QStringList{QStringLiteral("kirran")});

    editor->setCursorPosition({.paragraph = 0, .offset = 7});
    const QStringList texts = menuAtCursor(*editor, QStringLiteral("Kirran"));
    CHECK(texts.value(0) == QStringLiteral("Kirran"));
    CHECK(std::ranges::count(texts, QStringLiteral("Kirran")) == 1);
    CHECK(editor->textDocument()->findBlockByNumber(0).text() ==
          QStringLiteral("Then Kirran left."));
}

TEST_CASE("Book dictionary: the texts of the notes are checked with it",
          "[editor][spelling][dictionary][footnotes]") {
    auto spelling = englishService();
    const BookDictionary dictionary = sampleDictionary();
    auto editor = shownEditor(
        QStringLiteral("<kml><p>Kirran<footnote id=\"f1\">Vael wrote an email.</footnote> left."
                       "</p></kml>"),
        spelling.get());
    editor->setBookDictionary(&dictionary, true);
    REQUIRE(checked(*editor));

    std::vector<TextIssue> issues = editor->textIssues();
    REQUIRE(issues.size() == 1);
    CHECK(issues.front().kind == TextIssueKind::BookRule);
    CHECK(issues.front().noteId == QStringLiteral("f1"));
    CHECK(issues.front().text == QStringLiteral("email"));
    CHECK(issues.front().suggestions == QStringList{QStringLiteral("e-mail")});

    // A dictionary changed: checked again when asked
    BookDictionary changed = dictionary;
    changed.removeRule(changed.rules().front().id);
    editor->setBookDictionary(&changed, true);
    REQUIRE(checked(*editor));
    CHECK(editor->textIssues().empty());
}
