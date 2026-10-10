/// @file test_message_dialog.cpp
/// @brief The program's own messages, questions, typed texts, progress and colors, in place of the system ones

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/progress_dialog.h"
#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/core/settings_manager.h"

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
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

TEST_CASE("Own messages: the progress of a long task", "[gui][dialogs]") {
    ProgressDialog dialog(QStringLiteral("Export"), QStringLiteral("Exporting the archive..."));
    auto* bar = dialog.findChild<QProgressBar*>();
    REQUIRE(bar != nullptr);

    CHECK(dialog.windowTitle() == QStringLiteral("Export"));
    CHECK(showsText(dialog, QStringLiteral("Exporting the archive...")));
    CHECK(dialog.windowModality() == Qt::WindowModal);
    CHECK_FALSE(dialog.acceptButton()->isVisibleTo(&dialog));
    CHECK(dialog.cancelButton()->isVisibleTo(&dialog));

    dialog.setRange(0, 10);
    dialog.setValue(4);
    CHECK(dialog.value() == 4);
    CHECK(bar->maximum() == 10);

    SECTION("Cancel and Esc ask the task to stop") {
        dialog.show();
        press(dialog, Qt::Key_Escape);
        CHECK(dialog.wasCanceled());
        CHECK_FALSE(dialog.isVisible());
    }

    SECTION("A task that cannot stop has no Cancel, and Esc does not close it") {
        dialog.setCancelVisible(false);
        dialog.show();
        press(dialog, Qt::Key_Escape);
        CHECK_FALSE(dialog.wasCanceled());
        CHECK(dialog.isVisible());
        dialog.hide();
    }
}

namespace {

/// @brief Type a text into a field key by key, as the writer does
void typeText(QWidget* field, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(field, &event);
    }
}

/// @brief Press a key in a widget
void pressIn(QWidget* widget, Qt::Key key) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

} // anonymous namespace

TEST_CASE("Own messages: choosing a color", "[gui][dialogs]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    settings.set<std::vector<std::string>>("ui.recentColors", {});

    ColorDialog dialog(QColor(QStringLiteral("#123456")), QStringLiteral("Primary icon color"));

    CHECK(dialog.windowTitle() == QStringLiteral("Select Color"));
    CHECK(showsText(dialog, QStringLiteral("Primary icon color")));
    CHECK(dialog.color() == QColor(QStringLiteral("#123456")));
    CHECK(dialog.hexField()->text() == QStringLiteral("#123456"));
    CHECK(dialog.redField()->value() == 0x12);
    CHECK(dialog.greenField()->value() == 0x34);
    CHECK(dialog.blueField()->value() == 0x56);
    CHECK(dialog.paletteList()->count() == ColorDialog::paletteColors().size());
    CHECK(ColorDialog::paletteColors().size() == 60);
    CHECK(dialog.paletteList()->selectedItems().isEmpty());
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Select"));

    SECTION("Getting into the palette with the keyboard keeps the color") {
        QFocusEvent focusIn(QEvent::FocusIn, Qt::TabFocusReason);
        QApplication::sendEvent(dialog.paletteList(), &focusIn);
        CHECK(dialog.color() == QColor(QStringLiteral("#123456")));
    }

    SECTION("The arrows move in the palette and choose the color") {
        dialog.setColor(Qt::black);  // The last row starts with black
        REQUIRE(dialog.paletteList()->currentRow() == 50);
        pressIn(dialog.paletteList(), Qt::Key_Right);
        CHECK(dialog.color() == ColorDialog::paletteColors().at(51));
        pressIn(dialog.paletteList(), Qt::Key_Up);
        CHECK(dialog.color() == ColorDialog::paletteColors().at(41));
        CHECK(dialog.hexField()->text() == ColorDialog::paletteColors().at(41).name().toUpper());
    }

    SECTION("A typed HEX chooses the color once it is complete") {
        dialog.hexField()->clear();
        typeText(dialog.hexField(), QStringLiteral("#ff80"));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        CHECK(dialog.color() == QColor(QStringLiteral("#123456")));
        typeText(dialog.hexField(), QStringLiteral("00"));
        CHECK(dialog.acceptButton()->isEnabled());
        CHECK(dialog.color() == QColor(QStringLiteral("#ff8000")));
        CHECK(dialog.redField()->value() == 255);
        CHECK(dialog.greenField()->value() == 128);
    }

    SECTION("Red, green and blue choose the color") {
        dialog.redField()->setValue(200);
        dialog.blueField()->setValue(10);
        CHECK(dialog.color() == QColor(200, 0x34, 10));
        CHECK(dialog.hexField()->text() == QColor(200, 0x34, 10).name().toUpper());
    }

    SECTION("Previous brings back the color the setting had") {
        dialog.setColor(QColor(QStringLiteral("#abcdef")));
        dialog.previousButton()->click();
        CHECK(dialog.color() == QColor(QStringLiteral("#123456")));
    }

    SECTION("Select keeps the color among the recent ones, newest first") {
        CHECK_FALSE(dialog.recentList()->isVisibleTo(&dialog));
        dialog.setColor(QColor(QStringLiteral("#abcdef")));
        dialog.accept();
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK(ColorDialog::recentColors() == QStringList{QStringLiteral("#abcdef")});

        ColorDialog::addRecentColor(QColor(QStringLiteral("#112233")));
        ColorDialog::addRecentColor(QColor(QStringLiteral("#abcdef")));
        CHECK(ColorDialog::recentColors() ==
              QStringList({QStringLiteral("#abcdef"), QStringLiteral("#112233")}));

        ColorDialog next(QColor(QStringLiteral("#000000")));
        CHECK(next.recentList()->isVisibleTo(&next));
        CHECK(next.recentList()->count() == 2);
    }

    SECTION("A palette color picked before is marked in both lists next time") {
        const QColor picked = ColorDialog::paletteColors().at(41);
        dialog.setColor(picked);
        dialog.accept();

        ColorDialog next(dialog.color());
        REQUIRE(next.recentList()->count() == 1);
        CHECK(next.recentList()->item(0)->isSelected());
        REQUIRE(next.paletteList()->selectedItems().size() == 1);
        CHECK(next.paletteList()->selectedItems().first() == next.paletteList()->item(41));
    }

    SECTION("At most ten recent colors are kept") {
        for (int i = 0; i < 12; ++i) {
            ColorDialog::addRecentColor(QColor(i * 10, 0, 0));
        }
        CHECK(ColorDialog::recentColors().size() == ColorDialog::MAX_RECENT_COLORS);
        CHECK(ColorDialog::recentColors().first() == QColor(110, 0, 0).name());
    }

    SECTION("getColor gives the chosen color, or nothing when cancelled") {
        whenShown<ColorDialog>([](ColorDialog* shown) {
            shown->setColor(QColor(QStringLiteral("#00ff00")));
            shown->acceptButton()->click();
        });
        CHECK(ColorDialog::getColor(Qt::red, nullptr) == QColor(QStringLiteral("#00ff00")));

        whenShown<ColorDialog>([](ColorDialog* shown) { shown->reject(); });
        CHECK_FALSE(ColorDialog::getColor(Qt::red, nullptr).has_value());
    }

    settings.set<std::vector<std::string>>("ui.recentColors", {});
}

TEST_CASE("Own messages: a long message fits the screen and scrolls", "[gui][dialogs]") {
    const QRect screen = QApplication::primaryScreen()->availableGeometry();
    QStringList lines;
    for (int i = 0; i < 300; ++i) {
        lines.append(QStringLiteral("Line %1 of a long report").arg(i + 1));
    }
    MessageDialog dialog(MessageDialog::Kind::Error, QStringLiteral("Import Failed"),
                         lines.join(QLatin1Char('\n')));
    dialog.show();
    QApplication::processEvents();

    // The window stays on the screen with its buttons; the message scrolls
    CHECK(dialog.height() <= screen.height());
    CHECK(screen.contains(dialog.geometry()));
    const QRect accept(dialog.acceptButton()->mapTo(&dialog, QPoint(0, 0)),
                       dialog.acceptButton()->size());
    CHECK(dialog.rect().contains(accept));
    auto* area = dialog.findChild<QScrollArea*>();
    REQUIRE(area != nullptr);
    CHECK(area->verticalScrollBar()->isVisible());

    // A short message needs no scrolling
    MessageDialog shortMessage(MessageDialog::Kind::Information, QStringLiteral("Saved"),
                               QStringLiteral("The book was saved."));
    shortMessage.show();
    QApplication::processEvents();
    auto* shortArea = shortMessage.findChild<QScrollArea*>();
    REQUIRE(shortArea != nullptr);
    CHECK_FALSE(shortArea->verticalScrollBar()->isVisible());
}

TEST_CASE("Own messages: a window uses the height of the screen before it scrolls",
          "[gui][dialogs]") {
    // Taller than the two thirds of the screen Qt gives a window, lower than the screen
    const QRect screen = QApplication::primaryScreen()->availableGeometry();
    const auto report = [](int count) {
        QStringList lines;
        for (int i = 0; i < count; ++i) {
            lines.append(QStringLiteral("Line %1").arg(i + 1));
        }
        return lines.join(QLatin1Char('\n'));
    };
    int count = 1;
    while (MessageDialog(MessageDialog::Kind::Information, QStringLiteral("Report"), report(count))
               .sizeHint()
               .height() <= screen.height() * 2 / 3) {
        ++count;
    }
    MessageDialog dialog(MessageDialog::Kind::Information, QStringLiteral("Report"),
                         report(count));
    REQUIRE(dialog.sizeHint().height() > screen.height() * 2 / 3);
    REQUIRE(dialog.sizeHint().height() < screen.height() - 40);
    dialog.show();
    QApplication::processEvents();

    CHECK(dialog.height() == dialog.sizeHint().height());
    auto* area = dialog.findChild<QScrollArea*>();
    REQUIRE(area != nullptr);
    CHECK_FALSE(area->verticalScrollBar()->isVisible());

    // Shown details make the window taller, not the message scroll
    MessageDialog withDetails(MessageDialog::Kind::Error, QStringLiteral("Import Failed"),
                              QStringLiteral("The archive could not be read."));
    withDetails.setDetails(QStringLiteral("zip: bad header at byte 0"));
    withDetails.show();
    QApplication::processEvents();
    const int before = withDetails.height();
    withDetails.detailsButton()->click();
    // The window is resized once the layout has taken the details in, then the area
    QApplication::processEvents();
    QApplication::processEvents();
    CHECK(withDetails.height() > before);
    auto* detailsArea = withDetails.findChild<QScrollArea*>();
    REQUIRE(detailsArea != nullptr);
    CHECK_FALSE(detailsArea->verticalScrollBar()->isVisible());
    withDetails.detailsButton()->click();
    QApplication::processEvents();
    CHECK(withDetails.height() == before);

    // The color window shows all of itself
    ColorDialog colors(QColor(Qt::red), QStringLiteral("Primary"));
    colors.show();
    QApplication::processEvents();
    auto* colorArea = colors.findChild<QScrollArea*>();
    REQUIRE(colorArea != nullptr);
    CHECK_FALSE(colorArea->verticalScrollBar()->isVisible());
}
