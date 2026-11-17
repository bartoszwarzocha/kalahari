// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_declension.cpp
/// @brief The names of the cards in the book's dictionary: the name a new card gets, a new
/// title, another name, the declension on the short and the full card, Create Card of the
/// texts, and the window Relation, which finds the cards by the forms of their names and
/// declines them

#include "document_window.h"
#include "test_support/window_parts.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_names.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/book_dictionary_coordinator.h"
#include "kalahari/gui/card_names_coordinator.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/relation_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/widgets/card_name_box.h"
#include "kalahari/gui/widgets/name_forms_table.h"

#include <catch2/catch_test_macros.hpp>

#include <QAbstractButton>
#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidgetItemIterator>

#include <cstdio>
#include <functional>
#include <optional>

namespace kalahari {

namespace {

using core::DictionaryName;
using core::GrammaticalCase;
using core::NameKind;
using core::NameVariant;

/// @p utf8 as a QString
QString text(const char* utf8) {
    return QString::fromUtf8(utf8);
}

/// A Polish novel open until the test ends
struct PolishNovel {
    QTemporaryDir dir;
    core::ProjectManager& pm = test::book();
    QString chapterId;

    PolishNovel() {
        REQUIRE(dir.isValid());
        core::ProjectManager::loadBookTypes(
            {QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        REQUIRE(pm.createProject(dir.path(), QStringLiteral("Deszcz"), QStringLiteral("Autor"),
                                 QStringLiteral("pl"), false, QStringLiteral("kalahari.novel")));
        chapterId = pm.addElement(pm.chapterKindFor(core::BookPlace::Main),
                                  QStringLiteral("Burza"), core::BookPlace::Main);
        REQUIRE_FALSE(chapterId.isEmpty());
    }
    ~PolishNovel() {
        try {
            pm.closeProject(false);
        } catch (...) {
            // A destructor may not throw: the later tests then find this book open
            static_cast<void>(std::fputs("PolishNovel: the book of the test stays open\n", stderr));
        }
    }
    PolishNovel(const PolishNovel&) = delete;
    PolishNovel& operator=(const PolishNovel&) = delete;
    PolishNovel(PolishNovel&&) = delete;
    PolishNovel& operator=(PolishNovel&&) = delete;
};

/// The book's dictionary with its windows, and the names of the cards in it, as the main
/// window has them
struct CardNames {
    QStatusBar statusBar;
    QTabWidget tabs;
    gui::BookDictionaryCoordinator dictionary{test::book(), test::commands(), &tabs, nullptr,
                                              &statusBar};
    gui::CardNamesCoordinator names{test::book(), dictionary, &tabs};
};

/// The kind @p kindId of the Base
core::KindRef baseKind(const core::ProjectManager& pm, const char* kindId) {
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QLatin1String(kindId));
    REQUIRE(kind);
    return kind;
}

/// A new card @p title of kind @p kindId of the Base in the Workshop
QString addCard(core::ProjectManager& pm, const char* kindId, const QString& title) {
    const QString id = pm.addElement(baseKind(pm, kindId), title, core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// The name of card @p cardId in the book's dictionary
const DictionaryName& nameOf(const core::ProjectManager& pm, const QString& cardId) {
    const DictionaryName* name = pm.bookDictionary().findNameOfCard(cardId);
    REQUIRE(name != nullptr);
    return *name;
}

/// Put @p name in the book's dictionary
/// @return Its id there
QString put(core::ProjectManager& pm, const DictionaryName& name) {
    QString id;
    REQUIRE(pm.changeBookDictionary(
        [&name, &id](core::BookDictionary& dictionary) { id = dictionary.putName(name); }));
    return id;
}

/// The texts of the variants of @p name, the name itself first
QStringList textsOf(const DictionaryName& name) {
    QStringList texts;
    for (const NameVariant& variant : name.variants) {
        texts << variant.text;
    }
    return texts;
}

/// The texts of @p buttons
QStringList textsOf(const QList<QToolButton*>& buttons) {
    QStringList texts;
    for (const QToolButton* button : buttons) {
        texts << button->text();
    }
    return texts;
}

/// What a test does with a window shown
using Answer = std::function<void(QDialog&)>;

/// Run @p command, answering the windows it shows with @p answers in turn; a window beyond
/// them is closed with Cancel, so that the test never waits for it
/// @return The number of windows the command showed
int answering(const std::function<void()>& command, QList<Answer> answers = {}) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &answers]() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr || !dialog->isVisible()) {
            return;
        }
        ++shown;
        if (answers.isEmpty()) {
            dialog->reject();
        } else {
            answers.takeFirst()(*dialog);
        }
    });
    timer.start();
    command();
    return shown;
}

/// Answer the window for a text with @p typed and its accept button
Answer typing(const QString& typed) {
    return [typed](QDialog& dialog) {
        auto* window = qobject_cast<gui::dialogs::TextInputDialog*>(&dialog);
        if (window == nullptr) {  // no REQUIRE in an event handler
            dialog.reject();
            return;
        }
        window->field()->setText(typed);
        window->acceptButton()->click();
    };
}

/// Press @p key in @p widget, as the writer would
void press(QWidget* widget, Qt::Key key) {
    QKeyEvent override(QEvent::ShortcutOverride, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &override);
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

/// The widgets that Tab goes through from @p first, in order, until it comes back to it
QList<QWidget*> tabChain(QWidget* first) {
    QList<QWidget*> chain{first};
    const QWidget* window = first->window();
    for (QWidget* widget = first->nextInFocusChain(); widget != first;
         widget = widget->nextInFocusChain()) {
        const Qt::FocusPolicy policy = widget->focusPolicy();
        const bool tabs =
            policy == Qt::TabFocus || policy == Qt::StrongFocus || policy == Qt::WheelFocus;
        if (tabs && widget->isVisibleTo(window) && widget->isEnabled() &&
            widget->focusProxy() == nullptr) {
            chain << widget;
        }
    }
    return chain;
}

/// The texts of the cards the list of the window Relation shows
QStringList shownCards(const gui::dialogs::RelationDialog& dialog) {
    QStringList cards;
    for (QTreeWidgetItemIterator it(dialog.cardList()); *it != nullptr; ++it) {
        if ((*it)->parent() != nullptr && !(*it)->isHidden()) {
            cards << (*it)->text(0);
        }
    }
    return cards;
}

}  // namespace

TEST_CASE("Card declension: a new card gets its name in the book's dictionary, a renamed card "
          "its new title, the window Name and another name",
          "[gui][cards][dictionary]") {
    const PolishNovel novel;
    auto& pm = novel.pm;
    CardNames parts;
    const core::NameDeclension polish(QStringLiteral("pl"));

    // A name without a card: a card of its name takes it over, with the forms the author wrote
    DictionaryName ruth = core::makeName(QStringLiteral("Ruth"), NameKind::Character, polish);
    ruth.variants[0].forms.cell(GrammaticalCase::Vocative, false) =
        QStringList{QStringLiteral("Ruth!")};
    ruth.variants[0].edited = QStringList{QStringLiteral("vocative")};
    const QString ruthId = put(pm, ruth);

    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));
    CHECK(textsOf(nameOf(pm, anna)) == QStringList{QStringLiteral("Anna Wolska"),
                                                  QStringLiteral("Anna"),
                                                  QStringLiteral("Wolska")});
    CHECK(core::formOfName(nameOf(pm, anna), GrammaticalCase::Genitive) == text("Anny Wolskiej"));
    const QString ruthCard = addCard(pm, "character", QStringLiteral("ruth"));
    CHECK(nameOf(pm, ruthCard).id == ruthId);
    CHECK(nameOf(pm, ruthCard).variants.at(0).forms.cell(GrammaticalCase::Vocative, false) ==
          QStringList{QStringLiteral("Ruth!")});
    const QString city = addCard(pm, "location", text("Gdańsk"));
    CHECK(nameOf(pm, city).kind == NameKind::Place);
    CHECK(core::formOfName(nameOf(pm, city), GrammaticalCase::Locative) == text("Gdańsku"));
    // Not a card of a character, a location or an item: no name
    addCard(pm, "work_note", QStringLiteral("Notatka"));
    CHECK(pm.bookDictionary().names().size() == 3);

    // A new title: the name follows it
    REQUIRE(pm.renameElement(anna, QStringLiteral("Anna Kowalska")));
    CHECK(nameOf(pm, anna).text() == QStringLiteral("Anna Kowalska"));
    CHECK(core::formOfName(nameOf(pm, anna), GrammaticalCase::Genitive) ==
          text("Anny Kowalskiej"));

    // The window Name for the name of the card
    QString shown;
    CHECK(answering([&parts, &anna]() { CHECK_FALSE(parts.names.editName(anna)); },
                    {[&shown](QDialog& dialog) {
                        if (const auto* field =
                                dialog.findChild<QLineEdit*>(QStringLiteral("nameText"))) {
                            shown = field->text();
                        }
                        dialog.reject();
                    }}) == 1);
    CHECK(shown == QStringLiteral("Anna Kowalska"));
    CHECK_FALSE(parts.names.editName(QStringLiteral("missing")));

    // Another name, with the forms the program gives it; a name it has already is not added
    bool added = false;
    CHECK(answering([&]() { added = parts.names.addOtherName(anna, &parts.tabs); },
                    {typing(QStringLiteral(" Ania "))}) == 1);
    CHECK(added);
    REQUIRE(nameOf(pm, anna).variants.size() == 4);
    const NameVariant& ania = nameOf(pm, anna).variants.at(3);
    CHECK(ania.text == QStringLiteral("Ania"));
    CHECK_FALSE(ania.automatic);
    CHECK(ania.forms.cell(GrammaticalCase::Genitive, false) == QStringList{QStringLiteral("Ani")});
    CHECK(answering([&]() { added = parts.names.addOtherName(anna, &parts.tabs); },
                    {typing(QStringLiteral("ANIA"))}) == 1);
    CHECK_FALSE(added);
    CHECK(answering([&]() { added = parts.names.addOtherName(anna, &parts.tabs); }) == 1);
    CHECK_FALSE(added);
    CHECK(nameOf(pm, anna).variants.size() == 4);
    CHECK(answering([&]() {
              CHECK_FALSE(parts.names.addOtherName(QStringLiteral("missing"), &parts.tabs));
          }) == 0);

    // The new title keeps the other name the author added
    REQUIRE(pm.renameElement(anna, QStringLiteral("Anna Nowak")));
    CHECK(textsOf(nameOf(pm, anna)).contains(QStringLiteral("Ania")));
    CHECK_FALSE(textsOf(nameOf(pm, anna)).contains(QStringLiteral("Kowalska")));
}

TEST_CASE("Card declension: the short card shows the forms and the other names, Change... "
          "opens the window Name",
          "[gui][cards][dictionary]") {
    gui::registerAllIcons();
    const PolishNovel novel;
    auto& pm = novel.pm;
    const CardNames parts;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));

    gui::CardPage page(pm, false);
    QStringList windows;
    QObject::connect(&page, &gui::CardPage::requestNameWindow,
                     [&windows](const QString& cardId) { windows << cardId; });
    page.showCard(anna);
    gui::CardNameBox* box = page.nameBox();
    REQUIRE(box != nullptr);
    CHECK(box->title() == QStringLiteral("Declension"));
    CHECK(box->formsTable() == nullptr);
    CHECK(box->variantButtons().isEmpty());
    const QString forms = box->formsLabel()->text();
    CHECK(forms.startsWith(text("Anny Wolskiej · Annie Wolskiej · Annę Wolską")));
    CHECK_FALSE(forms.contains(QStringLiteral("Anna Wolska")));
    CHECK(forms.count(text("Annie Wolskiej")) == 1);  // the dative and the locative, once
    CHECK(box->otherNamesLabel()->text() == QStringLiteral("Other names: Anna, Wolska"));
    CHECK_FALSE(box->otherNamesLabel()->isHidden());

    // Change...: the window Name of the card
    CHECK(box->nameButton()->text() == QStringLiteral("Change..."));
    box->nameButton()->click();
    CHECK(windows == QStringList{anna});

    // A change of the dictionary shows at once
    DictionaryName name = nameOf(pm, anna);
    NameVariant ania;
    ania.text = QStringLiteral("Ania");
    name.variants.append(ania);
    put(pm, name);
    CHECK(box->otherNamesLabel()->text() == QStringLiteral("Other names: Anna, Wolska, Ania"));

    // A name of one word without other names
    const QString jan = addCard(pm, "character", QStringLiteral("Jan"));
    page.showCard(jan);
    CHECK(box->formsLabel()->text().startsWith(QStringLiteral("Jana · Janowi · Janem")));
    CHECK(box->otherNamesLabel()->isHidden());

    // A card whose name the dictionary does not have: it says so
    REQUIRE(pm.changeBookDictionary([&jan](core::BookDictionary& dictionary) {
        dictionary.removeName(dictionary.findNameOfCard(jan)->id);
    }));
    CHECK(box->formsLabel()->isHidden());
    CHECK(box->otherNamesLabel()->text().startsWith(
        QStringLiteral("The book's dictionary does not have this name")));
    CHECK(box->nameButton()->isEnabled());
}

TEST_CASE("Card declension: the full card shows the forms by case of the name or of another "
          "name, corrected right there",
          "[gui][cards][dictionary]") {
    gui::registerAllIcons();
    const PolishNovel novel;
    auto& pm = novel.pm;
    const CardNames parts;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));

    gui::CardPage page(pm, true);
    page.resize(1100, 900);
    QStringList windows;
    QStringList others;
    QObject::connect(&page, &gui::CardPage::requestNameWindow,
                     [&windows](const QString& cardId) { windows << cardId; });
    QObject::connect(&page, &gui::CardPage::requestOtherName,
                     [&others](const QString& cardId) { others << cardId; });
    page.showCard(anna);
    gui::CardNameBox* box = page.nameBox();
    REQUIRE(box != nullptr);
    QList<QToolButton*> buttons = box->variantButtons();
    CHECK(textsOf(buttons) == QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Anna"),
                                          QStringLiteral("Wolska")});
    REQUIRE(buttons.size() == 3);
    CHECK(buttons.at(0)->isChecked());
    CHECK(buttons.at(0)->toolTip() == QStringLiteral("The name itself"));
    CHECK(buttons.at(1)->toolTip() == QStringLiteral("Added by the program"));
    gui::NameFormsTable* table = box->formsTable();
    REQUIRE(table != nullptr);
    CHECK(table->item(1, 1)->text() == text("Anny Wolskiej"));
    CHECK(box->modelLabel()->text() == QStringLiteral("Declined as: Anna + Kowalska"));
    CHECK(box->nameButton()->text() == QStringLiteral("Name Window..."));

    // Another name chosen: its forms
    buttons.at(2)->click();
    CHECK(buttons.at(2)->isChecked());
    CHECK(table->variant().text == QStringLiteral("Wolska"));
    CHECK(table->item(1, 1)->text() == QStringLiteral("Wolskiej"));
    CHECK(box->modelLabel()->text() == QStringLiteral("Declined as: Kowalska"));

    // A form corrected in the table goes to the dictionary, as the author wrote it
    table->item(6, 1)->setText(QStringLiteral("Wolska!"));
    const NameVariant& wolska = nameOf(pm, anna).variants.at(2);
    CHECK(wolska.forms.cell(GrammaticalCase::Vocative, false) ==
          QStringList{QStringLiteral("Wolska!")});
    CHECK(wolska.edited.contains(core::FormTable::cellKey(GrammaticalCase::Vocative, false)));
    CHECK(table->item(6, 1)->text() == QStringLiteral("Wolska!"));

    // "+" and Insert: another name; Name Window...: the window Name
    box->addButton()->click();
    press(table, Qt::Key_Insert);
    CHECK(others == QStringList{anna, anna});
    box->nameButton()->click();
    CHECK(windows == QStringList{anna});

    // Another name in the dictionary: its button before "+", in the order of Tab; the one
    // chosen stays
    DictionaryName name = nameOf(pm, anna);
    NameVariant ania;
    ania.text = QStringLiteral("Ania");
    name.variants.append(ania);
    put(pm, name);
    buttons = box->variantButtons();
    CHECK(textsOf(buttons).last() == QStringLiteral("Ania"));
    CHECK(buttons.at(3)->toolTip() == QStringLiteral("Added by the author"));
    CHECK(buttons.at(2)->isChecked());
    CHECK(table->variant().text == QStringLiteral("Wolska"));
    const QList<QWidget*> chain = tabChain(buttons.at(0));
    CHECK(chain.mid(0, 7) == QList<QWidget*>{buttons.at(0), buttons.at(1), buttons.at(2),
                                             buttons.at(3), box->addButton(), table,
                                             box->nameButton()});
    CHECK(box->keyParts() == chain.mid(0, 7));

    // An other name gone, the one chosen with it: the name itself
    name.variants.removeLast();
    name.variants.removeLast();
    put(pm, name);
    QApplication::processEvents();  // the buttons over go
    CHECK(textsOf(box->variantButtons()) ==
          QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Anna")});
    CHECK(box->variantButtons().at(0)->isChecked());
    CHECK(table->variant().text == QStringLiteral("Anna Wolska"));

    // A card without a name in the dictionary: no table and no "+"
    REQUIRE(pm.changeBookDictionary([&anna](core::BookDictionary& dictionary) {
        dictionary.removeName(dictionary.findNameOfCard(anna)->id);
    }));
    QApplication::processEvents();
    CHECK(box->variantButtons().isEmpty());
    CHECK(table->isHidden());
    CHECK_FALSE(box->addButton()->isEnabled());
    CHECK_FALSE(box->otherNamesLabel()->isHidden());
    press(table, Qt::Key_Insert);
    CHECK(others.size() == 2);
}

TEST_CASE("Card declension: the texts of the book offer Create Card with the kinds of cards",
          "[gui][cards][dictionary]") {
    // The icons of the kinds, also when the test runs alone
    gui::registerAllIcons();
    const PolishNovel novel;
    auto& pm = novel.pm;
    test::Window window;
    gui::BookDictionaryCoordinator dictionary(pm, test::commands(), window.tabs, nullptr,
                                              &window.statusBar);
    const gui::CardNamesCoordinator names(pm, dictionary, window.tabs);
    QStringList titles;
    QStringList kinds;
    QObject::connect(&names, &gui::CardNamesCoordinator::addCardRequested,
                     [&titles, &kinds](const QString& title, const core::KindRef& kind) {
                         titles << title;
                         kinds << kind.kind->id;
                     });

    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("Burza"));
    REQUIRE(window.tabs->count() == 1);
    editor::BookEditor* text = window.editor(0)->getBookEditor();
    QStringList offered;
    QStringList references;
    for (const editor::BookEditor::CardKindChoice& choice : text->cardKinds()) {
        offered << choice.text;
        references << choice.kind;
        CHECK_FALSE(choice.icon.isNull());
    }
    CHECK(offered == QStringList{QStringLiteral("&Character..."), QStringLiteral("&Location..."),
                                 QStringLiteral("&Item...")});
    CHECK(references.first() == baseKind(pm, "character").reference());

    // Create Card of the text: Add Element with the kind and the name
    emit text->createCardAsked(QStringLiteral("Jan Kos"), references.at(1));
    emit text->createCardAsked(QStringLiteral("Nic"), QStringLiteral("missing:kind"));
    CHECK(titles == QStringList{QStringLiteral("Jan Kos")});
    CHECK(kinds == QStringList{QStringLiteral("location")});
}

TEST_CASE("Card declension: the window Relation finds the cards by the forms of their names "
          "and declines them when the program speaks the language of the book",
          "[gui][cards][dictionary][relations]") {
    gui::registerAllIcons();
    const PolishNovel novel;
    auto& pm = novel.pm;
    const CardNames parts;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));
    const QString jan = addCard(pm, "character", QStringLiteral("Jan Wolski"));
    addCard(pm, "location", QStringLiteral("Kraków"));
    DictionaryName name = nameOf(pm, jan);
    NameVariant jasiek;
    jasiek.text = QStringLiteral("Jasiek");
    name.variants.append(jasiek);
    put(pm, name);

    // In English: the names as they are
    {
        const gui::dialogs::RelationDialog dialog(pm, anna, std::nullopt);
        CHECK(shownCards(dialog) == QStringList{QStringLiteral("Jan Wolski"), text("Kraków")});
        dialog.filterEdit()->setText(QStringLiteral("janem"));
        CHECK(shownCards(dialog) == QStringList{QStringLiteral("Jan Wolski")});
        dialog.filterEdit()->setText(QStringLiteral("jasi"));
        CHECK(shownCards(dialog) == QStringList{QStringLiteral("Jan Wolski")});
        dialog.filterEdit()->setText(QStringLiteral("krak"));
        CHECK(shownCards(dialog) == QStringList{text("Kraków")});
        dialog.filterEdit()->setText(QStringLiteral("Jan"));
        CHECK(dialog.previewLabel()->text().startsWith(
            QStringLiteral("On the card \"Anna Wolska\": ")));
    }

    // In the language of the book: the names declined
    gui::setProgramLanguage(QStringLiteral("pl"));
    {
        const gui::dialogs::RelationDialog dialog(pm, anna, std::nullopt);
        dialog.filterEdit()->setText(QStringLiteral("Jan"));
        const QString preview = dialog.previewLabel()->text();
        CHECK(preview.startsWith(text("On the card of Anny Wolskiej: ")));
        CHECK(preview.contains(QStringLiteral("On the card of Jana Wolskiego: ")));

        // A card to create gets the forms the program gives it
        dialog.filterEdit()->setText(QStringLiteral("Marek"));
        CHECK(dialog.previewLabel()->text().contains(QStringLiteral("On the card of Marka: ")));
    }
    gui::setProgramLanguage(QStringLiteral("en"));
}

}  // namespace kalahari
