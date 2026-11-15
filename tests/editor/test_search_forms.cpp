// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_search_forms.cpp
/// @brief The search of all forms ("Odm"): the forms of a name of the book's dictionary or of
///        a word of the spelling dictionary, found as whole words, and its option on the find
///        bar

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/book_dictionary.h>
#include <kalahari/core/name_declension.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/find_replace_bar.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/search_forms.h>
#include <kalahari/editor/spell_check_service.h>

#include <QList>
#include <QPushButton>
#include <QStringList>
#include <QTextDocument>
#include <QToolButton>

#include "editor_test_utils.h"

#include <algorithm>
#include <vector>

using kalahari::core::BookDictionary;
using kalahari::core::makeName;
using kalahari::core::NameDeclension;
using kalahari::core::NameKind;
using kalahari::editor::BookEditor;
using kalahari::editor::FindReplaceBar;
using kalahari::editor::formsToFind;
using kalahari::editor::SearchEngine;
using kalahari::editor::SearchMatch;
using kalahari::editor::SearchOptions;
using kalahari::editor::SpellCheckService;

namespace {

/// The texts of the matches
QStringList textsOf(const std::vector<SearchMatch>& matches) {
    QStringList texts;
    for (const SearchMatch& match : matches) {
        texts << match.matchedText;
    }
    return texts;
}

/// The option of all forms of a find bar
QToolButton* formsOption(const FindReplaceBar& bar) {
    const QList<QToolButton*> buttons = bar.findChildren<QToolButton*>();
    const auto found = std::ranges::find_if(buttons, [](const QToolButton* button) {
        return button->text() == QStringLiteral("Forms");
    });
    return found != buttons.end() ? *found : nullptr;
}

/// A dictionary of the book with the name Anna Wolska
BookDictionary annaWolska() {
    BookDictionary dictionary;
    dictionary.putName(makeName(QStringLiteral("Anna Wolska"), NameKind::Character,
                                NameDeclension(QStringLiteral("pl"))));
    return dictionary;
}

}  // namespace

TEST_CASE("Search of all forms: the forms as whole words, the longest first",
          "[editor][search][forms]") {
    QTextDocument document;
    document.setPlainText(QStringLiteral("Anna i Annie. Annę widział Annabel; ANNY nie."));
    SearchEngine engine;
    engine.setDocument(&document);
    engine.setSearchText(QStringLiteral("Anna"));

    SearchOptions options;
    options.allForms = true;
    options.forms = {QStringLiteral("Anna"), QStringLiteral("Anny"), QStringLiteral("Anni"),
                     QStringLiteral("Annie"), QStringLiteral("Annę")};
    engine.setOptions(options);
    CHECK(textsOf(engine.findAll()) ==
          QStringList{QStringLiteral("Anna"), QStringLiteral("Annie"), QStringLiteral("Annę"),
                      QStringLiteral("ANNY")});

    // Match Case: the forms as they are written
    options.caseSensitive = true;
    engine.setOptions(options);
    CHECK(engine.findAll().size() == 3);

    // Without forms the text itself, as the other options say
    options = SearchOptions{};
    options.allForms = true;
    engine.setOptions(options);
    CHECK(textsOf(engine.findAll()) ==
          QStringList{QStringLiteral("Anna"), QStringLiteral("Anna")});  // Annabel too

    // A regular expression is searched as written
    options.useRegex = true;
    options.forms = {QStringLiteral("Annie")};
    engine.setOptions(options);
    engine.setSearchText(QStringLiteral("Ann[ae]"));
    CHECK(textsOf(engine.findAll()) == QStringList{QStringLiteral("Anna"), QStringLiteral("Anna")});
}

TEST_CASE("Search of all forms: the forms of a name of the book's dictionary",
          "[editor][search][forms][dictionary]") {
    const BookDictionary dictionary = annaWolska();
    const QStringList anna = formsToFind(QStringLiteral("Anna"), &dictionary, nullptr);
    for (const char* form : {"Anna", "Anny", "Annie", "Annę", "Anną", "Anno"}) {
        CHECK(anna.contains(QString::fromUtf8(form)));
    }
    CHECK_FALSE(anna.contains(QStringLiteral("Wolska")));
    // From a form of it, in any case
    CHECK(formsToFind(QString::fromUtf8("annę"), &dictionary, nullptr) == anna);
    CHECK(formsToFind(QStringLiteral("Anna Wolska"), &dictionary, nullptr)
              .contains(QString::fromUtf8("Annę Wolską")));
    // Not a name, and no spelling dictionary: the text itself
    CHECK(formsToFind(QStringLiteral("Jan"), &dictionary, nullptr).isEmpty());
    CHECK(formsToFind(QString(), &dictionary, nullptr).isEmpty());
}

TEST_CASE("Search of all forms: the forms of a word of the spelling dictionary",
          "[editor][search][forms]") {
    SpellCheckService spelling;
    REQUIRE(spelling.loadDictionary(QStringLiteral("pl_PL")));
    const QStringList smok = formsToFind(QStringLiteral("smoka"), nullptr, &spelling);
    for (const char* form : {"smok", "smoka", "smokowi", "smokiem", "smoki", "smoków"}) {
        CHECK(smok.contains(QString::fromUtf8(form)));
    }
    const QStringList said = formsToFind(QString::fromUtf8("powiedział"), nullptr, &spelling);
    CHECK(said.contains(QString::fromUtf8("powiedziała")));
    CHECK(said.contains(QString::fromUtf8("powiedzieli")));
    // At the start of a sentence
    CHECK(formsToFind(QStringLiteral("Smoka"), nullptr, &spelling).contains(QStringLiteral("smokiem")));
    // A name of the book comes first
    const BookDictionary dictionary = annaWolska();
    CHECK_FALSE(formsToFind(QStringLiteral("Anna"), &dictionary, &spelling)
                    .contains(QStringLiteral("anna")));
    // Words the dictionary does not know, and more words than one: the text itself
    CHECK(formsToFind(QStringLiteral("zorblat"), nullptr, &spelling).isEmpty());
    CHECK(formsToFind(QStringLiteral("dwa smoki"), nullptr, &spelling).isEmpty());
}

TEST_CASE("Search of all forms: the option of the find bar, without Replace All",
          "[editor][search][forms]") {
    QTextDocument document;
    document.setPlainText(QStringLiteral("Smok i smoka, smokiem."));
    SearchEngine engine;
    engine.setDocument(&document);
    FindReplaceBar bar;
    bar.setSearchEngine(&engine);
    bar.setFormsOf([](const QString&) {
        return QStringList{QStringLiteral("smok"), QStringLiteral("smoka"),
                           QStringLiteral("smokiem")};
    });
    bar.showFindReplace();
    bar.setSearchText(QStringLiteral("smok"));
    REQUIRE(engine.totalMatchCount() == 3);  // as parts of words too

    QToolButton* forms = formsOption(bar);
    REQUIRE(forms != nullptr);
    const QPushButton* replaceAll = nullptr;
    for (const QPushButton* button : bar.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Replace All")) {
            replaceAll = button;
        }
    }
    REQUIRE(replaceAll != nullptr);
    CHECK(replaceAll->isEnabled());

    forms->toggle();
    CHECK(engine.options().allForms);
    CHECK(textsOf(engine.findAll()) ==
          QStringList{QStringLiteral("Smok"), QStringLiteral("smoka"), QStringLiteral("smokiem")});
    CHECK_FALSE(replaceAll->isEnabled());
    CHECK(replaceAll->toolTip().contains(QStringLiteral("all forms")));

    forms->toggle();
    CHECK_FALSE(engine.options().allForms);
    CHECK(replaceAll->isEnabled());
}

TEST_CASE("Search of all forms: the find bar of a chapter finds the forms of its book's names",
          "[editor][search][forms][dictionary]") {
    const BookDictionary dictionary = annaWolska();
    BookEditor editor;
    editor.fromKml(kalahari::test::kmlOf({QStringLiteral("Anna i Annie, Annabel.")}));
    editor.setBookDictionary(&dictionary, false);
    editor.showFind();
    auto* bar = editor.findChild<FindReplaceBar*>();
    REQUIRE(bar != nullptr);
    bar->setSearchText(QStringLiteral("Anna"));
    REQUIRE(editor.searchEngine() != nullptr);
    CHECK(textsOf(editor.searchEngine()->findAll()) ==
          QStringList{QStringLiteral("Anna"), QStringLiteral("Anna")});  // Annabel too

    QToolButton* forms = formsOption(*bar);
    REQUIRE(forms != nullptr);
    forms->toggle();
    CHECK(textsOf(editor.searchEngine()->findAll()) ==
          QStringList{QStringLiteral("Anna"), QStringLiteral("Annie")});
}
