/// @file test_quick_insert.cpp
/// @brief Quick insert: the items the typed letters find, and the list at the cursor that @ and
///        # open in the text

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/quick_insert.h>
#include <kalahari/editor/quick_insert_controller.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QKeyEvent>

#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

QuickInsertItem card(const QString& name) {
    return {QuickInsertSource::Card, name, name, QStringLiteral("character"),
            QStringLiteral("Characters")};
}

QuickInsertItem fragment(const QString& name, const QString& text) {
    return {QuickInsertSource::Fragment, name, text, text, QStringLiteral("Text Fragments")};
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
    Fixture f({QString()});
    f.type(QStringLiteral("jan@poczta"));
    CHECK_FALSE(f.quick->isOpen());
    f.type(QStringLiteral(" C#"));
    CHECK_FALSE(f.quick->isOpen());
    CHECK(f.editor->plainText() == QStringLiteral("jan@poczta C#"));
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
