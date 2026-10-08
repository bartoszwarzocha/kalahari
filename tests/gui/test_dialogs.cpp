/// @file test_dialogs.cpp
/// @brief The program's own dialogs: their common base and the Navigator's dialogs

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/book_constants.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/new_element_dialog.h"
#include "kalahari/gui/dialogs/rename_element_dialog.h"

#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>

using namespace kalahari::gui::dialogs;

namespace {

/// @brief The labels of a dialog that show a text, also while the dialog is hidden
bool showsText(const QDialog& dialog, const QString& text) {
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->isVisibleTo(&dialog) && label->text().contains(text)) {
            return true;
        }
    }
    return false;
}

/// @brief Type a letter into a field, as the writer does
void type(QLineEdit* field, const QString& letter) {
    QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, letter);
    QApplication::sendEvent(field, &press);
}

} // anonymous namespace

TEST_CASE("Own dialogs: the heading and the buttons of the base", "[gui][dialogs]") {
    KalahariDialog dialog;
    dialog.setHeading(QStringLiteral("Add Chapter"), QStringLiteral("Where it goes"));
    dialog.setAcceptText(QStringLiteral("Add"));

    CHECK(dialog.windowTitle() == QStringLiteral("Add Chapter"));
    CHECK(showsText(dialog, QStringLiteral("Add Chapter")));
    CHECK(showsText(dialog, QStringLiteral("Where it goes")));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Add"));
    CHECK(dialog.acceptButton()->isDefault());
    CHECK(dialog.cancelButton()->isVisibleTo(&dialog));

    SECTION("Without a description the heading shows only the title") {
        dialog.setHeading(QStringLiteral("Rename"));
        CHECK_FALSE(showsText(dialog, QStringLiteral("Where it goes")));
    }

    SECTION("Apply is hidden until shown, and it keeps the dialog open") {
        CHECK_FALSE(dialog.applyButton()->isVisibleTo(&dialog));
        dialog.setApplyButtonVisible(true);
        CHECK(dialog.applyButton()->isVisibleTo(&dialog));

        int applied = 0;
        QObject::connect(&dialog, &KalahariDialog::applyClicked, [&applied]() { ++applied; });
        dialog.show();
        dialog.applyButton()->click();
        CHECK(applied == 1);
        CHECK(dialog.isVisible());

        dialog.acceptButton()->click();
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.result() == QDialog::Accepted);
    }

    SECTION("Cancel closes it without accepting") {
        dialog.show();
        dialog.cancelButton()->click();
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.result() == QDialog::Rejected);
    }
}

TEST_CASE("Own dialogs: a compact heading is lower", "[gui][dialogs]") {
    // The Settings window needs a heading that does not take room from its pages
    KalahariDialog regular;
    regular.setHeading(QStringLiteral("Settings"));
    KalahariDialog compact;
    compact.setHeading(QStringLiteral("Settings"));
    compact.setCompactHeading(true);

    const auto* regularHeading = regular.findChild<QFrame*>(QStringLiteral("kalahariDialogHeading"));
    const auto* compactHeading = compact.findChild<QFrame*>(QStringLiteral("kalahariDialogHeading"));
    REQUIRE(regularHeading != nullptr);
    REQUIRE(compactHeading != nullptr);
    CHECK(compactHeading->sizeHint().height() < regularHeading->sizeHint().height());
}

TEST_CASE("Own dialogs: a dialog opens as high as its content needs", "[gui][dialogs]") {
    // Regression: the height was measured for a narrower dialog, where the sentence in the
    // heading takes more lines, so the wider dialog had a gap above its buttons
    RenameElementDialog dialog(QStringLiteral("Chapter One: The Beginning"),
                               QStringLiteral("template.chapter"));
    dialog.adjustSize();  // what showing the dialog does

    CHECK(dialog.width() >= dialog.minimumWidth());
    CHECK(dialog.height() == dialog.layout()->totalHeightForWidth(dialog.width()));
}

TEST_CASE("Navigator: the new chapter dialog asks for a title", "[gui][dialogs]") {
    NewElementDialog dialog(NewElementKind::Chapter, QStringLiteral("Part One"));

    CHECK(dialog.elementType() == QLatin1String(kalahari::core::TYPE_CHAPTER));
    CHECK(dialog.findChild<QComboBox*>() == nullptr);
    CHECK(showsText(dialog, QStringLiteral("Part One")));  // the part it goes to

    auto* title = dialog.findChild<QLineEdit*>();
    REQUIRE(title != nullptr);
    CHECK(title->text() == QStringLiteral("New Chapter"));
    CHECK(title->selectedText() == title->text());  // typing replaces it
    CHECK(dialog.acceptButton()->isEnabled());

    title->setText(QStringLiteral("   "));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());

    title->setText(QStringLiteral("  The Storm "));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.title() == QStringLiteral("The Storm"));
}

TEST_CASE("Navigator: the new part dialog asks for a title", "[gui][dialogs]") {
    NewElementDialog dialog(NewElementKind::Part);

    CHECK(dialog.elementType() == QStringLiteral("part"));
    CHECK(dialog.findChild<QComboBox*>() == nullptr);
    CHECK(dialog.title() == QStringLiteral("New Part"));
}

TEST_CASE("Navigator: a new item gets a type and the type's name as its title",
          "[gui][dialogs]") {
    // Regression: every new front matter item was a preface and every back matter
    // item an epilogue, whatever the writer wanted
    SECTION("Front matter") {
        NewElementDialog dialog(NewElementKind::FrontMatterItem);
        auto* types = dialog.findChild<QComboBox*>();
        auto* title = dialog.findChild<QLineEdit*>();
        REQUIRE(types != nullptr);
        REQUIRE(title != nullptr);

        const QStringList expected = {QLatin1String(kalahari::core::TYPE_TITLE_PAGE),
                                      QLatin1String(kalahari::core::TYPE_COPYRIGHT),
                                      QLatin1String(kalahari::core::TYPE_DEDICATION),
                                      QLatin1String(kalahari::core::TYPE_PREFACE)};
        CHECK(NewElementDialog::elementTypes(NewElementKind::FrontMatterItem) == expected);
        REQUIRE(types->count() == expected.size());
        CHECK(dialog.elementType() == expected.first());
        CHECK(dialog.title() == QStringLiteral("Title Page"));

        types->setCurrentIndex(2);
        CHECK(dialog.elementType() == QLatin1String(kalahari::core::TYPE_DEDICATION));
        CHECK(dialog.title() == QStringLiteral("Dedication"));

        // A title the writer typed stays when the type changes
        title->setText(QStringLiteral("For "));
        title->end(false);
        type(title, QStringLiteral("M"));
        REQUIRE(dialog.title() == QStringLiteral("For M"));
        types->setCurrentIndex(3);
        CHECK(dialog.elementType() == QLatin1String(kalahari::core::TYPE_PREFACE));
        CHECK(dialog.title() == QStringLiteral("For M"));
    }

    SECTION("Back matter") {
        NewElementDialog dialog(NewElementKind::BackMatterItem);
        const QStringList expected = {QLatin1String(kalahari::core::TYPE_EPILOGUE),
                                      QLatin1String(kalahari::core::TYPE_GLOSSARY),
                                      QLatin1String(kalahari::core::TYPE_BIBLIOGRAPHY),
                                      QLatin1String(kalahari::core::TYPE_ABOUT_AUTHOR)};
        CHECK(NewElementDialog::elementTypes(NewElementKind::BackMatterItem) == expected);
        CHECK(dialog.elementType() == expected.first());
        CHECK(dialog.title() == QStringLiteral("Epilogue"));
    }
}

TEST_CASE("Navigator: the rename dialog starts with the current name", "[gui][dialogs]") {
    RenameElementDialog dialog(QStringLiteral("Chapter One"), QStringLiteral("template.chapter"));

    CHECK(showsText(dialog, QStringLiteral("Chapter One")));
    auto* name = dialog.findChild<QLineEdit*>();
    REQUIRE(name != nullptr);
    CHECK(name->text() == QStringLiteral("Chapter One"));
    CHECK(name->selectedText() == name->text());
    CHECK(dialog.acceptButton()->isEnabled());

    name->setText(QString());
    CHECK_FALSE(dialog.acceptButton()->isEnabled());

    name->setText(QStringLiteral(" The Beginning  "));
    CHECK(dialog.name() == QStringLiteral("The Beginning"));
}
