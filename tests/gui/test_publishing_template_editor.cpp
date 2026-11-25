// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_publishing_template_editor.cpp
/// @brief The template of a text for publishers changed in place, beside the text: a built-in
///        one kept as the writer's own, their own one changed, a new one, Ctrl+Enter and Esc

#include "kalahari_tests/changed_setting.h"
#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/publishing_card_panel.h"
#include "kalahari/gui/panels/publishing_text_box.h"
#include "kalahari/gui/widgets/publishing_template_editor.h"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>

#include <nlohmann/json.hpp>

#include <string>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;

using core::PublishingTemplate;
using core::PublishingTexts;
using gui::PublishingTemplateEditor;
using gui::PublishingTextBox;

namespace {

/// Press @p key with @p modifiers on @p widget
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    REQUIRE(widget != nullptr);
    QKeyEvent down(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &down);
}

/// The texts of column @p column of @p table
QStringList columnOf(const QTableWidget& table, int column) {
    QStringList texts;
    for (int row = 0; row < table.rowCount(); ++row) {
        const QTableWidgetItem* item = table.item(row, column);
        texts << (item != nullptr ? item->text() : QString());
    }
    return texts;
}

/// The names of the points of the box
QStringList pointsOf(const PublishingTextBox& box) {
    QStringList names;
    for (const QCheckBox* point : box.points()) {
        names << point->text();
    }
    return names;
}

/// The writer's own templates and the unit of the lengths during a test, as they were after it
struct KeptTemplates {
    test::ChangedSetting<nlohmann::json> templates{PublishingTexts::TEMPLATES_SETTING,
                                                   nlohmann::json::array()};
    test::ChangedSetting<std::string> unit{core::CoverMaterial::UNIT_SETTING, "characters"};
};

}  // namespace

TEST_CASE("For Publishers: a built-in template changed beside a synopsis is kept as the "
          "writer's own",
          "[gui][publishing][publishingtemplate]") {
    gui::registerAllIcons();
    const KeptTemplates kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString id = pm.addPublishingText(QString::fromLatin1(PublishingTexts::SYNOPSIS),
                                            QStringLiteral("kalahari.novel:one_page"),
                                            QStringLiteral("Synopsis – version 1"));
    REQUIRE_FALSE(id.isEmpty());
    PublishingTextBox box(pm);
    box.showText(id, nullptr);
    REQUIRE(box.points().size() == 5);

    // The pencil: the template in place of the points, its name with the keys
    box.changeTemplateButton()->click();
    PublishingTemplateEditor* editor = box.templateEditor();
    REQUIRE(editor->isVisibleTo(&box));
    CHECK_FALSE(box.points().constFirst()->isVisibleTo(&box));
    CHECK_FALSE(box.templateBox()->isEnabled());
    CHECK(editor->noteLabel()->text().startsWith(QStringLiteral("A built-in template stays")));
    CHECK(editor->nameEdit()->text() == QStringLiteral("One page – five steps"));
    CHECK(editor->targetSpin()->value() == 1800);
    QTableWidget* points = editor->pointsTable();
    CHECK(columnOf(*points, 0).size() == 5);
    CHECK(columnOf(*points, 0).constFirst() == QStringLiteral("Hero and world"));
    CHECK_FALSE(editor->patternEdit()->isVisibleTo(editor));

    // The keys of the points: Delete removes, Insert adds after the one chosen, Alt+Up moves
    points->setCurrentCell(4, 0);
    press(points, Qt::Key_Delete);
    CHECK(points->rowCount() == 4);
    points->setCurrentCell(0, 0);
    press(points, Qt::Key_Down, Qt::AltModifier);
    CHECK(columnOf(*points, 0).value(1) == QStringLiteral("Hero and world"));
    editor->addPoint();
    CHECK(points->rowCount() == 5);
    points->item(2, 0)->setText(QStringLiteral("The well"));
    points->item(2, 1)->setText(QStringLiteral("Where the water is."));
    editor->targetSpin()->setValue(1500);

    // Ctrl+Enter keeps it as the writer's template, which the text then has
    press(editor->nameEdit(), Qt::Key_Return, Qt::ControlModifier);
    CHECK_FALSE(editor->isVisibleTo(&box));
    const QList<PublishingTemplate> own = PublishingTexts::ownTemplates();
    REQUIRE(own.size() == 1);
    CHECK(own.constFirst().name.text(QStringLiteral("en")) ==
          QStringLiteral("One page – five steps – mine"));
    CHECK(own.constFirst().targetCharacters == 1500);
    CHECK(own.constFirst().types.isEmpty());  // for all books
    CHECK(pm.findElement(id)->publishing.templateId == own.constFirst().reference());
    CHECK(pointsOf(box) == QStringList({QStringLiteral("The event that changes everything"),
                                        QStringLiteral("Hero and world"),
                                        QStringLiteral("The well"),
                                        QStringLiteral("Goal and obstacles"),
                                        QStringLiteral("Climax")}));
    CHECK(box.templateBox()->isEnabled());
    // The built-in template stays as it was
    const std::optional<PublishingTemplate> builtIn =
        pm.publishingTemplate(QStringLiteral("kalahari.novel:one_page"));
    REQUIRE(builtIn.has_value());
    CHECK(builtIn->points.size() == 5);

    // Their own template changed again: the same one; Esc drops the changes
    box.changeTemplateButton()->click();
    CHECK(editor->noteLabel()->text().startsWith(QStringLiteral("Your template")));
    editor->nameEdit()->setText(QStringLiteral("Desert"));
    press(editor->nameEdit(), Qt::Key_Escape);
    CHECK_FALSE(editor->isVisibleTo(&box));
    CHECK(PublishingTexts::ownTemplates().constFirst().name.text(QStringLiteral("en")) ==
          QStringLiteral("One page – five steps – mine"));
    box.changeTemplateButton()->click();
    editor->nameEdit()->setText(QStringLiteral("Desert"));
    editor->keepButton()->click();
    REQUIRE(PublishingTexts::ownTemplates().size() == 1);
    CHECK(PublishingTexts::ownTemplates().constFirst().name.text(QStringLiteral("en")) ==
          QStringLiteral("Desert"));
    CHECK(box.templateBox()->currentText() == QStringLiteral("Desert"));
}

TEST_CASE("For Publishers: New Template... on the list, and the pattern of a sentence with its "
          "fields",
          "[gui][publishing][publishingtemplate]") {
    gui::registerAllIcons();
    const KeptTemplates kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString id = pm.addPublishingText(QString::fromLatin1(PublishingTexts::PITCH),
                                            QStringLiteral("kalahari.novel:when_must"),
                                            QStringLiteral("Sentence – version 1"));
    REQUIRE_FALSE(id.isEmpty());
    test::Window window;
    window.navigator.onElementSelected(id, QStringLiteral("Sentence – version 1"));
    auto* card = qobject_cast<gui::PublishingCardPanel*>(window.tabs->currentWidget());
    REQUIRE(card != nullptr);
    PublishingTextBox* box = card->textBox();

    // New Template... starts the writer's template from scratch
    QComboBox* templates = box->templateBox();
    const int item = templates->findText(QStringLiteral("New Template..."));
    REQUIRE(item == templates->count() - 1);
    templates->setCurrentIndex(item);
    emit templates->activated(item);
    PublishingTemplateEditor* editor = box->templateEditor();
    REQUIRE(editor->isVisibleTo(box));
    CHECK(templates->currentData().toString() == QStringLiteral("kalahari.novel:when_must"));
    CHECK(editor->nameEdit()->text() == QStringLiteral("New template"));
    CHECK(editor->patternEdit()->isVisibleTo(editor));
    CHECK_FALSE(editor->pointsTable()->isVisibleTo(editor));

    // The fields come from the pattern, in braces
    editor->patternEdit()->setText(QStringLiteral("{hero} crosses {place}."));
    emit editor->patternEdit()->textEdited(editor->patternEdit()->text());
    CHECK(columnOf(*editor->fieldsTable(), 0) ==
          QStringList({QStringLiteral("hero"), QStringLiteral("place")}));
    editor->fieldsTable()->item(1, 1)->setText(QStringLiteral("where"));
    editor->keepButton()->click();

    // The text has the new template; the card the fields of its pattern
    const QList<PublishingTemplate> own = PublishingTexts::ownTemplates();
    REQUIRE(own.size() == 1);
    CHECK(own.constFirst().types.isEmpty());
    CHECK(pm.findElement(id)->publishing.templateId == own.constFirst().reference());
    REQUIRE(card->field(QStringLiteral("place")) != nullptr);
    CHECK(card->field(QStringLiteral("place"))->accessibleName() == QStringLiteral("where"));
    CHECK(card->field(QStringLiteral("event")) == nullptr);

    // Its pattern changed in place: the card has the fields of the new pattern
    box->changeTemplateButton()->click();
    editor->patternEdit()->setText(QStringLiteral("{hero} crosses {place} at {time}."));
    emit editor->patternEdit()->textEdited(editor->patternEdit()->text());
    editor->keepButton()->click();
    CHECK(card->field(QStringLiteral("time")) != nullptr);
    CHECK(card->field(QStringLiteral("place"))->accessibleName() == QStringLiteral("where"));
}
