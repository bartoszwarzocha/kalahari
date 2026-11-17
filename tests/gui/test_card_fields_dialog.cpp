// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_fields_dialog.cpp
/// @brief The window Card Fields: the fields of the cards of each kind, where each is shown,
/// the fields the writer adds and the values of their lists; Fields... of the full card, and
/// an own field of a card made a field of all the cards of its kind

#include "document_window.h"
#include "kalahari_tests/changed_setting.h"
#include "kalahari_tests/polish.h"
#include "test_support/window_parts.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_field_set.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/card_fields_dialog.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/widgets/own_fields_box.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>

#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <functional>
#include <optional>

namespace kalahari {

namespace {

using gui::dialogs::CardChoicesDialog;
using gui::dialogs::CardFieldDialog;
using gui::dialogs::CardFieldsDialog;

/// The kind of the cards of the characters of the Base
constexpr QLatin1String CHARACTER("kalahari.base:character");

/// A new card @p title of kind @p kindId of the Base in the Workshop
QString addCard(core::ProjectManager& pm, const char* kindId, const QString& title) {
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QLatin1String(kindId));
    REQUIRE(kind);
    const QString id = pm.addElement(kind, title, core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// The kind @p kindId of the cards of the Base
core::KindRef baseKind(const core::ProjectManager& pm, const char* kindId) {
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QLatin1String(kindId));
    REQUIRE(kind);
    return kind;
}

/// The row of the table of @p dialog with field @p fieldId; -1 when it has none
int rowOf(const CardFieldsDialog& dialog, const QString& fieldId) {
    const QTableWidget* table = dialog.fieldTable();
    for (int row = CardFieldsDialog::FIXED_ROWS; row < table->rowCount(); ++row) {
        if (table->item(row, CardFieldsDialog::NAME_COLUMN)->data(Qt::UserRole).toString() ==
            fieldId) {
            return row;
        }
    }
    return -1;
}

/// The field @p fieldId of kind @p kind as @p dialog has it
core::BookCardField fieldOf(const CardFieldsDialog& dialog, const QString& kind,
                            const QString& fieldId) {
    const QList<core::BookCardField> fields = dialog.fieldsOf(kind);
    const auto found = std::find_if(fields.cbegin(), fields.cend(),
                                    [&fieldId](const core::BookCardField& each) {
                                        return each.id == fieldId;
                                    });
    REQUIRE(found != fields.cend());
    return *found;
}

/// The ids of the fields of kind @p kind of @p dialog, in their order
QStringList idsOf(const CardFieldsDialog& dialog, const QString& kind) {
    QStringList ids;
    for (const core::BookCardField& field : dialog.fieldsOf(kind)) {
        ids << field.id;
    }
    return ids;
}

/// Run @p command; each window it shows is filled by the next of @p answers, then saved with
/// the button that does its job (a window there is no answer for is closed)
/// @return The number of windows shown
int answeringWith(const std::function<void()>& command,
                  const QList<std::function<void(QDialog&)>>& answers) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &answers]() {
        QDialog* dialog = test::shownDialog();
        if (dialog == nullptr || dialog->property("answered").toBool()) {
            return;
        }
        dialog->setProperty("answered", true);
        const int index = shown++;
        auto* own = qobject_cast<gui::dialogs::KalahariDialog*>(dialog);
        if (index >= answers.size() || own == nullptr) {
            QMetaObject::invokeMethod(dialog, "reject", Qt::QueuedConnection);
            return;
        }
        answers.at(index)(*dialog);
        QMetaObject::invokeMethod(own->acceptButton(), "click", Qt::QueuedConnection);
    });
    timer.start();
    command();
    return shown;
}

/// Press @p key in @p widget
void press(QWidget* widget, int key) {
    QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &down);
}

}  // namespace

TEST_CASE("Card fields window: the fields of a kind, in the order of the card, where each is "
          "shown",
          "[gui][cards][card_fields]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Fields"));
    auto& pm = novel.pm;
    CardFieldsDialog dialog(pm, CHARACTER);
    const QTableWidget* table = dialog.fieldTable();

    // The kinds of the cards of the book, the characters first
    REQUIRE(dialog.kindCombo()->count() == 3);
    CHECK(dialog.kindCombo()->currentText() == QStringLiteral("Character"));
    CHECK(dialog.windowTitle() == QStringLiteral("Card Fields: characters"));

    // The name and In Short, which every card has everywhere, then the fields of the kind
    const core::KindRef character = baseKind(pm, "character");
    REQUIRE(table->rowCount() == CardFieldsDialog::FIXED_ROWS + character.kind->fields.size());
    CHECK(table->item(0, CardFieldsDialog::NAME_COLUMN)->text() == QStringLiteral("Name"));
    CHECK(table->item(1, CardFieldsDialog::NAME_COLUMN)->text() == QStringLiteral("In Short"));
    CHECK_FALSE(table->item(0, CardFieldsDialog::CARD_COLUMN)->flags() & Qt::ItemIsEnabled);
    CHECK(table->item(0, CardFieldsDialog::CARD_COLUMN)->checkState() == Qt::Checked);
    const int role = rowOf(dialog, QStringLiteral("role"));
    CHECK(role == CardFieldsDialog::FIXED_ROWS);
    CHECK(table->item(role, CardFieldsDialog::NAME_COLUMN)->text() == QStringLiteral("Role"));
    CHECK(table->item(role, CardFieldsDialog::TYPE_COLUMN)->text().startsWith(
        QStringLiteral("list: ")));
    const int origin = rowOf(dialog, QStringLiteral("origin"));
    CHECK(table->item(origin, CardFieldsDialog::TYPE_COLUMN)->text() ==
          QStringLiteral("card: location"));
    CHECK(table->item(origin, CardFieldsDialog::CARD_COLUMN)->checkState() == Qt::Unchecked);
    CHECK(table->item(rowOf(dialog, QStringLiteral("appearance")),
                      CardFieldsDialog::TYPE_COLUMN)
              ->text() == QStringLiteral("longer text"));
    CHECK(table->item(rowOf(dialog, QStringLiteral("age")), CardFieldsDialog::TYPE_COLUMN)
              ->text() == QStringLiteral("text"));

    // A field of the kind cannot go, nor its values change; the fields are those of the type
    dialog.setCurrentField(QStringLiteral("age"));
    CHECK_FALSE(dialog.removeButton()->isEnabled());
    CHECK_FALSE(dialog.choicesButton()->isEnabled());
    CHECK_FALSE(dialog.restoreButton()->isEnabled());
    CHECK(dialog.upButton()->isEnabled());
    CHECK(dialog.defaultsCheck() != nullptr);

    SECTION("a box ticked or cleared, with the mouse or Space") {
        const int age = rowOf(dialog, QStringLiteral("age"));
        dialog.fieldTable()->item(age, CardFieldsDialog::LIST_COLUMN)->setCheckState(Qt::Unchecked);
        CHECK_FALSE(fieldOf(dialog, CHARACTER, QStringLiteral("age")).inList);
        CHECK(dialog.restoreButton()->isEnabled());

        dialog.fieldTable()->setCurrentCell(origin, CardFieldsDialog::CARD_COLUMN);
        press(dialog.fieldTable(), Qt::Key_Space);
        CHECK(fieldOf(dialog, CHARACTER, QStringLiteral("origin")).onCard);
        CHECK(table->item(origin, CardFieldsDialog::CARD_COLUMN)->checkState() == Qt::Checked);

        // Back to the fields of the type
        dialog.restoreTypeFields();
        CHECK(dialog.fieldsOf(CHARACTER) == core::card_fields::ofKind(*character.kind));
        CHECK_FALSE(dialog.restoreButton()->isEnabled());
    }

    SECTION("the fields in another order") {
        dialog.setCurrentField(QStringLiteral("age"));
        dialog.moveUp();
        CHECK(idsOf(dialog, CHARACTER).mid(0, 3) ==
              QStringList({QStringLiteral("role"), QStringLiteral("age"),
                           QStringLiteral("gender")}));
        CHECK(table->currentRow() == rowOf(dialog, QStringLiteral("age")));
        dialog.moveUp();
        CHECK(idsOf(dialog, CHARACTER).first() == QStringLiteral("age"));
        CHECK_FALSE(dialog.upButton()->isEnabled());
        dialog.moveDown();
        CHECK(idsOf(dialog, CHARACTER).at(1) == QStringLiteral("age"));
    }

    SECTION("another kind keeps the changes of the first") {
        dialog.fieldTable()
            ->item(rowOf(dialog, QStringLiteral("age")), CardFieldsDialog::CARD_COLUMN)
            ->setCheckState(Qt::Unchecked);
        dialog.showKind(QStringLiteral("kalahari.base:location"));
        CHECK(dialog.windowTitle() == QStringLiteral("Card Fields: locations"));
        CHECK(rowOf(dialog, QStringLiteral("parent")) >= 0);
        CHECK(table->item(rowOf(dialog, QStringLiteral("ruler")), CardFieldsDialog::TYPE_COLUMN)
                  ->text() == QStringLiteral("card: character"));
        dialog.showKind(CHARACTER);
        CHECK_FALSE(fieldOf(dialog, CHARACTER, QStringLiteral("age")).onCard);
    }
}

TEST_CASE("Card fields window: a field of the writer added, changed and taken out",
          "[gui][cards][card_fields]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Fields"));
    auto& pm = novel.pm;
    CardFieldsDialog dialog(pm, CHARACTER);
    const QTableWidget* table = dialog.fieldTable();
    dialog.setCurrentField(QStringLiteral("role"));

    // A list, after the current field; its values at once
    const int shown = answeringWith([&dialog]() { dialog.addField(); },
                                    {[](QDialog& window) {
                                         auto& field = dynamic_cast<CardFieldDialog&>(window);
                                         field.nameEdit()->setText(QStringLiteral("Race"));
                                         field.typeCombo()->setCurrentIndex(2);
                                         CHECK_FALSE(field.cardKindCombo()->isVisible());
                                     },
                                     [](QDialog& window) {
                                         auto& values = dynamic_cast<CardChoicesDialog&>(window);
                                         CHECK(values.windowTitle() ==
                                               QStringLiteral("List Values: Race"));
                                         values.addChoice();
                                         auto* editor = values.list()->findChild<QLineEdit*>();
                                         REQUIRE(editor != nullptr);
                                         editor->setText(QStringLiteral("elf"));
                                         press(editor, Qt::Key_Return);
                                     }});
    CHECK(shown == 2);
    REQUIRE(idsOf(dialog, CHARACTER).at(1) == QStringLiteral("own-1"));
    core::BookCardField race = fieldOf(dialog, CHARACTER, QStringLiteral("own-1"));
    CHECK(race.name == QStringLiteral("Race"));
    CHECK(race.type == core::CardFieldType::Choice);
    CHECK(race.onCard);
    REQUIRE(race.choices.size() == 1);
    CHECK(race.choices.first().id == QStringLiteral("c1"));
    CHECK(race.choices.first().name.text(QStringLiteral("en")) == QStringLiteral("elf"));
    const int row = rowOf(dialog, QStringLiteral("own-1"));
    CHECK(table->currentRow() == row);
    CHECK(table->item(row, CardFieldsDialog::NAME_COLUMN)->data(Qt::UserRole + 1).toBool());
    CHECK(table->item(row, CardFieldsDialog::TYPE_COLUMN)->text() == QStringLiteral("list: elf"));
    CHECK(dialog.removeButton()->isEnabled());
    CHECK(dialog.choicesButton()->isEnabled());

    // A name the cards have is not taken
    answeringWith([&dialog]() { dialog.addField(); },
                  {[](QDialog& window) {
                      auto& field = dynamic_cast<CardFieldDialog&>(window);
                      field.nameEdit()->setText(QStringLiteral(" age "));
                      CHECK_FALSE(field.acceptButton()->isEnabled());
                      CHECK(field.noteLabel()->text().contains(QStringLiteral("already")));
                      field.nameEdit()->setText(QStringLiteral("Mentor"));
                      CHECK(field.acceptButton()->isEnabled());
                      field.typeCombo()->setCurrentIndex(3);
                      field.inverseEdit()->setText(QStringLiteral("Pupil"));
                  }});
    const core::BookCardField mentor = fieldOf(dialog, CHARACTER, QStringLiteral("own-2"));
    CHECK(mentor.type == core::CardFieldType::Card);
    CHECK(mentor.cardKind == QStringLiteral("character"));
    CHECK(mentor.inverseName == QStringLiteral("Pupil"));
    CHECK(table->item(rowOf(dialog, QStringLiteral("own-2")), CardFieldsDialog::TYPE_COLUMN)
              ->text() == QStringLiteral("card: character"));

    // F2 changes it: a list stays a list
    dialog.setCurrentField(QStringLiteral("own-1"));
    answeringWith([&dialog]() { press(dialog.fieldTable(), Qt::Key_F2); },
                  {[](QDialog& window) {
                      auto& field = dynamic_cast<CardFieldDialog&>(window);
                      CHECK(field.windowTitle() == QStringLiteral("Card Field"));
                      CHECK(field.typeCombo()->count() == 1);
                      field.nameEdit()->setText(QStringLiteral("Folk"));
                  }});
    race = fieldOf(dialog, CHARACTER, QStringLiteral("own-1"));
    CHECK(race.name == QStringLiteral("Folk"));
    CHECK(race.choices.size() == 1);

    // Delete takes it out; a new field does not take its id
    press(dialog.fieldTable(), Qt::Key_Delete);
    CHECK_FALSE(idsOf(dialog, CHARACTER).contains(QStringLiteral("own-1")));
    answeringWith([&dialog]() { press(dialog.fieldTable(), Qt::Key_Insert); },
                  {[](QDialog& window) {
                      dynamic_cast<CardFieldDialog&>(window).nameEdit()->setText(
                          QStringLiteral("Habit"));
                  }});
    CHECK(idsOf(dialog, CHARACTER).contains(QStringLiteral("own-3")));

    // The type's fields again: the writer's fields go
    dialog.restoreTypeFields();
    CHECK(dialog.fieldsOf(CHARACTER) ==
          core::card_fields::ofKind(*baseKind(pm, "character").kind));
}

TEST_CASE("Card fields window: the values of a list", "[gui][cards][card_fields]") {
    gui::registerAllIcons();
    const QList<core::CardFieldChoice> choices{
        core::CardFieldChoice{QStringLiteral("c1"),
                              core::LocalizedText::plain(QStringLiteral("elf"))},
        core::CardFieldChoice{QStringLiteral("c2"),
                              core::LocalizedText::plain(QStringLiteral("dwarf"))}};
    CardChoicesDialog dialog(QStringLiteral("Race"), choices);
    REQUIRE(dialog.list()->count() == 2);
    CHECK(dialog.choices() == choices);
    CHECK_FALSE(dialog.upButton()->isEnabled());

    // In another order, one renamed: the values keep their ids
    dialog.list()->setCurrentRow(1);
    dialog.upButton()->click();
    dialog.list()->item(1)->setText(QStringLiteral("high elf"));
    REQUIRE(dialog.choices().size() == 2);
    CHECK(dialog.choices().at(0).id == QStringLiteral("c2"));
    CHECK(dialog.choices().at(1).id == QStringLiteral("c1"));
    CHECK(dialog.choices().at(1).name.text(QString()) == QStringLiteral("high elf"));

    // One goes; a new one does not take its id, and one left without a name is not kept
    dialog.list()->setCurrentRow(0);
    press(dialog.list(), Qt::Key_Delete);
    REQUIRE(dialog.list()->count() == 1);
    dialog.addChoice();
    CHECK(dialog.list()->currentItem()->data(Qt::UserRole).toString() == QStringLiteral("c3"));
    CHECK(dialog.choices().size() == 1);
}

TEST_CASE("Card fields window: saved in the book, and as the fields of new books",
          "[gui][cards][card_fields]") {
    gui::registerAllIcons();
    const test::KeptSetting<nlohmann::json> kept(core::card_fields::DEFAULTS_SETTING);
    const test::OpenNovel novel(QStringLiteral("Fields"));
    auto& pm = novel.pm;
    const core::KindRef character = baseKind(pm, "character");
    const core::KindRef item = baseKind(pm, "item");

    // Nothing changed, nothing saved
    CHECK(test::accepting([&pm]() {
              CHECK_FALSE(gui::dialogs::editCardFields(pm, CHARACTER, nullptr));
          }) == 1);
    CHECK_FALSE(pm.hasOwnCardFields(character));

    // The age hidden, the origin of an item shown; the fields of new books too
    answeringWith(
        [&pm]() { CHECK(gui::dialogs::editCardFields(pm, CHARACTER, nullptr)); },
        {[](QDialog& window) {
            auto& dialog = dynamic_cast<CardFieldsDialog&>(window);
            dialog.fieldTable()
                ->item(rowOf(dialog, QStringLiteral("age")), CardFieldsDialog::CARD_COLUMN)
                ->setCheckState(Qt::Unchecked);
            dialog.showKind(QStringLiteral("kalahari.base:item"));
            dialog.fieldTable()
                ->item(rowOf(dialog, QStringLiteral("origin")), CardFieldsDialog::CARD_COLUMN)
                ->setCheckState(Qt::Checked);
            dialog.defaultsCheck()->setChecked(true);
        }});
    REQUIRE(pm.cardField(character, QStringLiteral("age")) != nullptr);
    CHECK_FALSE(pm.cardField(character, QStringLiteral("age"))->onCard);
    CHECK(pm.cardField(item, QStringLiteral("origin"))->onCard);
    CHECK_FALSE(pm.hasOwnCardFields(baseKind(pm, "location")));
    const core::CardFieldSets defaults = core::card_fields::defaults();
    CHECK(defaults.keys() == QStringList({CHARACTER, QStringLiteral("kalahari.base:item")}));
    CHECK(defaults.value(CHARACTER) == pm.cardFieldSet(character));

    // The fields of new books in the Settings: the same window, without a book
    std::optional<core::CardFieldSets> changed;
    answeringWith(
        [&changed, &defaults]() {
            changed = gui::dialogs::editDefaultCardFields(defaults, nullptr);
        },
        {[](QDialog& window) {
            auto& dialog = dynamic_cast<CardFieldsDialog&>(window);
            CHECK(dialog.windowTitle() == QStringLiteral("Default Fields of New Books"));
            CHECK(dialog.kindCombo()->itemData(0).toString() == CHARACTER);
            CHECK(dialog.kindCombo()->itemData(1).toString() ==
                  QStringLiteral("kalahari.base:location"));
            CHECK(dialog.defaultsCheck() == nullptr);
            CHECK_FALSE(fieldOf(dialog, CHARACTER, QStringLiteral("age")).onCard);
            dialog.restoreTypeFields();
        }});
    REQUIRE(changed.has_value());
    CHECK(changed->keys() == QStringList{QStringLiteral("kalahari.base:item")});
}

namespace {

/// Whether the windows of the fields of the cards fit 1366x768 at 150%: 911 pixels wide and 460
/// high (a Mac's larger controls need a few more)
void checkFieldWindowsFit(const core::ProjectManager& pm, const char* language) {
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 480;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    INFO("In " << language);
    const CardFieldsDialog dialog(pm, CHARACTER);
    INFO("Card Fields " << dialog.sizeHint().width() << "x" << dialog.sizeHint().height());
    CHECK(dialog.sizeHint().width() <= 911);
    CHECK(dialog.sizeHint().height() <= MAX_HEIGHT);
    const CardFieldsDialog defaults(core::CardFieldSets{});
    INFO("Default Fields " << defaults.sizeHint().width() << "x"
                           << defaults.sizeHint().height());
    CHECK(defaults.sizeHint().width() <= 911);
    CHECK(defaults.sizeHint().height() <= MAX_HEIGHT);
    core::BookCardField field;
    field.type = core::CardFieldType::Card;
    const CardFieldDialog one(baseKind(pm, "character"), field, {});
    INFO("Card Field " << one.sizeHint().width() << "x" << one.sizeHint().height());
    CHECK(one.sizeHint().width() <= 911);
    CHECK(one.sizeHint().height() <= MAX_HEIGHT);
    const CardChoicesDialog values(QStringLiteral("Race"), {});
    INFO("List Values " << values.sizeHint().width() << "x" << values.sizeHint().height());
    CHECK(values.sizeHint().width() <= 911);
    CHECK(values.sizeHint().height() <= MAX_HEIGHT);
}

}  // namespace

TEST_CASE("Card fields window: fits a small screen, in English and in Polish",
          "[gui][cards][card_fields]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Fields"));
    checkFieldWindowsFit(novel.pm, "English");
    const test::Polish polish;
    REQUIRE(polish.loaded());
    checkFieldWindowsFit(novel.pm, "Polish");
}

TEST_CASE("Card fields: Fields... of the full card, and an own field made a field of the kind",
          "[gui][cards][card_fields]") {
    gui::registerAllIcons();
    const test::OpenNovel novel(QStringLiteral("Fields"));
    auto& pm = novel.pm;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));
    const QString jan = addCard(pm, "character", QStringLiteral("Jan"));
    REQUIRE(pm.setFields(
        anna, {core::ElementField{QStringLiteral("Race"), QStringLiteral("elf"), {}}}));
    REQUIRE(pm.setFields(
        jan, {core::ElementField{QStringLiteral("race"), QStringLiteral("dwarf"), {}}}));

    gui::CardPage page(pm, true);
    page.showCard(anna);
    REQUIRE(page.fieldsButton() != nullptr);
    CHECK(page.fieldsButton()->isEnabled());
    QStringList said;
    CHECK(test::accepting([&page]() { page.fieldsButton()->click(); }, &said) == 1);
    CHECK(said.join(QLatin1Char('\n')).contains(QStringLiteral("Card Fields: characters")));
    const gui::CardPage shortCard(pm, false);
    CHECK(shortCard.fieldsButton() == nullptr);

    // The menu of the own field: Add to All Cards of This Kind
    QTableWidget* table = page.ownFields()->table();
    REQUIRE(table->rowCount() == 1);
    table->setCurrentCell(0, 1);
    emit table->customContextMenuRequested(table->visualItemRect(table->item(0, 0)).center());
    QMenu* menu = page.ownFields()->menu();
    QAction* toKind = nullptr;
    for (QAction* action : menu->actions()) {
        if (action->text() == QStringLiteral("Add to &All Cards of This Kind")) {
            toKind = action;
        }
    }
    menu->hide();
    REQUIRE(toKind != nullptr);
    CHECK(toKind->isEnabled());
    toKind->trigger();
    const core::CardField* race = pm.cardField(baseKind(pm, "character"), QStringLiteral("own-1"));
    REQUIRE(race != nullptr);
    CHECK(race->name.text(QStringLiteral("en")) == QStringLiteral("Race"));
    CHECK(pm.findElement(jan)->card.value(QStringLiteral("own-1")) == QStringLiteral("dwarf"));
    // The card shows it among the fields of its kind, without the own field
    auto* editor = qobject_cast<QLineEdit*>(page.fieldEditor(QStringLiteral("own-1")));
    REQUIRE(editor != nullptr);
    CHECK(editor->text() == QStringLiteral("elf"));
    CHECK(table->rowCount() == 0);
}

}  // namespace kalahari
