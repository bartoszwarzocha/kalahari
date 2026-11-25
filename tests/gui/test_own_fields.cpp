// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_own_fields.cpp
/// @brief The writer's own fields of a book and of its series: the model, the texts, the
///        deletion with Undo, the Properties panel and quick insert

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "kalahari_tests/gui/document_window.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/quick_insert.h"
#include "kalahari/editor/text_fields.h"
#include "kalahari/gui/field_coordinator.h"
#include "kalahari/gui/own_fields.h"
#include "kalahari/gui/panels/book_fields_box.h"
#include "kalahari/gui/quick_insert_items.h"
#include "kalahari_tests/test_support/window_parts.h"

#include <QCoreApplication>
#include <QFocusEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextDocument>
#include <QToolButton>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace kalahari {

using gui::FieldCoordinator;
using gui::OwnField;
using test::OpenNovel;
using test::Window;
namespace own_fields = gui::own_fields;

namespace {

/// A field of the book
OwnField bookField(const QString& name, const QString& value,
                   const QString& description = QString()) {
    OwnField field;
    field.id = own_fields::newId();
    field.name = name;
    field.value = value;
    field.description = description;
    return field;
}

/// A field of the series, the same in all the volumes
OwnField seriesField(const QString& name, const QString& value) {
    OwnField field = bookField(name, value);
    field.series = true;
    field.seriesValue = value;
    return field;
}

/// An own field in the text
editor::TextField textField(const OwnField& field) {
    return {.type = editor::FieldType::Custom, .format = QString(), .key = field.id,
            .name = field.name};
}

/// The fields of the open texts, as the main window has them
struct Fields {
    editor::BookEditor* keys = nullptr;
    FieldCoordinator coordinator;

    explicit Fields(Window& window)
        : coordinator(kalahari::test::book(), kalahari::test::commands(), window.tabs,
                      [this]() { return keys; }, &window.window) {
        coordinator.connectCommands();
    }
};

/// The texts the fields of an editor show, parted by "|"
std::string shownFields(const editor::BookEditor& text) {
    const QTextDocument& document = *text.textDocument();
    QStringList texts;
    for (int position = 0; position + 1 < document.characterCount(); ++position) {
        if (const QString shown = text.fieldTextAt(position); !shown.isEmpty()) {
            texts << shown;
        }
    }
    return texts.join(QLatin1Char('|')).toStdString();
}

}  // namespace

TEST_CASE("Own fields: a book's fields made, changed and taken away", "[gui][fields][own]") {
    const OpenNovel book(QStringLiteral("Sand"));
    core::ProjectManager& pm = book.pm;
    CHECK(own_fields::of(pm).isEmpty());

    const OwnField narrator = bookField(QStringLiteral("Narrator"), QStringLiteral("Ilias"),
                                        QStringLiteral("Who tells the story"));
    REQUIRE(own_fields::add(pm, narrator));
    const OwnField city = bookField(QStringLiteral("City"), QString());
    REQUIRE(own_fields::add(pm, city));
    // By name; an id taken, and a field of a series outside one, are not added
    QList<OwnField> fields = own_fields::of(pm);
    REQUIRE(fields.size() == 2);
    CHECK(fields[0].name == QStringLiteral("City"));
    CHECK(fields[1] == narrator);
    CHECK_FALSE(own_fields::add(pm, narrator));
    CHECK_FALSE(own_fields::add(pm, seriesField(QStringLiteral("Publisher"), QString())));
    CHECK(own_fields::newId() != own_fields::newId());

    // The names, case aside
    CHECK(own_fields::isNameTaken(pm, QStringLiteral(" narrator ")));
    CHECK_FALSE(own_fields::isNameTaken(pm, QStringLiteral("Narrator"), narrator.id));
    CHECK_FALSE(own_fields::isNameTaken(pm, QStringLiteral("Weather")));

    // Changed: the name, the value, the description; the value alone (the Properties panel)
    OwnField changed = narrator;
    changed.name = QStringLiteral(" Teller ");
    changed.description = QStringLiteral("The voice");
    REQUIRE(own_fields::change(pm, changed));
    REQUIRE(own_fields::setValue(pm, narrator.id, QStringLiteral("Hilary")));
    const std::optional<OwnField> found = own_fields::find(pm, narrator.id);
    REQUIRE(found);
    CHECK(found->name == QStringLiteral("Teller"));
    CHECK(found->value == QStringLiteral("Hilary"));
    CHECK(found->description == QStringLiteral("The voice"));
    CHECK_FALSE(own_fields::setValue(pm, QStringLiteral("field-none"), QStringLiteral("x")));

    // What the texts of the book show
    const QMap<QString, editor::CustomFieldValue> values =
        own_fields::values(pm, *pm.book());
    CHECK(values.value(narrator.id).value == QStringLiteral("Hilary"));
    CHECK(values.value(city.id).name == QStringLiteral("City"));

    REQUIRE(own_fields::remove(pm, city.id));
    CHECK_FALSE(own_fields::remove(pm, city.id));
    CHECK(own_fields::of(pm).size() == 1);
}

TEST_CASE("Own fields: in a series a field of all the volumes, a volume with its own value",
          "[gui][fields][own][series]") {
    OpenNovel book(QStringLiteral("Sand"));
    core::ProjectManager& pm = book.pm;
    const QString first = pm.book()->id;
    core::NewBook request;
    request.title = QStringLiteral("Dunes");
    const QString second = pm.addBook(request);
    REQUIRE_FALSE(second.isEmpty());
    REQUIRE(pm.isSeries());

    OwnField city = seriesField(QStringLiteral("City"), QStringLiteral("Tau"));
    REQUIRE(own_fields::add(pm, city, first));
    REQUIRE(own_fields::add(pm, bookField(QStringLiteral("Narrator"), QStringLiteral("Ilias")),
                            first));
    // The book's own first, then the series'; the other volume has only the series'
    QList<OwnField> fields = own_fields::of(pm, first);
    REQUIRE(fields.size() == 2);
    CHECK(fields[0].name == QStringLiteral("Narrator"));
    CHECK(fields[1].series);
    REQUIRE(own_fields::of(pm, second).size() == 1);
    CHECK(own_fields::of(pm, second)[0].value == QStringLiteral("Tau"));

    // The value of the series: in every volume
    REQUIRE(own_fields::setValue(pm, city.id, QStringLiteral("Maun"), second));
    CHECK(own_fields::find(pm, city.id, first)->value == QStringLiteral("Maun"));

    // A value of its own in the first volume; the other keeps the series'
    city.seriesValue = QStringLiteral("Maun");
    city.ownValue = true;
    city.value = QStringLiteral("Ghanzi");
    REQUIRE(own_fields::change(pm, city, first));
    CHECK(own_fields::find(pm, city.id, first)->value == QStringLiteral("Ghanzi"));
    CHECK(own_fields::find(pm, city.id, second)->value == QStringLiteral("Maun"));
    // Its own value changes alone
    REQUIRE(own_fields::setValue(pm, city.id, QStringLiteral("Kang"), first));
    CHECK(own_fields::find(pm, city.id, first)->value == QStringLiteral("Kang"));
    CHECK(own_fields::find(pm, city.id, second)->value == QStringLiteral("Maun"));
    for (const core::ProjectBook& volume : pm.project()->books) {
        const QMap<QString, editor::CustomFieldValue> values = own_fields::values(pm, volume);
        CHECK(values.value(city.id).value ==
              (volume.id == first ? QStringLiteral("Kang") : QStringLiteral("Maun")));
    }
}

TEST_CASE("Own fields: the texts show the values, a field deleted becomes text with Undo",
          "[gui][fields][own]") {
    const OpenNovel book(QStringLiteral("Sand"));
    core::ProjectManager& pm = book.pm;
    const OwnField narrator = bookField(QStringLiteral("Narrator"), QStringLiteral("Ilias"));
    REQUIRE(own_fields::add(pm, narrator));
    // A second text, closed, with the field in its file
    const QString closed = pm.addElement(pm.chapterKindFor(core::BookPlace::Main),
                                         QStringLiteral("The Calm"), core::BookPlace::Main);
    REQUIRE_FALSE(closed.isEmpty());
    pm.setChapterContent(closed, QStringLiteral("<kml><p>Told by ") +
                                     editor::textFieldKml(textField(narrator)) +
                                     QStringLiteral(".</p></kml>"));
    REQUIRE(pm.saveChapterContent(closed));

    Window window;
    Fields fields(window);
    window.writeChapter(book.chapterId, QStringLiteral("By "));
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    fields.keys = text;
    text->setCursorPosition(editor::CursorPosition{.paragraph = 0, .offset = 3});
    REQUIRE(text->insertField(textField(narrator)));
    fields.coordinator.refresh();
    CHECK(shownFields(*text) == "Ilias");
    CHECK(fields.coordinator.contextFor(book.chapterId).customFields.contains(narrator.id));

    // A new value: the text follows
    REQUIRE(own_fields::setValue(pm, narrator.id, QStringLiteral("Hilary")));
    CHECK(shownFields(*text) == "Hilary");
    const FieldCoordinator::FieldUses uses = fields.coordinator.usesOf(narrator.id);
    CHECK(uses.count == 2);
    CHECK(uses.texts == 2);

    // Deleted, its places as text, in the open text and in the closed one
    std::optional<bool> asked;
    fields.coordinator.setDeleteAsker([&asked](const OwnField& field, int count, int texts) {
        CHECK(field.name == QStringLiteral("Narrator"));
        CHECK(count == 2);
        CHECK(texts == 2);
        asked = true;
        return std::optional<bool>(true);
    });
    QString notice;
    std::function<void()> undo;
    fields.coordinator.setNoticeShower(
        [&notice, &undo](gui::EditorPanel*, const QString& message, const QString&,
                         const QString&, std::function<void()> action) {
            notice = message;
            undo = std::move(action);
        });
    REQUIRE(fields.coordinator.deleteOwnField(narrator.id, QString(), nullptr));
    CHECK(asked);
    CHECK(notice.contains(QStringLiteral("Narrator")));
    CHECK_FALSE(own_fields::find(pm, narrator.id));
    CHECK(text->toKml().contains(QStringLiteral("By Hilary")));
    CHECK(pm.chapterText(closed).contains(QStringLiteral("Told by Hilary.")));

    // Undo in the bar brings it back everywhere
    REQUIRE(fields.coordinator.canUndoDelete());
    REQUIRE(undo);
    undo();
    CHECK(own_fields::find(pm, narrator.id));
    CHECK(shownFields(*text) == "Hilary");
    CHECK(editor::customFieldCountInKml(pm.chapterText(closed), narrator.id) == 1);

    // Deleted again, kept as empty fields; the writer's Undo in the text brings it back
    fields.coordinator.setDeleteAsker(
        [](const OwnField&, int, int) { return std::optional<bool>(false); });
    REQUIRE(fields.coordinator.deleteOwnField(narrator.id, QString(), nullptr));
    fields.coordinator.refresh();
    CHECK(shownFields(*text) == "[Narrator]");
    text->undoCommand();
    CHECK(own_fields::find(pm, narrator.id));
    CHECK(shownFields(*text) == "Hilary");

    // Not deleted when the window is cancelled
    fields.coordinator.setDeleteAsker([](const OwnField&, int, int) { return std::nullopt; });
    CHECK_FALSE(fields.coordinator.deleteOwnField(narrator.id, QString(), nullptr));
    CHECK(own_fields::find(pm, narrator.id));
}

TEST_CASE("Own fields: the group of the Properties panel", "[gui][fields][own][properties]") {
    const OpenNovel book(QStringLiteral("Sand"));
    core::ProjectManager& pm = book.pm;
    gui::BookFieldsBox box(pm);
    CHECK(box.fieldIds().isEmpty());
    CHECK_FALSE(box.emptyLabel()->isHidden());
    CHECK(box.addButton()->isEnabled());
    CHECK_FALSE(box.insertButton()->isEnabled());

    const OwnField narrator = bookField(QStringLiteral("Narrator"), QStringLiteral("Ilias"),
                                        QStringLiteral("Who tells the story"));
    REQUIRE(own_fields::add(pm, narrator));
    REQUIRE(box.fieldIds() == QStringList{narrator.id});
    QLineEdit* value = box.valueEdit(narrator.id);
    REQUIRE(value != nullptr);
    CHECK(value->text() == QStringLiteral("Ilias"));
    CHECK(value->toolTip() == QStringLiteral("Who tells the story"));
    CHECK(box.emptyLabel()->isHidden());
    CHECK(box.seriesNote()->isHidden());

    // Insert in Text... with a text of the book in front, the field with the keys chosen
    CHECK_FALSE(box.insertButton()->isEnabled());
    box.setCanInsert(true);
    CHECK(box.insertButton()->isEnabled());
    QStringList inserted;
    int added = 0;
    int settings = 0;
    QObject::connect(&box, &gui::BookFieldsBox::requestInsertField,
                     [&inserted](const QString& id) { inserted << id; });
    QObject::connect(&box, &gui::BookFieldsBox::requestNewField, [&added]() { ++added; });
    QObject::connect(&box, &gui::BookFieldsBox::requestSettings, [&settings]() { ++settings; });
    // The keys come to the field (sent straight to it: a window shown in a test is not
    // always active, as on macOS, and then setFocus() sends no FocusIn)
    box.show();
    QFocusEvent focusIn(QEvent::FocusIn, Qt::TabFocusReason);
    QCoreApplication::sendEvent(value, &focusIn);
    QCoreApplication::processEvents();
    box.insertButton()->click();
    CHECK(inserted == QStringList{narrator.id});
    box.addButton()->click();
    CHECK(added == 1);
    box.settingsButton()->click();
    CHECK(settings == 1);

    // A value written is kept when the keys leave it; the texts follow
    value->setText(QStringLiteral("Hilary"));
    emit value->editingFinished();
    value->clearFocus();
    CHECK(own_fields::find(pm, narrator.id)->value == QStringLiteral("Hilary"));
    CHECK(box.valueEdit(narrator.id) == value);  // the same row

    // A change elsewhere shows here
    REQUIRE(own_fields::setValue(pm, narrator.id, QStringLiteral("Kai")));
    CHECK(box.valueEdit(narrator.id) == value);
    CHECK(value->text() == QStringLiteral("Kai"));

    // Without a text of the book in front: nothing to insert in
    box.setCanInsert(false);
    CHECK_FALSE(box.insertButton()->isEnabled());

    // The last field taken away: the note of no fields again
    REQUIRE(own_fields::remove(pm, narrator.id));
    CHECK(box.fieldIds().isEmpty());
    CHECK_FALSE(box.emptyLabel()->isHidden());
}

TEST_CASE("Own fields: the note of the fields of the series in the Properties panel",
          "[gui][fields][own][properties][series]") {
    OpenNovel book(QStringLiteral("Sand"));
    core::ProjectManager& pm = book.pm;
    core::NewBook request;
    request.title = QStringLiteral("Dunes");
    REQUIRE_FALSE(pm.addBook(request).isEmpty());
    gui::BookFieldsBox box(pm);
    REQUIRE(own_fields::add(pm, seriesField(QStringLiteral("City"), QStringLiteral("Tau"))));
    REQUIRE(own_fields::add(pm, seriesField(QStringLiteral("Publisher"), QString())));
    CHECK_FALSE(box.seriesNote()->isHidden());
    CHECK(box.seriesNote()->text().contains(QStringLiteral("“City”")));
    CHECK(box.seriesNote()->text().contains(QStringLiteral("“Publisher”")));
    const QString publisher = box.fieldIds().at(1);
    CHECK(box.valueEdit(publisher)->placeholderText().contains(QStringLiteral("[Publisher]")));
}

TEST_CASE("Own fields: quick insert offers them after #, as fields", "[gui][fields][own][quick]") {
    const OpenNovel book(QStringLiteral("Sand"));
    core::ProjectManager& pm = book.pm;
    const OwnField narrator = bookField(QStringLiteral("Narrator"), QStringLiteral("Ilias"));
    REQUIRE(own_fields::add(pm, narrator));
    REQUIRE(own_fields::add(pm, bookField(QStringLiteral("City"), QString())));

    const QList<editor::QuickInsertItem> items = gui::fieldItems(pm);
    REQUIRE(items.size() == 2);
    CHECK(items[0].source == editor::QuickInsertSource::Field);
    CHECK(items[0].name == QStringLiteral("City"));
    CHECK(items[0].detail == QStringLiteral("no value"));
    CHECK(items[1].field == narrator.id);
    CHECK(items[1].detail == QStringLiteral("Ilias"));
    CHECK(items[1].group == QStringLiteral("Own Fields"));

    // After # by default, in a text of the book only
    const auto hasFields = [](const QList<editor::QuickInsertItem>& list) {
        return std::any_of(list.cbegin(), list.cend(), [](const editor::QuickInsertItem& item) {
            return item.source == editor::QuickInsertSource::Field;
        });
    };
    CHECK(hasFields(gui::quickInsertItems(pm, QLatin1Char('#'), QString())));
    CHECK_FALSE(hasFields(gui::quickInsertItems(pm, QLatin1Char('#'))));
    CHECK_FALSE(hasFields(gui::quickInsertItems(pm, QLatin1Char('@'), QString())));
    CHECK(hasFields(gui::quickInsertItems(pm, QChar(), QString())));
}

}  // namespace kalahari
