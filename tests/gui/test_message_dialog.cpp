/// @file test_message_dialog.cpp
/// @brief The program's own messages, questions and typed texts, in place of the system ones

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/dialogs/message_dialog.h"

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>

#include <functional>

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

/// @brief Press a key in a dialog, as the writer does
void press(QDialog& dialog, Qt::Key key) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(&dialog, &event);
}

/// @brief Act on the dialog a static function opens, once it is shown
template <typename Dialog>
void whenShown(std::function<void(Dialog*)> action) {
    QTimer::singleShot(0, [action]() {
        auto* dialog = qobject_cast<Dialog*>(QApplication::activeModalWidget());
        REQUIRE(dialog != nullptr);
        action(dialog);
    });
}

} // anonymous namespace

TEST_CASE("Own messages: a message with OK, its text and Copy", "[gui][dialogs]") {
    MessageDialog dialog(MessageDialog::Kind::Error, QStringLiteral("Import Failed"),
                         QStringLiteral("The archive could not be read."));

    CHECK(dialog.windowTitle() == QStringLiteral("Import Failed"));
    CHECK(showsText(dialog, QStringLiteral("Import Failed")));
    CHECK(showsText(dialog, QStringLiteral("The archive could not be read.")));

    // Only OK and Copy: no Cancel, Apply, alternative or Show Details
    CHECK(dialog.acceptButton()->isVisibleTo(&dialog));
    CHECK(dialog.acceptButton()->isDefault());
    CHECK_FALSE(dialog.cancelButton()->isVisibleTo(&dialog));
    CHECK_FALSE(dialog.applyButton()->isVisibleTo(&dialog));
    CHECK_FALSE(dialog.alternativeButton()->isVisibleTo(&dialog));
    CHECK_FALSE(dialog.detailsButton()->isVisibleTo(&dialog));
    CHECK(dialog.copyButton()->isVisibleTo(&dialog));

    SECTION("The message can be selected with the mouse and the keyboard") {
        bool selectable = false;
        for (const QLabel* label : dialog.findChildren<QLabel*>()) {
            if (label->text() == QStringLiteral("The archive could not be read.")) {
                selectable = label->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard)
                    && label->textInteractionFlags().testFlag(Qt::TextSelectableByMouse);
            }
        }
        CHECK(selectable);
    }

    SECTION("Copy puts the title and the message on the clipboard") {
        QApplication::clipboard()->clear();
        dialog.copyButton()->click();
        CHECK(QApplication::clipboard()->text() ==
              QStringLiteral("Import Failed\n\nThe archive could not be read."));
        CHECK(dialog.copyButton()->text() == QStringLiteral("Copied"));
    }

    SECTION("Copy and Show Details are reached with Alt and a letter") {
        dialog.setDetails(QStringLiteral("zip: not an archive"));
        CHECK(dialog.copyButton()->text().contains(QLatin1Char('&')));
        CHECK(dialog.detailsButton()->text().contains(QLatin1Char('&')));
    }

    SECTION("OK closes it with the accept answer") {
        dialog.show();
        dialog.acceptButton()->click();
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.answer() == MessageDialog::Answer::Accept);
    }

    SECTION("Esc closes it as Cancel") {
        dialog.show();
        press(dialog, Qt::Key_Escape);
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.answer() == MessageDialog::Answer::Cancel);
    }
}

TEST_CASE("Own messages: the details under Show Details", "[gui][dialogs]") {
    MessageDialog dialog(MessageDialog::Kind::Warning, QStringLiteral("Import Failed"),
                         QStringLiteral("The archive could not be read."));
    dialog.setDetails(QStringLiteral("zip: not an archive"));
    auto* details = dialog.findChild<QPlainTextEdit*>();
    REQUIRE(details != nullptr);

    CHECK(dialog.detailsButton()->isVisibleTo(&dialog));
    CHECK_FALSE(details->isVisibleTo(&dialog));
    CHECK(details->isReadOnly());

    dialog.detailsButton()->click();
    CHECK(details->isVisibleTo(&dialog));
    CHECK(details->toPlainText() == QStringLiteral("zip: not an archive"));
    CHECK(dialog.detailsButton()->text().contains(QStringLiteral("Hide")));

    dialog.detailsButton()->click();
    CHECK_FALSE(details->isVisibleTo(&dialog));

    SECTION("Copy takes the details too, also while they are hidden") {
        CHECK(dialog.clipboardText() == QStringLiteral(
            "Import Failed\n\nThe archive could not be read.\n\nzip: not an archive"));
    }

    SECTION("Empty details hide Show Details again") {
        dialog.setDetails(QString());
        CHECK_FALSE(dialog.detailsButton()->isVisibleTo(&dialog));
        CHECK(dialog.clipboardText() ==
              QStringLiteral("Import Failed\n\nThe archive could not be read."));
    }
}

TEST_CASE("Own messages: a question with named buttons", "[gui][dialogs]") {
    MessageDialog dialog(MessageDialog::Kind::Question, QStringLiteral("Delete Toolbar"),
                         QStringLiteral("Delete the toolbar 'Mine'?"));
    dialog.setQuestionButtons(QStringLiteral("&Delete"));

    CHECK(dialog.acceptButton()->text() == QStringLiteral("&Delete"));
    CHECK(dialog.cancelButton()->isVisibleTo(&dialog));
    CHECK(dialog.acceptButton()->isDefault());

    SECTION("Cancel can get its own text") {
        dialog.setQuestionButtons(QStringLiteral("&Delete"), QStringLiteral("&Keep"));
        CHECK(dialog.cancelButton()->text() == QStringLiteral("&Keep"));
    }

    SECTION("With Cancel as the default button Enter does nothing to the toolbar") {
        dialog.setDefaultButton(MessageDialog::DefaultButton::Cancel);
        CHECK(dialog.cancelButton()->isDefault());
        CHECK_FALSE(dialog.acceptButton()->isDefault());

        dialog.show();
        press(dialog, Qt::Key_Return);
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.answer() == MessageDialog::Answer::Cancel);
    }

    SECTION("Enter presses the action's button by default") {
        dialog.show();
        press(dialog, Qt::Key_Return);
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.answer() == MessageDialog::Answer::Accept);
    }

    SECTION("The alternative button gives its own answer") {
        dialog.setAlternativeText(QStringLiteral("Do&n't Save"));
        CHECK(dialog.alternativeButton()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.alternativeButton()->isDefault());

        dialog.show();
        dialog.alternativeButton()->click();
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.answer() == MessageDialog::Answer::Alternative);
    }
}

TEST_CASE("Own messages: the functions that show them in one call", "[gui][dialogs]") {
    SECTION("A message is shown and closed with OK") {
        bool shown = false;
        whenShown<MessageDialog>([&shown](MessageDialog* dialog) {
            shown = dialog->windowTitle() == QStringLiteral("Saved");
            dialog->acceptButton()->click();
        });
        MessageDialog::information(nullptr, QStringLiteral("Saved"), QStringLiteral("Done."));
        CHECK(shown);
    }

    SECTION("An error shows its details") {
        bool hasDetails = false;
        whenShown<MessageDialog>([&hasDetails](MessageDialog* dialog) {
            hasDetails = dialog->detailsButton()->isVisible();
            dialog->acceptButton()->click();
        });
        MessageDialog::error(nullptr, QStringLiteral("Import Failed"),
                             QStringLiteral("The archive could not be read."),
                             QStringLiteral("zip: not an archive"));
        CHECK(hasDetails);
    }

    SECTION("confirm is true only for the action's button") {
        whenShown<MessageDialog>([](MessageDialog* dialog) { dialog->acceptButton()->click(); });
        CHECK(MessageDialog::confirm(nullptr, QStringLiteral("Delete Toolbar"),
                                     QStringLiteral("Delete it?"), QStringLiteral("&Delete")));

        whenShown<MessageDialog>([](MessageDialog* dialog) { dialog->cancelButton()->click(); });
        CHECK_FALSE(MessageDialog::confirm(nullptr, QStringLiteral("Delete Toolbar"),
                                           QStringLiteral("Delete it?"),
                                           QStringLiteral("&Delete")));
    }

    SECTION("confirm can make Cancel the default button") {
        bool cancelIsDefault = false;
        whenShown<MessageDialog>([&cancelIsDefault](MessageDialog* dialog) {
            cancelIsDefault = dialog->cancelButton()->isDefault();
            dialog->cancelButton()->click();
        });
        MessageDialog::confirm(nullptr, QStringLiteral("Reset Toolbars"),
                               QStringLiteral("Reset all toolbars?"), QStringLiteral("&Reset"),
                               MessageDialog::Kind::Warning,
                               MessageDialog::DefaultButton::Cancel);
        CHECK(cancelIsDefault);
    }

    SECTION("ask gives the button that closed it") {
        whenShown<MessageDialog>(
            [](MessageDialog* dialog) { dialog->alternativeButton()->click(); });
        CHECK(MessageDialog::ask(nullptr, QStringLiteral("Unsaved Changes"),
                                 QStringLiteral("Save the changes?"), QStringLiteral("&Save"),
                                 QStringLiteral("Do&n't Save")) ==
              MessageDialog::Answer::Alternative);

        whenShown<MessageDialog>([](MessageDialog* dialog) { dialog->reject(); });
        CHECK(MessageDialog::ask(nullptr, QStringLiteral("Unsaved Changes"),
                                 QStringLiteral("Save the changes?"), QStringLiteral("&Save"),
                                 QStringLiteral("Do&n't Save")) ==
              MessageDialog::Answer::Cancel);
    }
}

TEST_CASE("Own messages: a text to type", "[gui][dialogs]") {
    TextInputDialog dialog(QStringLiteral("Rename Toolbar"), QStringLiteral("New name"),
                           QStringLiteral("Mine"));

    CHECK(dialog.windowTitle() == QStringLiteral("Rename Toolbar"));
    CHECK(showsText(dialog, QStringLiteral("New name")));
    CHECK(dialog.field()->text() == QStringLiteral("Mine"));
    CHECK(dialog.field()->selectedText() == QStringLiteral("Mine"));
    CHECK(dialog.acceptButton()->isEnabled());

    SECTION("An empty name cannot be accepted") {
        dialog.field()->setText(QStringLiteral("   "));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
    }

    SECTION("The text is given without spaces at its ends") {
        dialog.field()->setText(QStringLiteral("  Writing  "));
        CHECK(dialog.text() == QStringLiteral("Writing"));
    }

    SECTION("getText gives the text, or nothing when cancelled") {
        whenShown<TextInputDialog>([](TextInputDialog* shown) {
            shown->field()->setText(QStringLiteral(" Notes "));
            shown->acceptButton()->click();
        });
        CHECK(TextInputDialog::getText(nullptr, QStringLiteral("New Toolbar"),
                                       QStringLiteral("Name"), QString(),
                                       QStringLiteral("&Create")) == QStringLiteral("Notes"));

        whenShown<TextInputDialog>([](TextInputDialog* shown) { shown->cancelButton()->click(); });
        CHECK_FALSE(TextInputDialog::getText(nullptr, QStringLiteral("New Toolbar"),
                                             QStringLiteral("Name")).has_value());
    }
}
