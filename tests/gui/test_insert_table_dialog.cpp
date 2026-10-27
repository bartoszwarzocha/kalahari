/// @file test_insert_table_dialog.cpp
/// @brief Insert > Table...: the window of the new table and the table the editor panel puts
///        in the text

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/dialogs/insert_table_dialog.h"
#include "kalahari/gui/panels/editor_panel.h"

#include "document_window.h"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>

using namespace kalahari;
using namespace kalahari::editor;
using kalahari::gui::dialogs::InsertTableDialog;
using kalahari::gui::dialogs::KalahariDialog;

namespace {

/// The spin box a label names
QSpinBox* spinBoxOf(const QWidget& dialog, const QString& label)
{
    for (QLabel* each : dialog.findChildren<QLabel*>()) {
        if (each->text() == label) {
            return qobject_cast<QSpinBox*>(each->buddy());
        }
    }
    return nullptr;
}

/// The widget for a screen reader by its name
QWidget* namedWidget(const QWidget& dialog, const QString& name)
{
    for (QWidget* each : dialog.findChildren<QWidget*>()) {
        if (each->accessibleName() == name) {
            return each;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("Insert Table dialog: columns, rows and the header row", "[gui][tables][dialog]") {
    // A gear only with settings in the Settings dialog: this window has none
    KalahariDialog::setSettingsOpener([](QWidget*, const QString&) {});
    InsertTableDialog dialog;
    KalahariDialog::setSettingsOpener(nullptr);
    dialog.show();
    QApplication::processEvents();
    CHECK(dialog.windowTitle() == QStringLiteral("Insert Table"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Insert"));
    CHECK(dialog.findChildren<QToolButton*>().isEmpty());

    QSpinBox* columns = spinBoxOf(dialog, QStringLiteral("&Columns"));
    QSpinBox* rows = spinBoxOf(dialog, QStringLiteral("&Rows"));
    REQUIRE(columns != nullptr);
    REQUIRE(rows != nullptr);
    CHECK(dialog.columns() == 3);
    CHECK(dialog.rows() == 3);
    CHECK(dialog.headerRow());
    CHECK(columns->minimum() == 1);
    CHECK(columns->maximum() == BookEditor::MAX_INSERTED_TABLE_COLUMNS);
    CHECK(rows->maximum() == BookEditor::MAX_INSERTED_TABLE_ROWS);
    CHECK(dialog.focusWidget() == columns);

    // The picture says what it shows
    const QWidget* preview = namedWidget(dialog, QStringLiteral("Preview of the table"));
    REQUIRE(preview != nullptr);
    CHECK(preview->accessibleDescription() ==
          QStringLiteral("3 columns and 3 rows, with a header row"));
    columns->setValue(4);
    rows->setValue(2);
    dialog.findChild<QCheckBox*>()->setChecked(false);
    CHECK(preview->accessibleDescription() == QStringLiteral("4 columns and 2 rows"));
    CHECK(dialog.columns() == 4);
    CHECK(dialog.rows() == 2);
    CHECK_FALSE(dialog.headerRow());
    CHECK_FALSE(dialog.grab().isNull());
}

TEST_CASE("Editor panel: Insert > Table puts the window's table at the cursor",
          "[gui][tables][dialog]") {
    gui::EditorPanel panel;
    BookEditor* editor = panel.getBookEditor();
    editor->fromKml(QStringLiteral("<kml><p>One</p><p></p><p>Two</p></kml>"));
    editor->setCursorPosition({1, 0});

    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<InsertTableDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            if (QDialog* other = test::shownDialog()) {
                other->reject();
            }
            return;
        }
        spinBoxOf(*dialog, QStringLiteral("&Columns"))->setValue(2);
        spinBoxOf(*dialog, QStringLiteral("&Rows"))->setValue(1);
        dialog->acceptButton()->click();
    });
    panel.showInsertTableDialog();
    CHECK(editor->toKml().toStdString() ==
          "<kml><p>One</p><table><tr><th><p></p></th><th><p></p></th></tr></table>"
          "<p>Two</p></kml>");
    CHECK(editor->isInTable());

    // In a table the window does not open
    QTimer::singleShot(0, [&]() {
        if (QDialog* dialog = test::shownDialog()) {
            FAIL_CHECK("a window opened in a table");
            dialog->reject();
        }
    });
    panel.showInsertTableDialog();
    QApplication::processEvents();
}
