// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_publishing_templates.cpp
/// @brief Templates of Texts for Publishers: the templates of the book type and the writer's
///        own, made, copied, changed and deleted with the keys

#include "kalahari_tests/editor/editor_test_utils.h"
#include "kalahari_tests/gui/dialog_checks.h"
#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/gui/publishing_test_utils.h"
#include "kalahari_tests/polish.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/publishing_templates_dialog.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/main_window.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <memory>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::PublishingTemplate;
using core::PublishingTexts;
using gui::dialogs::MessageDialog;
using gui::dialogs::PublishingTemplatesDialog;

namespace {

/// Press @p key with @p modifiers in @p widget
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    REQUIRE(widget != nullptr);
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

/// Type @p text in @p widget, a key for each character
void type(QWidget* widget, const QString& text) {
    REQUIRE(widget != nullptr);
    for (const QChar character : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(character));
        QCoreApplication::sendEvent(widget, &event);
    }
}

/// The texts of the rows of @p list, the names of the groups in brackets
QStringList rowsOf(const QListWidget& list) {
    QStringList rows;
    for (int row = 0; row < list.count(); ++row) {
        const QListWidgetItem* item = list.item(row);
        const bool group = item->flags() == Qt::NoItemFlags;
        rows << (group ? QLatin1Char('[') + item->text() + QLatin1Char(']') : item->text());
    }
    return rows;
}

/// The texts of column @p column of @p table
QStringList columnOf(const QTableWidget& table, int column) {
    QStringList texts;
    for (int row = 0; row < table.rowCount(); ++row) {
        texts << table.item(row, column)->text();
    }
    return texts;
}

/// The names of the points of the writer's first template, as the settings keep them
QStringList keptPoints() {
    const QList<PublishingTemplate> own = PublishingTexts::ownTemplates();
    REQUIRE_FALSE(own.isEmpty());
    QStringList names;
    for (const core::PublishingTemplatePoint& point : own.first().points) {
        names << point.name.text(QStringLiteral("en"));
    }
    return names;
}

/// Whether the window fits 1366x768 at 150%: 911 pixels wide and 460 high (a Mac's larger
/// controls need a few more), with a synopsis chosen and with a sentence
void checkFits(const core::ProjectManager& pm, const char* language) {
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 480;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    INFO("In " << language);
    PublishingTemplatesDialog dialog(PublishingTemplatesDialog::builtInOf(&pm),
                                     PublishingTemplatesDialog::installedTypes(),
                                     QStringLiteral("kalahari.novel"), pm.mainTexts());
    for (const char* reference : {"kalahari.novel:full", "kalahari.novel:when_must"}) {
        dialog.setCurrentTemplate(QString::fromLatin1(reference));
        REQUIRE(dialog.currentTemplate() == QString::fromLatin1(reference));
        const QSize size = dialog.sizeHint();
        INFO(reference << " " << size.width() << "x" << size.height());
        CHECK(size.width() <= 911);
        CHECK(size.height() <= MAX_HEIGHT);
    }
    // The writer's own template: the boxes of the types and the buttons of the points
    dialog.setCurrentTemplate(QStringLiteral("kalahari.novel:full"));
    dialog.copyButton()->click();
    REQUIRE(dialog.addPointButton()->isVisibleTo(&dialog));
    const QSize own = dialog.sizeHint();
    INFO("own " << own.width() << "x" << own.height());
    CHECK(own.width() <= 911);
    CHECK(own.height() <= MAX_HEIGHT);
    // Each access key once, of the points and of the pattern too
    test::checkKeys(dialog);
}

/// Type @p text in the cell of @p table being changed and press Enter
void typeInCell(const QTableWidget& table, const QString& text) {
    QLineEdit* editor = nullptr;
    for (QLineEdit* child : table.findChildren<QLineEdit*>()) {
        if (child->isVisible()) {
            editor = child;
        }
    }
    REQUIRE(editor != nullptr);
    editor->selectAll();
    type(editor, text);
    press(editor, Qt::Key_Return);
    QCoreApplication::processEvents();
}

}  // namespace

TEST_CASE("Templates of Texts for Publishers: the templates of the book type and the writer's "
          "own, with the keys",
          "[gui][publishing][dialogs]") {
    // The keys need an active window, which the offscreen platform and Windows give; the test
    // runs on a Mac do not
    const QString platform = QGuiApplication::platformName();
    if (platform != QStringLiteral("offscreen") && platform != QStringLiteral("windows")) {
        SKIP("needs an active window: run with QT_QPA_PLATFORM=offscreen");
    }
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    const test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    PublishingTemplatesDialog dialog(PublishingTemplatesDialog::builtInOf(&pm),
                                     PublishingTemplatesDialog::installedTypes(),
                                     QStringLiteral("kalahari.novel"), pm.mainTexts());
    dialog.show();
    dialog.activateWindow();
    REQUIRE(test::waitUntil([&dialog] { return dialog.isActiveWindow(); }));
    // The questions are answered (test::answering()); this window is none of them
    dialog.setProperty("handled", true);
    QListWidget& list = *dialog.templateList();

    // The templates of the novel, then the writer's, who has none yet
    CHECK(rowsOf(list) == QStringList{QStringLiteral("[Novel – Built-in]"),
                                      QStringLiteral("One page – five steps"),
                                      QStringLiteral("Full synopsis – up to three pages"),
                                      QStringLiteral("Chapter by chapter"),
                                      QStringLiteral("When… must… before…"),
                                      QStringLiteral("Hero, goal, obstacle"),
                                      QStringLiteral("Comparison with another book"),
                                      QStringLiteral("[My Templates]")});
    CHECK(dialog.headingDescription().contains(QStringLiteral("\"Novel\"")));

    // A template of the type shows all of it and changes nothing
    CHECK(dialog.currentTemplate() == QStringLiteral("kalahari.novel:one_page"));
    CHECK(dialog.nameEdit()->text() == QStringLiteral("One page – five steps"));
    CHECK(dialog.nameEdit()->isReadOnly());
    CHECK_FALSE(dialog.partBox()->isEnabled());
    CHECK(dialog.typeLabel()->isVisible());
    CHECK(dialog.typeLabel()->text().startsWith(
        QStringLiteral("Novel – a template of the book type")));
    CHECK(dialog.charactersBox()->isReadOnly());
    CHECK(dialog.pointsTable()->rowCount() == 5);
    CHECK_FALSE(dialog.pointsTable()->item(0, PublishingTemplatesDialog::POINT_NAME_COLUMN)
                    ->flags()
                    .testFlag(Qt::ItemIsEditable));
    CHECK_FALSE(dialog.addPointButton()->isVisible());
    CHECK(dialog.copyButton()->isEnabled());
    CHECK_FALSE(dialog.deleteButton()->isEnabled());
    CHECK(dialog.typeLabel()->text().contains(QStringLiteral("Make a Copy")));

    // The arrows go through the templates, past the names of the groups
    list.setFocus();
    press(&list, Qt::Key_Down);
    press(&list, Qt::Key_Down);
    CHECK(dialog.currentTemplate() == QStringLiteral("kalahari.novel:each_chapter"));
    // A point for each chapter, named after it, with the length for each of them
    QStringList chapters;
    for (const auto& [id, title] : pm.mainTexts()) {
        chapters << title;
    }
    CHECK(chapters.contains(QStringLiteral("The Storm")));
    CHECK(columnOf(*dialog.pointsTable(), PublishingTemplatesDialog::POINT_NAME_COLUMN) ==
          chapters);
    CHECK(dialog.lengthNote()->isVisible());
    CHECK(dialog.lengthNote()->text().startsWith(
        QStringLiteral("For each text; this book has %1 texts").arg(chapters.size())));
    // End goes to the last template, past the name of the writer's group, which has none
    press(&list, Qt::Key_End);
    CHECK(dialog.currentTemplate() == QStringLiteral("kalahari.novel:comparison"));
    CHECK(dialog.patternEdit()->isVisible());
    CHECK_FALSE(dialog.pointsTable()->isVisible());
    CHECK(dialog.fieldsTable()->rowCount() > 0);
    CHECK(dialog.fieldsTable()->item(0, PublishingTemplatesDialog::FIELD_ID_COLUMN)
              ->text()
              .startsWith(QLatin1Char('{')));
    press(&list, Qt::Key_Home);
    CHECK(dialog.currentTemplate() == QStringLiteral("kalahari.novel:one_page"));
    // Delete does nothing to a template of the type
    press(&list, Qt::Key_Delete);
    CHECK(list.count() == 8);

    // Make a Copy: the writer's own template, kept at once, for the type of the book
    dialog.copyButton()->click();
    REQUIRE(PublishingTexts::ownTemplates().size() == 1);
    const PublishingTemplate copy = PublishingTexts::ownTemplates().first();
    CHECK(dialog.currentTemplate() == copy.reference());
    CHECK(copy.types == QStringList{QStringLiteral("kalahari.novel")});
    CHECK(dialog.nameEdit()->text() == QStringLiteral("One page – five steps – copy"));
    CHECK(dialog.nameEdit()->hasFocus());
    CHECK_FALSE(dialog.nameEdit()->isReadOnly());
    CHECK(dialog.typeBox(QStringLiteral("kalahari.novel"))->isChecked());
    CHECK(dialog.deleteButton()->isEnabled());
    CHECK(dialog.changed());
    CHECK(rowsOf(list).last() == QStringLiteral("One page – five steps – copy"));

    // Its name, typed
    dialog.nameEdit()->selectAll();
    type(dialog.nameEdit(), QStringLiteral("Contest"));
    CHECK(PublishingTexts::ownTemplates().first().name.text(QStringLiteral("en")) ==
          QStringLiteral("Contest"));
    CHECK(list.currentItem()->text() == QStringLiteral("Contest"));

    // Its points: Insert adds one after the point chosen and changes its name at once
    QTableWidget& points = *dialog.pointsTable();
    CHECK(points.item(0, 0)->flags().testFlag(Qt::ItemIsEditable));
    CHECK(dialog.addPointButton()->isVisible());
    points.setFocus();
    points.setCurrentCell(0, PublishingTemplatesDialog::POINT_NAME_COLUMN);
    press(&points, Qt::Key_Insert);
    CHECK(points.rowCount() == 6);
    CHECK(points.currentRow() == 1);
    typeInCell(points, QStringLiteral("Theme"));
    QStringList names = keptPoints();
    REQUIRE(names.size() == 6);
    CHECK(names[1] == QStringLiteral("Theme"));
    // Alt+Down and Alt+Up move it, Delete takes it out
    points.setFocus();
    press(&points, Qt::Key_Down, Qt::AltModifier);
    CHECK(keptPoints()[2] == QStringLiteral("Theme"));
    CHECK(points.currentRow() == 2);
    press(&points, Qt::Key_Up, Qt::AltModifier);
    CHECK(keptPoints()[1] == QStringLiteral("Theme"));
    press(&points, Qt::Key_Delete);
    CHECK(keptPoints().size() == 5);
    CHECK_FALSE(keptPoints().contains(QStringLiteral("Theme")));
    // The hint of a point
    points.setCurrentCell(0, PublishingTemplatesDialog::POINT_HINT_COLUMN);
    press(&points, Qt::Key_F2);
    typeInCell(points, QStringLiteral("Who and where."));
    CHECK(PublishingTexts::ownTemplates().first().points.first().hint.text(
              QStringLiteral("en")) == QStringLiteral("Who and where."));

    // Its types and its length
    dialog.typeBox(QStringLiteral("kalahari.novel"))->click();
    dialog.typeBox(QStringLiteral("kalahari.short_stories"))->click();
    CHECK(PublishingTexts::ownTemplates().first().types ==
          QStringList{QStringLiteral("kalahari.short_stories")});
    dialog.charactersBox()->setValue(1000);
    CHECK(PublishingTexts::ownTemplates().first().targetCharacters == 1000);

    // A sentence: the pattern, whose fields follow it
    dialog.partBox()->setCurrentIndex(
        dialog.partBox()->findData(QString::fromLatin1(PublishingTexts::PITCH)));
    CHECK(dialog.patternEdit()->isVisible());
    CHECK_FALSE(dialog.pointsTable()->isVisible());
    type(dialog.patternEdit(), QStringLiteral("When {event}, {hero} must act."));
    CHECK(columnOf(*dialog.fieldsTable(), PublishingTemplatesDialog::FIELD_ID_COLUMN) ==
          QStringList{QStringLiteral("{event}"), QStringLiteral("{hero}")});
    const PublishingTemplate pitch = PublishingTexts::ownTemplates().first();
    CHECK(pitch.part == QLatin1String(PublishingTexts::PITCH));
    CHECK(pitch.pattern.text(QStringLiteral("en")) ==
          QStringLiteral("When {event}, {hero} must act."));
    REQUIRE(pitch.fields.size() == 2);
    CHECK(pitch.fields[1].id == QStringLiteral("hero"));

    // Insert in the list: a new template of the part chosen, for the type of the book
    list.setFocus();
    press(&list, Qt::Key_Insert);
    REQUIRE(PublishingTexts::ownTemplates().size() == 2);
    const PublishingTemplate made = PublishingTexts::ownTemplates().last();
    CHECK(made.name.text(QStringLiteral("en")) == QStringLiteral("New template"));
    CHECK(made.part == QLatin1String(PublishingTexts::PITCH));
    CHECK(made.types == QStringList{QStringLiteral("kalahari.novel")});
    CHECK(dialog.currentTemplate() == made.reference());
    // Home and End go through the groups to the first and the last template
    press(&list, Qt::Key_Home);
    CHECK(dialog.currentTemplate() == QStringLiteral("kalahari.novel:one_page"));
    press(&list, Qt::Key_End);
    CHECK(dialog.currentTemplate() == made.reference());
    press(&list, Qt::Key_Up);
    CHECK(dialog.currentTemplate() == copy.reference());
    press(&list, Qt::Key_Up);
    CHECK(dialog.currentTemplate() == QStringLiteral("kalahari.novel:comparison"));
    press(&list, Qt::Key_End);

    // Delete asks first: Cancel keeps it, Delete takes it out
    QStringList said;
    list.setFocus();
    CHECK(test::answering([&]() { press(&list, Qt::Key_Delete); },
                          MessageDialog::Answer::Cancel, said) == 1);
    CHECK(PublishingTexts::ownTemplates().size() == 2);
    INFO(said.join(QLatin1Char('\n')).toStdString());
    CHECK(said.join(QLatin1Char('\n')).contains(QStringLiteral("\"New template\"")));
    CHECK(test::answering([&]() { dialog.deleteButton()->click(); },
                          MessageDialog::Answer::Accept, said) == 1);
    REQUIRE(PublishingTexts::ownTemplates().size() == 1);
    CHECK(dialog.currentTemplate() == PublishingTexts::ownTemplates().first().reference());

    // Another window shows the writer's templates as they are kept
    const PublishingTemplatesDialog again(PublishingTemplatesDialog::builtInOf(&pm),
                                          PublishingTemplatesDialog::installedTypes(),
                                          QStringLiteral("kalahari.novel"), pm.mainTexts());
    CHECK(rowsOf(*again.templateList()).last() == QStringLiteral("Contest"));
}

TEST_CASE("Templates of Texts for Publishers: without a book, the templates of every type",
          "[gui][publishing][dialogs]") {
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    {
        // The types of the books are those of the program
        const test::OpenNovel novel(QStringLiteral("Types"));
    }
    const PublishingTemplatesDialog dialog(PublishingTemplatesDialog::builtInOf(nullptr),
                                           PublishingTemplatesDialog::installedTypes(), QString(),
                                           {});
    const QStringList rows = rowsOf(*dialog.templateList());
    CHECK(rows.contains(QStringLiteral("[Novel – Built-in]")));
    CHECK(rows.contains(QStringLiteral("[Short Story Collection – Built-in]")));
    CHECK(rows.last() == QStringLiteral("[My Templates]"));
    CHECK(dialog.headingDescription().startsWith(
        QStringLiteral("The templates of the book types")));
    // A new template is for every type
    dialog.newButton()->click();
    REQUIRE(PublishingTexts::ownTemplates().size() == 1);
    CHECK(PublishingTexts::ownTemplates().first().types.isEmpty());
    CHECK(PublishingTexts::ownTemplates().first().points.size() == 1);
}

TEST_CASE("Templates of Texts for Publishers: fits a small screen, in English and in Polish",
          "[gui][publishing][dialogs]") {
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    const test::OpenNovel novel(QStringLiteral("Fits"));
    checkFits(novel.pm, "English");
    const test::Polish polish;
    REQUIRE(polish.loaded());
    checkFits(novel.pm, "Polish");
}
