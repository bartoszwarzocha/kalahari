// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_relation_dialog.cpp
/// @brief The window Relation and the relations of a card: the other card chosen or made, the
/// relation between the kinds of the two cards or one the writer names, and the list of the
/// relations on the short and the full card with its keys and its menu

#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_relations.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/relation_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/widgets/relations_box.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <functional>
#include <optional>

namespace kalahari {

namespace {

using gui::dialogs::MessageDialog;
using gui::dialogs::RelationChoice;
using gui::dialogs::RelationDialog;

/// A new card @p title of kind @p kindId of the Base in the Workshop, of gender @p gender
/// (empty: not said)
QString addCard(core::ProjectManager& pm, const char* kindId, const QString& title,
                const QString& gender = QString()) {
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QLatin1String(kindId));
    REQUIRE(kind);
    const QString id = pm.addElement(kind, title, core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    if (!gender.isEmpty()) {
        REQUIRE(pm.setCardValue(id, QStringLiteral("gender"), gender));
    }
    return id;
}

/// The cards of the book of the test: Anna, Jan, Alex, Kraków and the Sword
struct Cards {
    QString anna;
    QString jan;
    QString alex;
    QString krakow;
    QString sword;

    explicit Cards(core::ProjectManager& pm)
        : anna(addCard(pm, "character", QStringLiteral("Anna"), QStringLiteral("woman"))),
          jan(addCard(pm, "character", QStringLiteral("Jan"), QStringLiteral("man"))),
          alex(addCard(pm, "character", QStringLiteral("Alex"))),
          krakow(addCard(pm, "location", QStringLiteral("Kraków"))),
          sword(addCard(pm, "item", QStringLiteral("Sword"))) {}
};

/// Key @p key with @p text sent to @p widget, as it gets it when it has the keys
void press(QWidget* widget, Qt::Key key, const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, text);
    QCoreApplication::sendEvent(widget, &event);
}

/// The items of @p list that are shown, the cards under their groups after two spaces
QStringList shownItems(const QTreeWidget* list) {
    QStringList texts;
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* item = list->topLevelItem(i);
        if (item->isHidden()) {
            continue;
        }
        texts << item->text(0);
        for (int j = 0; j < item->childCount(); ++j) {
            if (!item->child(j)->isHidden()) {
                texts << QStringLiteral("  ") + item->child(j)->text(0);
            }
        }
    }
    return texts;
}

/// The texts of the labels of @p dialog
QStringList labelsOf(const QDialog& dialog) {
    QStringList texts;
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        texts << label->text();
    }
    return texts;
}

/// The relations @p combo offers, without its lines
QStringList relationsOf(const QComboBox* combo) {
    QStringList texts;
    for (int i = 0; i < combo->count(); ++i) {
        if (!combo->itemText(i).isEmpty()) {
            texts << combo->itemText(i);
        }
    }
    return texts;
}

/// The rows of @p list, each "relation | card | note", the groups as they are
QStringList rowsOf(const QTreeWidget* list) {
    QStringList rows;
    const auto row = [list](const QTreeWidgetItem* item) {
        QStringList columns;
        for (int column = 0; column < list->columnCount(); ++column) {
            columns << item->text(column);
        }
        return columns.join(QStringLiteral(" | "));
    };
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* item = list->topLevelItem(i);
        rows << (item->childCount() > 0 ? item->text(0) : row(item));
        for (int j = 0; j < item->childCount(); ++j) {
            rows << QStringLiteral("  ") + row(item->child(j));
        }
    }
    return rows;
}

/// The texts of the actions of @p menu, a line as an empty text, "(off)" after those that do
/// nothing
QStringList actionsOf(const QMenu* menu) {
    QStringList texts;
    for (const QAction* action : menu->actions()) {
        texts << (action->isEnabled() || action->isSeparator()
                      ? action->text()
                      : action->text() + QStringLiteral(" (off)"));
    }
    return texts;
}

/// The row of @p list whose card column is @p title
QTreeWidgetItem* rowOf(const QTreeWidget* list, const QString& title) {
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = list->topLevelItem(i);
        if (item->childCount() == 0 && item->text(1) == title) {
            return item;
        }
        for (int j = 0; j < item->childCount(); ++j) {
            if (item->child(j)->text(1) == title) {
                return item->child(j);
            }
        }
    }
    return nullptr;
}

/// The relations card @p cardId shows: "relation: card (note)", in their order
QStringList shownOf(const core::ProjectManager& pm, const QString& cardId) {
    const core::CardRelations relations = pm.cardRelations();
    const core::ProjectElement* card = pm.findElement(cardId);
    REQUIRE(card != nullptr);
    QStringList shown;
    for (const core::ShownRelation& relation : relations.of(cardId)) {
        const core::ProjectElement* other = pm.findElement(relation.otherId);
        REQUIRE(other != nullptr);
        QString text = relations.labelOf(relation.relation, relations.kindOf(*card), *other,
                                         QStringLiteral("en")) +
                       QStringLiteral(": ") + other->title;
        if (!relation.note.isEmpty()) {
            text += QStringLiteral(" (") + relation.note + QLatin1Char(')');
        }
        shown << text;
    }
    return shown;
}

/// Run @p command; in each window Relation it opens, @p choose chooses, then the window saves
/// when it can, and closes otherwise
/// @return The number of windows Relation the command opened
int relating(const std::function<void()>& command,
             const std::function<void(RelationDialog&)>& choose) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &choose]() {
        auto* dialog = qobject_cast<RelationDialog*>(test::shownDialog());
        if (dialog == nullptr || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        ++shown;
        choose(*dialog);
        if (dialog->acceptButton()->isEnabled()) {
            dialog->acceptButton()->click();
        } else {
            dialog->reject();
        }
    });
    timer.start();
    command();
    return shown;
}

}  // namespace

TEST_CASE("Relation: the cards in groups, the filter, the cards it makes and the relations "
          "between the two kinds",
          "[gui][cards][relations]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const Cards cards(pm);

    RelationDialog dialog(kalahari::test::book(), cards.anna, std::nullopt);
    CHECK(dialog.objectName() == QStringLiteral("relationDialog"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("&Add"));
    gui::dialogs::RowList* list = dialog.cardList();

    // The other cards in the groups of their kinds, by name; the first one is chosen
    CHECK(shownItems(list) ==
          QStringList{QStringLiteral("Characters"), QStringLiteral("  Alex"),
                      QStringLiteral("  Jan"), QStringLiteral("Locations"),
                      QStringLiteral("  Kraków"), QStringLiteral("Items"),
                      QStringLiteral("  Sword")});
    REQUIRE(list->currentItem() != nullptr);
    CHECK(list->currentItem()->text(0) == QStringLiteral("Alex"));
    CHECK(dialog.choice().otherId == cards.alex);

    // The relations between two characters with their groups, then the writer's own
    const QStringList people = relationsOf(dialog.relationCombo());
    REQUIRE(people.size() > 3);
    CHECK(people.mid(0, 3) == QStringList{QStringLiteral("parent (family)"),
                                          QStringLiteral("child (family)"),
                                          QStringLiteral("siblings (family)")});
    CHECK(people.contains(QStringLiteral("friendship (other)")));
    CHECK(people.last() == QStringLiteral("Own Relation..."));
    CHECK_FALSE(dialog.nameEdit()->isVisibleTo(&dialog));

    // Both cards show it, each with the name by the gender of the other one
    dialog.setCurrentCard(cards.jan);
    dialog.setCurrentRelation(QStringLiteral("sibling"));
    CHECK(dialog.previewLabel()->text() ==
          QStringLiteral("On the card \"Anna\": <b>brother</b> – Jan<br>"
                         "On the card \"Jan\": <b>sister</b> – Anna"));
    dialog.noteEdit()->setText(QStringLiteral(" twins "));
    CHECK(dialog.acceptButton()->isEnabled());
    const RelationChoice sibling = dialog.choice();
    CHECK(sibling.otherId == cards.jan);
    CHECK(sibling.relationId == QStringLiteral("sibling"));
    CHECK(sibling.note == QStringLiteral("twins"));
    CHECK(sibling.newKind.isEmpty());

    // A location: the relations of a character with a location, without groups
    dialog.setCurrentCard(cards.krakow);
    const QStringList places = relationsOf(dialog.relationCombo());
    REQUIRE_FALSE(places.isEmpty());
    CHECK(places.first() == QStringLiteral("lives in"));
    CHECK_FALSE(places.contains(QStringLiteral("siblings (family)")));
    CHECK(places.last() == QStringLiteral("Own Relation..."));
    CHECK(dialog.previewLabel()->text() ==
          QStringLiteral("On the card \"Anna\": <b>home</b> – Kraków<br>"
                         "On the card \"Kraków\": <b>lives here</b> – Anna"));

    // Typing filters the cards; for a name no card of a kind has, it can make one
    dialog.filterEdit()->setText(QStringLiteral("an"));
    CHECK(shownItems(list) ==
          QStringList{QStringLiteral("Characters"), QStringLiteral("  Jan"),
                      QStringLiteral("Create character \"an\"..."),
                      QStringLiteral("Create location \"an\"..."),
                      QStringLiteral("Create item \"an\"...")});
    CHECK(list->currentItem()->text(0) == QStringLiteral("Jan"));
    // A card of the same name, whatever its letters, is not made again
    dialog.filterEdit()->setText(QStringLiteral("jan"));
    CHECK(shownItems(list) ==
          QStringList{QStringLiteral("Characters"), QStringLiteral("  Jan"),
                      QStringLiteral("Create location \"jan\"..."),
                      QStringLiteral("Create item \"jan\"...")});

    // A new card: the relations of its kind, and the relation the new card shows
    dialog.filterEdit()->setText(QStringLiteral("Zofia"));
    CHECK(shownItems(list) == QStringList{QStringLiteral("Create character \"Zofia\"..."),
                                          QStringLiteral("Create location \"Zofia\"..."),
                                          QStringLiteral("Create item \"Zofia\"...")});
    CHECK(list->currentItem()->text(0) == QStringLiteral("Create character \"Zofia\"..."));
    CHECK(dialog.relationCombo()->currentText() == QStringLiteral("parent (family)"));
    CHECK(dialog.previewLabel()->text() ==
          QStringLiteral("On the card \"Anna\": <b>parent</b> – Zofia<br>"
                         "On the card \"Zofia\": <b>daughter</b> – Anna"));
    const RelationChoice made = dialog.choice();
    CHECK(made.otherId.isEmpty());
    CHECK(made.newTitle == QStringLiteral("Zofia"));
    CHECK(made.newKind == QStringLiteral("kalahari.base:character"));
    CHECK(made.relationId == QStringLiteral("parent"));

    // The writer's relation: a name for each card, both needed
    dialog.filterEdit()->clear();
    dialog.setCurrentCard(cards.jan);
    QComboBox* combo = dialog.relationCombo();
    combo->setCurrentIndex(combo->count() - 1);
    CHECK(dialog.nameEdit()->isVisibleTo(&dialog));
    CHECK(dialog.inverseEdit()->isVisibleTo(&dialog));
    CHECK(labelsOf(dialog).contains(QStringLiteral("On the card \"Anna\":")));
    CHECK(labelsOf(dialog).contains(QStringLiteral("On the card \"Jan\":")));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    dialog.nameEdit()->setText(QStringLiteral("creditor"));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    dialog.inverseEdit()->setText(QStringLiteral("debtor"));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.previewLabel()->text() ==
          QStringLiteral("On the card \"Anna\": <b>creditor</b> – Jan<br>"
                         "On the card \"Jan\": <b>debtor</b> – Anna"));
    const RelationChoice own = dialog.choice();
    CHECK(own.relationId.isEmpty());
    CHECK(own.name == QStringLiteral("creditor"));
    CHECK(own.inverseName == QStringLiteral("debtor"));
    // Another card keeps the writer's relation chosen
    dialog.setCurrentCard(cards.alex);
    CHECK(combo->currentText() == QStringLiteral("Own Relation..."));
    CHECK(labelsOf(dialog).contains(QStringLiteral("On the card \"Alex\":")));

    // It fits a small screen, 1366x768 at 150%: 911 pixels wide and 460 high (a Mac's larger
    // controls need a few more); only the list scrolls
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 480;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    for (int i = 0; i < 30; ++i) {
        addCard(pm, "character", QStringLiteral("Guest %1").arg(i));
    }
    const RelationDialog crowded(kalahari::test::book(), cards.anna, std::nullopt);
    INFO("size " << crowded.sizeHint().width() << "x" << crowded.sizeHint().height());
    CHECK(crowded.sizeHint().width() <= 911);
    CHECK(crowded.sizeHint().height() <= MAX_HEIGHT);
}

TEST_CASE("Relation: the letters typed in the list go to the filter, the arrows of the "
          "filter choose in the list",
          "[gui][cards][relations]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    const Cards cards(novel.pm);

    RelationDialog dialog(kalahari::test::book(), cards.anna, std::nullopt);
    gui::dialogs::RowList* list = dialog.cardList();
    press(list, Qt::Key_K, QStringLiteral("k"));
    press(list, Qt::Key_R, QStringLiteral("r"));
    CHECK(dialog.filterEdit()->text() == QStringLiteral("kr"));
    CHECK(list->currentItem()->text(0) == QStringLiteral("Kraków"));
    press(list, Qt::Key_Backspace);
    CHECK(dialog.filterEdit()->text() == QStringLiteral("k"));

    // Down in the filter goes to the next item of the list
    dialog.filterEdit()->clear();
    CHECK(list->currentItem()->text(0) == QStringLiteral("Kraków"));
    dialog.setCurrentCard(cards.alex);
    press(dialog.filterEdit(), Qt::Key_Down);
    CHECK(list->currentItem()->text(0) == QStringLiteral("Jan"));
    CHECK(dialog.choice().otherId == cards.jan);
}

TEST_CASE("Relation: a relation changed keeps its card, its relation and its note",
          "[gui][cards][relations]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const Cards cards(pm);
    REQUIRE(pm.addRelation(cards.anna, cards.jan, QStringLiteral("sibling"),
                           QStringLiteral("twins")));

    // Jan's card shows it too: Anna is his sister
    const QList<core::ShownRelation> shown = pm.cardRelations().of(cards.jan);
    REQUIRE(shown.size() == 1);
    const RelationDialog dialog(kalahari::test::book(), cards.jan, shown.first());
    CHECK(dialog.acceptButton()->text() == QStringLiteral("&Change"));
    REQUIRE(dialog.cardList()->currentItem() != nullptr);
    CHECK(dialog.cardList()->currentItem()->text(0) == QStringLiteral("Anna"));
    CHECK(dialog.relationCombo()->currentText() == QStringLiteral("siblings (family)"));
    CHECK(dialog.noteEdit()->text() == QStringLiteral("twins"));
    CHECK(dialog.previewLabel()->text() ==
          QStringLiteral("On the card \"Jan\": <b>sister</b> – Anna<br>"
                         "On the card \"Anna\": <b>brother</b> – Jan"));
}

TEST_CASE("Relation: saved with the card it makes, with the relation the writer names, and in "
          "place of the one it changes",
          "[gui][cards][relations]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const Cards cards(pm);

    // A relation with a card of the book
    RelationChoice sibling;
    sibling.otherId = cards.jan;
    sibling.relationId = QStringLiteral("sibling");
    sibling.note = QStringLiteral("twins");
    CHECK(gui::dialogs::saveRelation(pm, cards.anna, sibling, std::nullopt, nullptr) == cards.jan);
    CHECK(shownOf(pm, cards.anna) == QStringList{QStringLiteral("brother: Jan (twins)")});

    // A new card, made in the Workshop with the relation
    RelationChoice parent;
    parent.newTitle = QStringLiteral("Zofia");
    parent.newKind = QStringLiteral("kalahari.base:character");
    parent.relationId = QStringLiteral("parent");
    const QString zofia = gui::dialogs::saveRelation(pm, cards.anna, parent, std::nullopt, nullptr);
    REQUIRE_FALSE(zofia.isEmpty());
    const core::ProjectElement* made = pm.findElement(zofia);
    REQUIRE(made != nullptr);
    CHECK(made->title == QStringLiteral("Zofia"));
    CHECK(pm.cardRelations().kindOf(*made) == QStringLiteral("kalahari.base:character"));
    CHECK(shownOf(pm, zofia) == QStringList{QStringLiteral("daughter: Anna")});

    // The writer's relation is kept in the book, then the relation is saved with it
    RelationChoice hideout;
    hideout.otherId = cards.krakow;
    hideout.name = QStringLiteral("hideout");
    hideout.inverseName = QStringLiteral("hides here");
    CHECK(gui::dialogs::saveRelation(pm, cards.anna, hideout, std::nullopt, nullptr) ==
          cards.krakow);
    REQUIRE(pm.project() != nullptr);
    REQUIRE(pm.project()->workshop.relations.size() == 2);
    CHECK(shownOf(pm, cards.anna).contains(QStringLiteral("hideout: Kraków")));
    CHECK(shownOf(pm, cards.krakow) == QStringList{QStringLiteral("hides here: Anna")});

    // In place of the relation it changes
    const QList<core::ShownRelation> before = pm.cardRelations().of(cards.anna);
    REQUIRE_FALSE(before.isEmpty());
    REQUIRE(before.first().otherId == cards.jan);
    RelationChoice friends;
    friends.otherId = cards.jan;
    friends.relationId = QStringLiteral("friend");
    CHECK(gui::dialogs::saveRelation(pm, cards.anna, friends, before.first(), nullptr) ==
          cards.jan);
    CHECK(shownOf(pm, cards.jan) == QStringList{QStringLiteral("friend: Anna")});

    // A card that cannot be made is said so, and nothing is saved
    RelationChoice wrong;
    wrong.newTitle = QStringLiteral("Nobody");
    wrong.newKind = QStringLiteral("kalahari.base:nothing");
    wrong.relationId = QStringLiteral("friend");
    QStringList said;
    QString saved;
    CHECK(test::answering(
              [&]() {
                  saved =
                      gui::dialogs::saveRelation(pm, cards.anna, wrong, std::nullopt, nullptr);
              },
              MessageDialog::Answer::Accept, said) == 1);
    CHECK(saved.isEmpty());
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("Card Not Added")));
}

TEST_CASE("Relations: the short and the full card list them, and change them with the keys, "
          "the buttons and the menu",
          "[gui][cards][relations]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const Cards cards(pm);
    REQUIRE(pm.addRelation(cards.anna, cards.jan, QStringLiteral("sibling"),
                           QStringLiteral("twins")));
    REQUIRE(pm.addRelation(cards.anna, cards.krakow, QStringLiteral("lives_in"), QString()));

    gui::RelationsBox brief(kalahari::test::book(), false);
    gui::RelationsBox full(kalahari::test::book(), true);
    QStringList shown;
    QStringList added;
    for (const gui::RelationsBox* box : {&brief, &full}) {
        QObject::connect(box, &gui::RelationsBox::requestShowCard,
                         [&shown](const QString& id) { shown << id; });
        QObject::connect(box, &gui::RelationsBox::cardAdded,
                         [&added](const QString& id) { added << id; });
    }
    brief.showCard(cards.anna);
    full.showCard(cards.anna);
    CHECK(brief.title() == QStringLiteral("Relations"));

    // The short card: the relation and the card; the note in the tooltip
    CHECK(rowsOf(brief.list()) == QStringList{QStringLiteral("brother | Jan"),
                                              QStringLiteral("home | Kraków")});
    CHECK(rowOf(brief.list(), QStringLiteral("Jan"))->toolTip(0) == QStringLiteral("twins"));
    CHECK(brief.list()->isVisibleTo(&brief));
    CHECK_FALSE(brief.emptyLabel()->isVisibleTo(&brief));
    CHECK_FALSE(rowOf(brief.list(), QStringLiteral("Jan"))->icon(1).isNull());
    // The full card: in the groups of the kinds, with the notes
    CHECK(rowsOf(full.list()) == QStringList{QStringLiteral("Characters"),
                                             QStringLiteral("  brother | Jan | twins"),
                                             QStringLiteral("Locations"),
                                             QStringLiteral("  home | Kraków | ")});

    // The other card shows it by itself
    gui::RelationsBox other(kalahari::test::book(), false);
    other.showCard(cards.jan);
    CHECK(rowsOf(other.list()) == QStringList{QStringLiteral("sister | Anna")});
    // A card without relations says how to add one
    other.showCard(cards.sword);
    CHECK(other.list()->topLevelItemCount() == 0);
    CHECK_FALSE(other.list()->isVisibleTo(&other));
    CHECK(other.emptyLabel()->isVisibleTo(&other));
    CHECK(other.emptyLabel()->text() == QStringLiteral("No relations. \"+\" or Insert adds one."));

    // The names, the genders and the relations changed elsewhere are shown at once; the current
    // row stays
    QTreeWidget* list = brief.list();
    list->setCurrentItem(rowOf(list, QStringLiteral("Jan")));
    REQUIRE(pm.setCardValue(cards.jan, QStringLiteral("gender"), QStringLiteral("woman")));
    REQUIRE(pm.renameElement(cards.jan, QStringLiteral("Janina")));
    CHECK(rowsOf(list) == QStringList{QStringLiteral("sister | Janina"),
                                      QStringLiteral("home | Kraków")});
    REQUIRE(list->currentItem() != nullptr);
    CHECK(list->currentItem()->text(1) == QStringLiteral("Janina"));
    CHECK(brief.removeButton()->isEnabled());

    // Enter and a double-click show the other card
    press(list, Qt::Key_Return);
    emit list->itemDoubleClicked(list->currentItem(), 0);
    CHECK(shown == QStringList{cards.jan, cards.jan});

    // The keys of the list wait the window's commands
    QKeyEvent overrideKey(QEvent::ShortcutOverride, Qt::Key_Delete, Qt::NoModifier);
    overrideKey.ignore();
    QCoreApplication::sendEvent(list, &overrideKey);
    CHECK(overrideKey.isAccepted());

    // The menu of a row has it all
    emit list->customContextMenuRequested(QPoint(4, 4));
    CHECK(actionsOf(brief.rowMenu()) ==
          QStringList{QStringLiteral("Show the Card"), QStringLiteral("Change the Relation... (F2)"),
                      QStringLiteral("Delete Relation (Delete)"), QString(),
                      QStringLiteral("New Relation... (Insert)")});
    brief.rowMenu()->close();
    full.list()->setCurrentItem(nullptr);
    emit full.list()->customContextMenuRequested(QPoint(4, 4));
    CHECK(actionsOf(full.rowMenu()) ==
          QStringList{QStringLiteral("Open the Card (off)"),
                      QStringLiteral("Change the Relation... (F2) (off)"),
                      QStringLiteral("Delete Relation (Delete) (off)"), QString(),
                      QStringLiteral("New Relation... (Insert)")});
    full.rowMenu()->close();
    CHECK_FALSE(full.removeButton()->isEnabled());

    // Delete: a relation with a note asks first
    QStringList said;
    CHECK(test::answering([list]() { press(list, Qt::Key_Delete); },
                          MessageDialog::Answer::Cancel, said) == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(
        QStringLiteral("Delete the relation \"sister – Janina\" with its note?")));
    CHECK(pm.cardRelations().of(cards.anna).size() == 2);
    // A relation without a note goes at once
    list->setCurrentItem(rowOf(list, QStringLiteral("Kraków")));
    said.clear();
    CHECK(test::answering([list]() { press(list, Qt::Key_Delete); },
                          MessageDialog::Answer::Cancel, said) == 0);
    CHECK(rowsOf(list) == QStringList{QStringLiteral("sister | Janina")});
    CHECK(rowsOf(full.list()) == QStringList{QStringLiteral("Characters"),
                                             QStringLiteral("  sister | Janina | twins")});

    // Insert: a new relation, the new row current
    CHECK(relating([list]() { press(list, Qt::Key_Insert); },
                   [&cards](RelationDialog& dialog) {
                       dialog.setCurrentCard(cards.sword);
                       dialog.setCurrentRelation(QStringLiteral("property"));
                   }) == 1);
    CHECK(rowsOf(list).contains(QStringLiteral("property | Sword")));
    REQUIRE(list->currentItem() != nullptr);
    CHECK(list->currentItem()->text(1) == QStringLiteral("Sword"));
    CHECK(added.isEmpty());

    // F2: the window for the current relation
    CHECK(relating([list]() { press(list, Qt::Key_F2); },
                   [](RelationDialog& dialog) {
                       CHECK(dialog.acceptButton()->text() == QStringLiteral("&Change"));
                       CHECK(dialog.cardList()->currentItem()->text(0) ==
                             QStringLiteral("Sword"));
                       dialog.noteEdit()->setText(QStringLiteral("a gift"));
                   }) == 1);
    CHECK(rowsOf(full.list()).contains(QStringLiteral("  property | Sword | a gift")));

    // "+": a relation with a card made in the window; the Navigator shows the new card
    CHECK(relating([&brief]() { brief.addButton()->click(); },
                   [](RelationDialog& dialog) {
                       dialog.filterEdit()->setText(QStringLiteral("Zofia"));
                       dialog.setCurrentRelation(QStringLiteral("friend"));
                   }) == 1);
    REQUIRE(added.size() == 1);
    REQUIRE(pm.findElement(added.first()) != nullptr);
    CHECK(pm.findElement(added.first())->title == QStringLiteral("Zofia"));
    CHECK(rowsOf(list).contains(QStringLiteral("friend | Zofia")));

    // A window closed changes nothing
    const QStringList before = rowsOf(list);
    CHECK(relating([list]() { press(list, Qt::Key_Insert); },
                   [](RelationDialog& dialog) {
                       dialog.filterEdit()->setText(QStringLiteral("Nobody"));
                       QComboBox* combo = dialog.relationCombo();
                       combo->setCurrentIndex(combo->count() - 1);
                   }) == 1);
    CHECK(rowsOf(list) == before);
    CHECK(added.size() == 1);
}

TEST_CASE("Card: its relations, with the button Relation of the full card; the other card "
          "is shown or opened",
          "[gui][cards][relations]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const Cards cards(pm);

    gui::CardPage full(kalahari::test::book(), true);
    gui::CardPage brief(kalahari::test::book(), false);
    QStringList opened;
    QStringList selected;
    QStringList added;
    for (const gui::CardPage* page : {&full, &brief}) {
        QObject::connect(page, &gui::CardPage::requestOpenCard,
                         [&opened](const QString& id) { opened << id; });
        QObject::connect(page, &gui::CardPage::requestSelectCard,
                         [&selected](const QString& id) { selected << id; });
        QObject::connect(page, &gui::CardPage::cardAdded,
                         [&added](const QString& id) { added << id; });
    }
    full.showCard(cards.anna);
    brief.showCard(cards.anna);
    REQUIRE(full.relationButton() != nullptr);
    CHECK(brief.relationButton() == nullptr);
    CHECK(full.relationButton()->text() == QStringLiteral("Relation..."));

    CHECK(relating([&full]() { full.relationButton()->click(); },
                   [](RelationDialog& dialog) {
                       dialog.filterEdit()->setText(QStringLiteral("Marek"));
                       dialog.setCurrentRelation(QStringLiteral("enemy"));
                   }) == 1);
    REQUIRE(added.size() == 1);
    const QString marek = added.first();
    CHECK(rowsOf(full.relationsBox()->list()) ==
          QStringList{QStringLiteral("Characters"), QStringLiteral("  enemy | Marek | ")});
    CHECK(rowsOf(brief.relationsBox()->list()) == QStringList{QStringLiteral("enemy | Marek")});

    // Enter on the full card opens the other card; on the short card it is chosen in the
    // Navigator
    QTreeWidget* fullList = full.relationsBox()->list();
    fullList->setCurrentItem(rowOf(fullList, QStringLiteral("Marek")));
    press(fullList, Qt::Key_Return);
    QTreeWidget* briefList = brief.relationsBox()->list();
    briefList->setCurrentItem(rowOf(briefList, QStringLiteral("Marek")));
    press(briefList, Qt::Key_Return);
    CHECK(opened == QStringList{marek});
    CHECK(selected == QStringList{marek});
}

}  // namespace kalahari
