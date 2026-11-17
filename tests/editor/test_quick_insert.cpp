// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_quick_insert.cpp
/// @brief Quick insert: the items the typed letters find, and the list at the cursor that @ and
///        # open in the text

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/quick_insert.h>
#include <kalahari/editor/quick_insert_controller.h>
#include "editor_test_utils.h"

#include <QColor>
#include <QCoreApplication>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QPixmap>
#include <QTreeWidget>

#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

QuickInsertItem card(const QString& name) {
    return {QuickInsertSource::Card, name, name, QStringLiteral("character"),
            QStringLiteral("Characters"), QString(), {}};
}

QuickInsertItem fragment(const QString& name, const QString& text) {
    return {.source = QuickInsertSource::Fragment,
            .name = name,
            .text = text,
            .detail = text,
            .group = QStringLiteral("Text Fragments"),
            .owner = QString(),
            .forms = {}};
}

/// A name of the book's dictionary with its forms; @p owner: the name it is another name of
QuickInsertItem name(const QString& text, const QList<QuickInsertForm>& forms,
                     const QString& owner = {}) {
    const QString detail = owner.isEmpty() ? QStringLiteral("character") : owner;
    return {.source = QuickInsertSource::Name,
            .name = text,
            .text = text,
            .detail = detail,
            .group = QStringLiteral("Characters"),
            .owner = owner,
            .forms = forms};
}

/// Anna Wolska, and Anna, her first name alone
QList<QuickInsertItem> annaWolska() {
    const auto form = [](const char* text, const char* cases) {
        return QuickInsertForm{.text = QString::fromUtf8(text), .cases = QString::fromUtf8(cases)};
    };
    return {name(QStringLiteral("Anna Wolska"),
                 {form("Anna Wolska", "nominative"), form("Anny Wolskiej", "genitive"),
                  form("Annie Wolskiej", "dative, locative"), form("Annę Wolską", "accusative"),
                  form("Anną Wolską", "instrumental"), form("Anno Wolska", "vocative")}),
            name(QStringLiteral("Anna"),
                 {form("Anna", "nominative"), form("Anny", "genitive"),
                  form("Annie", "dative, locative"), form("Annę", "accusative"),
                  form("Anną", "instrumental"), form("Anno", "vocative")},
                 QStringLiteral("Anna Wolska"))};
}

QStringList namesOf(const QList<QuickInsertItem>& items) {
    QStringList names;
    for (const QuickInsertItem& item : items) {
        names.append(item.name);
    }
    return names;
}

/// An editor with the paragraphs loaded and quick insert on it: @ for the cards, # for the
/// fragments, nothing (the command) for all of them
struct Fixture {
    std::unique_ptr<BookEditor> editor;
    QuickInsertController* quick = nullptr;
    QList<QuickInsertItem> cards = {card(QStringLiteral("Anna Wolska")),
                                    card(QStringLiteral("Jan Anioł")),
                                    card(QStringLiteral("Żoliborz"))};
    QList<QuickInsertItem> fragments = {fragment(QStringLiteral("scena"), QStringLiteral("* * *")),
                                        fragment(QStringLiteral("cudzysłów"), QStringLiteral("„”")),
                                        fragment(QStringLiteral("list"),
                                                 QStringLiteral("Droga Anno,\nPozdrawiam"))};

    explicit Fixture(const QStringList& paragraphs) {
        editor = std::make_unique<BookEditor>();
        resizeWidget(*editor, QSize(700, 500));
        editor->fromKml(kmlOf(paragraphs));
        quick = new QuickInsertController(editor.get(), [this](QChar trigger) {
            if (trigger == QLatin1Char('@')) {
                return cards;
            }
            if (trigger == QLatin1Char('#')) {
                return fragments;
            }
            return cards + fragments;
        });
        quick->setTriggers(QStringLiteral("@#"));
    }

    /// Type the text as the keyboard does, a character at a time
    void type(const QString& text) {
        for (const QChar ch : text) {
            QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
            QCoreApplication::sendEvent(editor.get(), &event);
        }
    }

    void press(int key) {
        QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
        QCoreApplication::sendEvent(editor.get(), &event);
    }

    QStringList shown() const { return namesOf(quick->list()->items()); }
};

}  // namespace

TEST_CASE("Quick insert: letters folded for the search", "[editor][quickinsert]") {
    CHECK(foldForSearch(QStringLiteral("Żółć")) == QStringLiteral("zolc"));
    CHECK(foldForSearch(QStringLiteral("Łódź")) == QStringLiteral("lodz"));
    CHECK(foldForSearch(QStringLiteral("Anna")) == QStringLiteral("anna"));
}

TEST_CASE("Quick insert: a character starts a word only after a space or an opening mark",
          "[editor][quickinsert]") {
    CHECK(startsQuickInsertWord(QString()));
    CHECK(startsQuickInsertWord(QStringLiteral("zapytał ")));
    CHECK(startsQuickInsertWord(QStringLiteral("– ")));
    CHECK(startsQuickInsertWord(QStringLiteral("„")));
    CHECK(startsQuickInsertWord(QStringLiteral("(")));
    CHECK_FALSE(startsQuickInsertWord(QStringLiteral("jan")));     // jan@poczta.pl
    CHECK_FALSE(startsQuickInsertWord(QStringLiteral("język C")));  // C#
    CHECK_FALSE(startsQuickInsertWord(QStringLiteral("nr 5")));

    CHECK(continuesQuickInsertQuery(QLatin1Char('a')));
    CHECK(continuesQuickInsertQuery(QChar(0x0105)));  // ą
    CHECK(continuesQuickInsertQuery(QLatin1Char('-')));
    CHECK_FALSE(continuesQuickInsertQuery(QLatin1Char(' ')));
    CHECK_FALSE(continuesQuickInsertQuery(QLatin1Char(',')));
    CHECK_FALSE(continuesQuickInsertQuery(QLatin1Char('.')));
}

TEST_CASE("Quick insert: the letters find the starts of the words of names",
          "[editor][quickinsert]") {
    const QList<QuickInsertItem> items = {card(QStringLiteral("Jan Anioł")),
                                          card(QStringLiteral("Anna Wolska")),
                                          card(QStringLiteral("Żoliborz")),
                                          card(QStringLiteral("Ksawery Zając"))};
    CHECK(namesOf(matchQuickInsert(items, QString())) == namesOf(items));
    // A name that starts with the letters comes before one with a later word that does
    CHECK(namesOf(matchQuickInsert(items, QStringLiteral("an"))) ==
          QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Jan Anioł")});
    CHECK(namesOf(matchQuickInsert(items, QStringLiteral("zol"))) ==
          QStringList{QStringLiteral("Żoliborz")});
    CHECK(namesOf(matchQuickInsert(items, QStringLiteral("ZAJ"))) ==
          QStringList{QStringLiteral("Ksawery Zając")});
    // Not in the middle of a word
    CHECK(matchQuickInsert(items, QStringLiteral("olska")).isEmpty());
}

TEST_CASE("Quick insert: pairs of marks leave the cursor between them", "[editor][quickinsert]") {
    CHECK(isEnclosingPair(QStringLiteral("„”")));
    CHECK(isEnclosingPair(QStringLiteral("()")));
    CHECK(isEnclosingPair(QStringLiteral("«»")));
    CHECK_FALSE(isEnclosingPair(QStringLiteral("–")));
    CHECK_FALSE(isEnclosingPair(QStringLiteral("ab")));
}

TEST_CASE("Quick insert: @ at the start of a word opens the cards, the letters narrow them",
          "[editor][quickinsert]") {
    Fixture f({QStringLiteral("zapytał ")});
    f.editor->setCursorPosition({0, 8});

    f.type(QStringLiteral("@"));
    REQUIRE(f.quick->isOpen());
    CHECK_FALSE(f.quick->list()->isHidden());
    CHECK(f.shown() == namesOf(f.cards));

    f.type(QStringLiteral("an"));
    CHECK(f.shown() == QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Jan Anioł")});
    CHECK(f.editor->plainText() == QStringLiteral("zapytał @an"));

    // Down chooses the next one, Enter inserts it in place of @an, without the @
    f.press(Qt::Key_Down);
    CHECK(f.quick->list()->current() == 1);
    f.press(Qt::Key_Return);
    CHECK_FALSE(f.quick->isOpen());
    CHECK(f.editor->plainText() == QStringLiteral("zapytał Jan Anioł"));
    CHECK(f.editor->cursorPosition() == CursorPosition{0, 17});

    // One undo brings back what was typed
    f.editor->undo();
    CHECK(f.editor->plainText() == QStringLiteral("zapytał @an"));
}

TEST_CASE("Quick insert: # opens the fragments; a pair of marks takes the cursor inside",
          "[editor][quickinsert]") {
    Fixture f({QString()});
    f.type(QStringLiteral("#cu"));
    REQUIRE(f.quick->isOpen());
    CHECK(f.shown() == QStringList{QStringLiteral("cudzysłów")});
    f.press(Qt::Key_Tab);
    CHECK(f.editor->plainText() == QStringLiteral("„”"));
    CHECK(f.editor->cursorPosition() == CursorPosition{0, 1});
}

TEST_CASE("Quick insert: a fragment with paragraphs inserts them", "[editor][quickinsert]") {
    Fixture f({QString()});
    f.type(QStringLiteral("#list"));
    REQUIRE(f.quick->isOpen());
    f.press(Qt::Key_Return);
    CHECK(f.editor->paragraphCount() == 2);
    CHECK(f.editor->paragraphPlainText(0) == QStringLiteral("Droga Anno,"));
    CHECK(f.editor->paragraphPlainText(1) == QStringLiteral("Pozdrawiam"));
}

TEST_CASE("Quick insert: inside a word the characters stay characters", "[editor][quickinsert]") {
    // Inside a sentence, where autocorrect leaves the small letter of the address
    Fixture f({QString()});
    f.type(QStringLiteral("Pisz na jan@poczta"));
    CHECK_FALSE(f.quick->isOpen());
    f.type(QStringLiteral(" C#"));
    CHECK_FALSE(f.quick->isOpen());
    CHECK(f.editor->plainText() == QStringLiteral("Pisz na jan@poczta C#"));
}

TEST_CASE("Quick insert: Esc, a space or a punctuation mark close the list, the text stays",
          "[editor][quickinsert]") {
    Fixture f({QString()});
    f.type(QStringLiteral("@a"));
    REQUIRE(f.quick->isOpen());
    f.press(Qt::Key_Escape);
    CHECK_FALSE(f.quick->isOpen());
    CHECK(f.editor->plainText() == QStringLiteral("@a"));

    f.type(QStringLiteral(" @"));
    REQUIRE(f.quick->isOpen());
    f.type(QStringLiteral(" "));
    CHECK_FALSE(f.quick->isOpen());

    f.type(QStringLiteral("@an,"));
    CHECK_FALSE(f.quick->isOpen());
    CHECK(f.editor->plainText() == QStringLiteral("@a @ @an,"));

    // Backspace over the @ closes it too
    f.type(QStringLiteral(" @"));
    REQUIRE(f.quick->isOpen());
    f.press(Qt::Key_Backspace);
    CHECK_FALSE(f.quick->isOpen());
}

TEST_CASE("Quick insert: nothing found shows so, and the next letter closes the list",
          "[editor][quickinsert]") {
    Fixture f({QString()});
    f.type(QStringLiteral("@x"));
    REQUIRE(f.quick->isOpen());
    CHECK(f.shown().isEmpty());
    CHECK(f.quick->list()->current() == -1);
    // Enter with nothing found does what it does in the text: a new paragraph
    f.press(Qt::Key_Return);
    CHECK_FALSE(f.quick->isOpen());
    CHECK(f.editor->paragraphCount() == 2);

    f.type(QStringLiteral("@xy"));
    CHECK_FALSE(f.quick->isOpen());
}

TEST_CASE("Quick insert: the command opens the list with everything at the cursor",
          "[editor][quickinsert]") {
    Fixture f({QStringLiteral("Wtedy ")});
    f.editor->setCursorPosition({0, 6});
    f.quick->openAtCursor();
    REQUIRE(f.quick->isOpen());
    CHECK(f.shown().size() == f.cards.size() + f.fragments.size());
    f.type(QStringLiteral("sc"));
    CHECK(f.shown() == QStringList{QStringLiteral("scena")});
    f.press(Qt::Key_Return);
    CHECK(f.editor->plainText() == QStringLiteral("Wtedy * * *"));
}

TEST_CASE("Quick insert: without triggers or items the characters do not open the list",
          "[editor][quickinsert]") {
    Fixture f({QString()});
    f.quick->setTriggers(QString());
    f.type(QStringLiteral("@"));
    CHECK_FALSE(f.quick->isOpen());

    f.quick->setTriggers(QStringLiteral("@#"));
    f.cards.clear();
    f.type(QStringLiteral(" @"));
    CHECK_FALSE(f.quick->isOpen());
}

TEST_CASE("Quick insert: a read-only text opens no list", "[editor][quickinsert]") {
    Fixture f({QStringLiteral("Tekst ")});
    f.editor->setReadOnly(true);
    f.editor->setCursorPosition({0, 6});
    f.type(QStringLiteral("@"));
    f.quick->openAtCursor();
    CHECK_FALSE(f.quick->isOpen());
}

TEST_CASE("Quick insert: the letters find the forms of names", "[editor][quickinsert]") {
    const QList<QuickInsertItem> names = annaWolska();

    // The letters of a name find it with its forms; the whole name first
    CHECK(namesOf(matchQuickInsert(names, QStringLiteral("an"))) ==
          QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Anna")});
    CHECK(namesOf(matchQuickInsert(names, QStringLiteral("anna"))) ==
          QStringList{QStringLiteral("Anna"), QStringLiteral("Anna Wolska")});
    CHECK(matchQuickInsert(names, QStringLiteral("an")).at(0).forms.size() == 6);

    // Those of a form find the form, with its case and the name it is a form of
    const QList<QuickInsertItem> accusative = matchQuickInsert(names, QStringLiteral("annę"));
    REQUIRE(namesOf(accusative) ==
            QStringList{QStringLiteral("Annę"), QStringLiteral("Annę Wolską")});
    CHECK(accusative.at(0).text == QStringLiteral("Annę"));
    CHECK(accusative.at(0).detail == QStringLiteral("accusative · Anna (Anna Wolska)"));
    CHECK(accusative.at(0).forms.isEmpty());
    CHECK(accusative.at(1).detail == QStringLiteral("accusative · Anna Wolska"));

    // As typed before without the marks of the letters: "wolską" is not "Wolska"
    CHECK(namesOf(matchQuickInsert(names, QStringLiteral("wolską"))) ==
          QStringList{QStringLiteral("Annę Wolską"), QStringLiteral("Anną Wolską")});
    CHECK(namesOf(matchQuickInsert(names, QStringLiteral("anne"))) ==
          QStringList{QStringLiteral("Annę"), QStringLiteral("Annę Wolską")});
    CHECK(matchQuickInsert(names, QStringLiteral("annx")).isEmpty());

    // A form in several cases is one item
    CHECK(namesOf(matchQuickInsert(names, QStringLiteral("annie"))) ==
          QStringList{QStringLiteral("Annie"), QStringLiteral("Annie Wolskiej")});
}

TEST_CASE("Quick insert: Right shows the forms of a name, Left the list again",
          "[editor][quickinsert]") {
    Fixture f({QStringLiteral("Czy ktoś widział ")});
    f.cards = annaWolska();
    f.editor->setCursorPosition({.paragraph = 0, .offset = 17});
    f.type(QStringLiteral("@an"));
    REQUIRE(f.quick->isOpen());
    const QuickInsertList* list = f.quick->list();
    CHECK(list->hasForms());
    CHECK_FALSE(list->showsForms());

    f.press(Qt::Key_Right);
    REQUIRE(list->showsForms());
    CHECK(f.shown() == QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Anny Wolskiej"),
                                   QStringLiteral("Annie Wolskiej"), QStringLiteral("Annę Wolską"),
                                   QStringLiteral("Anną Wolską"), QStringLiteral("Anno Wolska")});
    CHECK(list->current() == 0);
    CHECK(list->items().at(2).detail == QStringLiteral("dative, locative"));
    CHECK(f.editor->plainText() == QStringLiteral("Czy ktoś widział @an"));  // the cursor stays

    // Left: the list again, the name whose forms were shown chosen
    f.press(Qt::Key_Down);
    f.press(Qt::Key_Left);
    CHECK_FALSE(list->showsForms());
    CHECK(f.shown() == QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Anna")});
    CHECK(list->current() == 0);

    // Enter inserts the form chosen in place of @an
    f.press(Qt::Key_Down);
    f.press(Qt::Key_Right);
    CHECK(f.shown().at(3) == QStringLiteral("Annę"));
    for (int step = 0; step < 3; ++step) {
        f.press(Qt::Key_Down);
    }
    f.press(Qt::Key_Return);
    CHECK_FALSE(f.quick->isOpen());
    CHECK(f.editor->plainText() == QStringLiteral("Czy ktoś widział Annę"));

    // A letter typed while the forms are shown narrows the list again
    f.type(QStringLiteral(" i @an"));
    f.press(Qt::Key_Right);
    REQUIRE(list->showsForms());
    f.type(QStringLiteral("n"));
    CHECK_FALSE(list->showsForms());
    CHECK(f.shown() == QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Anna")});
}

TEST_CASE("Quick insert: without forms Right and Left move the cursor in the text",
          "[editor][quickinsert]") {
    Fixture f({QStringLiteral("zapytał ")});
    f.editor->setCursorPosition({.paragraph = 0, .offset = 8});
    f.type(QStringLiteral("@an"));
    REQUIRE(f.quick->isOpen());
    CHECK_FALSE(f.quick->list()->hasForms());
    CHECK_FALSE(f.quick->list()->showForms());
    f.press(Qt::Key_Left);
    CHECK(f.editor->cursorPosition() == CursorPosition{0, 10});
    CHECK(f.quick->isOpen());
    CHECK(f.shown() == QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Jan Anioł")});
}

TEST_CASE("Quick insert: the names of the cards with their colors", "[editor][quickinsert]") {
    Fixture f({QStringLiteral("zapytał ")});
    f.cards.first().color = QStringLiteral("#c0504d");
    f.editor->setCursorPosition({.paragraph = 0, .offset = 8});
    f.type(QStringLiteral("@"));
    REQUIRE(f.quick->isOpen());
    const auto* tree = f.quick->list()->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    // The rows after the heading of the group: a dot of the color of Anna Wolska, an empty place
    // of its size for the others, in line with it
    QList<const QTreeWidgetItem*> rows;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->flags() != Qt::NoItemFlags) {
            rows.append(tree->topLevelItem(i));
        }
    }
    REQUIRE(rows.size() == 3);
    const QImage dot = rows.at(0)->icon(0).pixmap(tree->iconSize()).toImage();
    const QImage none = rows.at(1)->icon(0).pixmap(tree->iconSize()).toImage();
    REQUIRE_FALSE(dot.isNull());
    REQUIRE_FALSE(none.isNull());
    const QColor middle = dot.pixelColor(dot.width() / 2, dot.height() / 2);
    CHECK(middle == QColor(QStringLiteral("#c0504d")));
    CHECK(none.pixelColor(none.width() / 2, none.height() / 2).alpha() == 0);

    // Without colors, no places for them
    f.press(Qt::Key_Escape);
    f.cards.first().color.clear();
    f.type(QStringLiteral(" @"));
    REQUIRE(f.quick->isOpen());
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        CHECK(tree->topLevelItem(i)->icon(0).isNull());
    }
}
