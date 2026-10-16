/// @file test_message_dialog.cpp
/// @brief The program's own messages, questions, typed texts, progress and colors, in place of the system ones

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/progress_dialog.h"
#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/core/settings_manager.h"

#include "../editor/editor_test_utils.h"

#include <QApplication>
#include <QAbstractButton>
#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>

#include <functional>
#include <string>
#include <vector>

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

TEST_CASE("Own messages: a message with OK, its text and Copy", "[gui][dialogs][clipboard]") {
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

namespace {

/// @brief The color window's settings as a writer has them the first time
void forgetColorChoices() {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    settings.set<std::vector<std::string>>("ui.recentColors", {});
    settings.set<std::vector<std::string>>("ui.myColors", {});
    settings.set<std::string>("ui.colorPalette", "basic");
    settings.set<bool>("ui.colorValuesShown", false);
}

/// @brief The color at @p index of the palette @p palette of a color window
QColor paletteColor(const ColorDialog& dialog, int palette, int index) {
    return dialog.palettes().at(palette).colors.at(index).color;
}

/// @brief What a swatch tells a screen reader: its name
QString swatchName(const QListWidget* list, int row) {
    return list->item(row)->data(Qt::AccessibleTextRole).toString();
}

} // anonymous namespace

TEST_CASE("Own messages: the palettes of the color window", "[gui][dialogs]") {
    const QList<ColorPalette> palettes = colorPalettes();
    REQUIRE(palettes.size() == 6);
    QStringList ids;
    for (const ColorPalette& palette : palettes) {
        ids.append(palette.id);
        CHECK_FALSE(palette.name.isEmpty());
        CHECK_FALSE(palette.description.isEmpty());
        REQUIRE(palette.colors.size() == PALETTE_FAMILIES * PALETTE_SHADES);

        // Sixty different colors, each with a name of its own
        QSet<QRgb> colors;
        QSet<QString> names;
        for (const NamedColor& named : palette.colors) {
            colors.insert(named.color.rgb());
            names.insert(named.name);
        }
        CHECK(colors.size() == PALETTE_FAMILIES * PALETTE_SHADES);
        CHECK(names.size() == PALETTE_FAMILIES * PALETTE_SHADES);
        CHECK_FALSE(names.contains(QString()));

        // Each family, a column, goes from light to dark
        for (int family = 0; family < PALETTE_FAMILIES; ++family) {
            for (int shade = 0; shade + 1 < PALETTE_SHADES; ++shade) {
                const QColor lighter = palette.colors.at(shade * PALETTE_FAMILIES + family).color;
                const QColor darker =
                    palette.colors.at((shade + 1) * PALETTE_FAMILIES + family).color;
                CHECK(lighter.lightness() > darker.lightness());
            }
        }
    }
    CHECK(ids == QStringList({QStringLiteral("basic"), QStringLiteral("kalahari"),
                              QStringLiteral("okavango"), QStringLiteral("pastel"),
                              QStringLiteral("ink"), QStringLiteral("savannaDusk")}));

    // A color of a palette has its name; one of no palette has none, but a closest one
    const NamedColor& ochre = palettes.at(1).colors.at(2 * PALETTE_FAMILIES + 3);
    CHECK(ochre.name == QStringLiteral("Ochre, medium light"));
    CHECK(paletteColorName(palettes, ochre.color) == ochre.name);
    CHECK(closestPaletteColorName(palettes, ochre.color) == ochre.name);
    CHECK(paletteColorName(palettes, QColor(QStringLiteral("#123456"))).isEmpty());
    CHECK_FALSE(closestPaletteColorName(palettes, QColor(QStringLiteral("#123456"))).isEmpty());
    CHECK(colorFromHsl(0, 100, 50) == QColor(255, 0, 0));
    CHECK(colorFromHsl(0, 0, 100) == QColor(255, 255, 255));
}

TEST_CASE("Own messages: choosing a color", "[gui][dialogs]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    forgetColorChoices();

    ColorDialog dialog(QColor(QStringLiteral("#123456")), QStringLiteral("Primary icon color"));

    CHECK(dialog.windowTitle() == QStringLiteral("Select Color"));
    CHECK(showsText(dialog, QStringLiteral("Primary icon color")));
    CHECK(dialog.color() == QColor(QStringLiteral("#123456")));
    CHECK(dialog.hexField()->text() == QStringLiteral("#123456"));
    CHECK(dialog.redField()->value() == 0x12);
    CHECK(dialog.greenField()->value() == 0x34);
    CHECK(dialog.blueField()->value() == 0x56);
    CHECK(dialog.paletteList()->selectedItems().isEmpty());
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Select"));

    // A color of no palette is the writer's own, with the closest named one
    CHECK(dialog.nameLabel()->text() == QStringLiteral("Own color"));
    CHECK(dialog.closestLabel()->text() ==
          QStringLiteral("Closest in the palettes: %1")
              .arg(closestPaletteColorName(dialog.palettes(), dialog.color())));

    // The numbers are not shown the first time
    CHECK_FALSE(dialog.valuesButton()->isChecked());
    CHECK_FALSE(dialog.hexField()->isVisibleTo(&dialog));

    SECTION("Six palettes of sixty named colors, Basic first") {
        REQUIRE(dialog.paletteBox()->count() == 6);
        const QStringList names = {QStringLiteral("Basic"),        QStringLiteral("Kalahari"),
                                   QStringLiteral("Okavango"),     QStringLiteral("Pastel"),
                                   QStringLiteral("Ink and Paper"), QStringLiteral("Savanna Dusk")};
        for (int i = 0; i < names.size(); ++i) {
            CHECK(dialog.paletteBox()->itemText(i) == names.at(i));
        }
        CHECK(dialog.paletteBox()->currentIndex() == 0);
        CHECK(showsText(dialog, dialog.palettes().at(0).description));
        REQUIRE(dialog.paletteList()->count() == PALETTE_FAMILIES * PALETTE_SHADES);
        // Each swatch says its name
        CHECK(swatchName(dialog.paletteList(), 0) == QStringLiteral("White"));
        CHECK(swatchName(dialog.paletteList(), 50) == QStringLiteral("Black"));
        CHECK(swatchName(dialog.paletteList(), 1) == QStringLiteral("Red, very light"));
        CHECK(dialog.paletteList()->item(1)->toolTip().startsWith(QStringLiteral("Red, very light\n#")));
    }

    SECTION("The palette chosen is shown, and is shown again next time") {
        dialog.paletteBox()->setCurrentIndex(1);
        REQUIRE(dialog.paletteList()->count() == PALETTE_FAMILIES * PALETTE_SHADES);
        CHECK(dialog.paletteList()->item(0)->data(Qt::UserRole).value<QColor>() ==
              paletteColor(dialog, 1, 0));
        CHECK(swatchName(dialog.paletteList(), 0) == QStringLiteral("Stone, very light"));
        CHECK(showsText(dialog, dialog.palettes().at(1).description));
        CHECK(settings.get<std::string>("ui.colorPalette", "") == "kalahari");

        ColorDialog next{QColor(Qt::red)};
        CHECK(next.paletteBox()->currentIndex() == 1);
        CHECK(next.paletteList()->item(0)->data(Qt::UserRole).value<QColor>() ==
              paletteColor(next, 1, 0));
    }

    SECTION("Getting into the palette with the keyboard keeps the color") {
        QFocusEvent focusIn(QEvent::FocusIn, Qt::TabFocusReason);
        QApplication::sendEvent(dialog.paletteList(), &focusIn);
        CHECK(dialog.color() == QColor(QStringLiteral("#123456")));
    }

    SECTION("The arrows move in the palette, choose the color and show its name") {
        dialog.setColor(Qt::black);  // The last row of Basic starts with black
        REQUIRE(dialog.paletteList()->currentRow() == 50);
        CHECK(dialog.nameLabel()->text() == QStringLiteral("Black"));
        CHECK(dialog.closestLabel()->text().isEmpty());
        pressIn(dialog.paletteList(), Qt::Key_Right);
        CHECK(dialog.color() == paletteColor(dialog, 0, 51));
        CHECK(dialog.nameLabel()->text() == QStringLiteral("Red, very dark"));
        pressIn(dialog.paletteList(), Qt::Key_Up);
        CHECK(dialog.color() == paletteColor(dialog, 0, 41));
        CHECK(dialog.nameLabel()->text() == QStringLiteral("Red, dark"));
        CHECK(dialog.hexField()->text() == paletteColor(dialog, 0, 41).name().toUpper());
    }

    SECTION("A shade makes the color lighter or darker and keeps it") {
        // Blue of Basic in its fourth shade, lightness 48: the shade of 47 is its own
        dialog.setColor(paletteColor(dialog, 0, 3 * PALETTE_FAMILIES + 7));
        REQUIRE(dialog.nameLabel()->text() == QStringLiteral("Blue, medium dark"));
        REQUIRE(dialog.shadesList()->count() == ColorDialog::SHADES);
        CHECK(dialog.shadesList()->currentRow() == 5);
        CHECK(dialog.shadesList()->item(5)->isSelected());
        CHECK(swatchName(dialog.shadesList(), 4) == QStringLiteral("Lighter"));
        CHECK(swatchName(dialog.shadesList(), 5) == QStringLiteral("This lightness"));
        CHECK(swatchName(dialog.shadesList(), 6) == QStringLiteral("Darker"));
        const auto shade = [&dialog](int row) {
            return dialog.shadesList()->item(row)->data(Qt::UserRole).value<QColor>();
        };
        CHECK(shade(0).lightness() > shade(ColorDialog::SHADES - 1).lightness());

        const int hue = dialog.hueSlider()->value();
        const int intensity = dialog.intensitySlider()->value();
        pressIn(dialog.shadesList(), Qt::Key_Left);
        CHECK(dialog.color() == colorFromHsl(hue, intensity, 56));
        CHECK(dialog.shadesList()->currentRow() == 4);
        CHECK(dialog.lightnessSlider()->value() == 56);
        CHECK(dialog.hueSlider()->value() == hue);
        CHECK(dialog.intensitySlider()->value() == intensity);
    }

    SECTION("The sliders change the hue, the intensity and the lightness") {
        dialog.setColor(Qt::red);
        CHECK(dialog.hueSlider()->value() == 0);
        CHECK(dialog.intensitySlider()->value() == 100);
        CHECK(dialog.lightnessSlider()->value() == 50);

        dialog.hueSlider()->setValue(120);
        CHECK(dialog.color() == colorFromHsl(120, 100, 50));
        dialog.lightnessSlider()->setValue(25);
        CHECK(dialog.color() == colorFromHsl(120, 100, 25));
        pressIn(dialog.lightnessSlider(), Qt::Key_Right);
        CHECK(dialog.lightnessSlider()->value() == 26);
        CHECK(dialog.color() == colorFromHsl(120, 100, 26));
        CHECK(dialog.hexField()->text() == colorFromHsl(120, 100, 26).name().toUpper());

        // A grey has no hue: the slider keeps the one it had, so the color comes back
        dialog.intensitySlider()->setValue(0);
        CHECK(dialog.color().hslSaturation() == 0);
        dialog.setColor(dialog.color());
        CHECK(dialog.hueSlider()->value() == 120);
        dialog.intensitySlider()->setValue(100);
        CHECK(dialog.color() == colorFromHsl(120, 100, 26));
    }

    SECTION("The numbers are shown when asked for, and so again next time") {
        dialog.valuesButton()->click();
        CHECK(dialog.valuesButton()->isChecked());
        CHECK(dialog.hexField()->isVisibleTo(&dialog));
        CHECK(dialog.redField()->isVisibleTo(&dialog));
        CHECK(settings.get<bool>("ui.colorValuesShown", false));

        ColorDialog next{QColor(Qt::red)};
        CHECK(next.valuesButton()->isChecked());
        CHECK(next.hexField()->isVisibleTo(&next));
        next.valuesButton()->click();
        CHECK_FALSE(next.hexField()->isVisibleTo(&next));
        CHECK_FALSE(settings.get<bool>("ui.colorValuesShown", true));
    }

    SECTION("A typed HEX chooses the color once it is complete") {
        dialog.valuesButton()->click();
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
        dialog.valuesButton()->click();
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
        CHECK(swatchName(next.recentList(), 0) == QStringLiteral("Own color"));
    }

    SECTION("A palette color picked before is marked in both lists next time, with its name") {
        const QColor picked = paletteColor(dialog, 0, 41);
        dialog.setColor(picked);
        dialog.accept();

        ColorDialog next(dialog.color());
        REQUIRE(next.recentList()->count() == 1);
        CHECK(next.recentList()->item(0)->isSelected());
        CHECK(swatchName(next.recentList(), 0) == QStringLiteral("Red, dark"));
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

    SECTION("My colors are added, kept for next time and removed") {
        const QString addHere = QStringLiteral("Add the colors you often use here.");
        CHECK(dialog.myColorsList()->count() == 0);
        CHECK(dialog.myColorsNote()->isVisibleTo(&dialog));
        CHECK(dialog.myColorsNote()->text() == addHere);
        CHECK_FALSE(dialog.removeButton()->isEnabled());

        dialog.setColor(QColor(QStringLiteral("#abcdef")));
        dialog.addButton()->click();
        CHECK(ColorDialog::myColors() == QStringList{QStringLiteral("#abcdef")});
        REQUIRE(dialog.myColorsList()->count() == 1);
        CHECK(dialog.myColorsList()->item(0)->isSelected());
        CHECK(dialog.removeButton()->isEnabled());
        CHECK_FALSE(dialog.myColorsNote()->isVisibleTo(&dialog));

        // The same color again: the note says it is there already
        dialog.addButton()->click();
        CHECK(dialog.myColorsList()->count() == 1);
        CHECK(dialog.myColorsNote()->isVisibleTo(&dialog));
        CHECK(dialog.myColorsNote()->text() ==
              QStringLiteral("This color is already among My colors."));

        // Next time it is there, and picking it chooses it
        ColorDialog next{QColor(Qt::red)};
        REQUIRE(next.myColorsList()->count() == 1);
        CHECK_FALSE(next.removeButton()->isEnabled());
        next.myColorsList()->setCurrentRow(0);
        CHECK(next.color() == QColor(QStringLiteral("#abcdef")));
        CHECK(next.removeButton()->isEnabled());

        // Removing it keeps the color chosen
        next.removeButton()->click();
        CHECK(next.myColorsList()->count() == 0);
        CHECK(ColorDialog::myColors().isEmpty());
        CHECK(next.myColorsNote()->text() == addHere);
        CHECK_FALSE(next.removeButton()->isEnabled());
        CHECK(next.color() == QColor(QStringLiteral("#abcdef")));
    }

    SECTION("My colors keep ten, and a full list says so") {
        std::vector<std::string> ten;
        ten.reserve(ColorDialog::MAX_MY_COLORS);
        for (int i = 0; i < ColorDialog::MAX_MY_COLORS; ++i) {
            ten.push_back(QColor(i * 20, 10, 10).name().toStdString());
        }
        settings.set<std::vector<std::string>>("ui.myColors", ten);
        ColorDialog full{QColor(Qt::blue)};
        CHECK(full.myColorsList()->count() == ColorDialog::MAX_MY_COLORS);
        full.addButton()->click();
        CHECK(full.myColorsList()->count() == ColorDialog::MAX_MY_COLORS);
        CHECK(full.myColorsNote()->text() ==
              QStringLiteral("My colors are full: remove one to add another."));
        CHECK_FALSE(ColorDialog::myColors().contains(QColor(Qt::blue).name()));
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

    forgetColorChoices();
}

namespace {

/// @brief The access keys of a dialog's labels and buttons, lower case, in their order
QStringList accessKeys(const QDialog& dialog) {
    QStringList texts;
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->buddy() != nullptr) {
            texts.append(label->text());
        }
    }
    for (const QAbstractButton* button : dialog.findChildren<QAbstractButton*>()) {
        if (button->isVisibleTo(&dialog)) {
            texts.append(button->text());
        }
    }
    QStringList keys;
    for (const QString& text : texts) {
        const qsizetype at = text.indexOf(QRegularExpression(QStringLiteral("&[^&]")));
        if (at >= 0) {
            keys.append(text.at(at + 1).toLower());
        }
    }
    return keys;
}

} // anonymous namespace

TEST_CASE("Own messages: the parts of the color window have keys of their own, also in Polish",
          "[gui][dialogs][keys]") {
    forgetColorChoices();
    const auto checkKeys = [](const ColorDialog& dialog) {
        const QStringList keys = accessKeys(dialog);
        INFO(keys.join(QLatin1Char(' ')).toStdString());
        // Palette, recent colors, My colors with Add and Remove, Previous, shades, the three
        // sliders, Numeric values, HEX, red, green and blue
        CHECK(keys.size() == 15);
        CHECK(QSet<QString>(keys.begin(), keys.end()).size() == keys.size());
    };
    {
        ColorDialog dialog{QColor(Qt::red)};
        checkKeys(dialog);
    }

    QTranslator polish;
    REQUIRE(polish.load(QStringLiteral(":/i18n/kalahari_pl.qm")));
    QCoreApplication::installTranslator(&polish);
    {
        ColorDialog dialog{QColor(Qt::red)};
        checkKeys(dialog);
        CHECK(dialog.paletteBox()->itemText(0) == QStringLiteral("Podstawowa"));
        CHECK(swatchName(dialog.paletteList(), 1) == QStringLiteral("Czerwień – bardzo jasny odcień"));
        CHECK(swatchName(dialog.paletteList(), 0) == QStringLiteral("Biel"));
    }
    QCoreApplication::removeTranslator(&polish);
    forgetColorChoices();
}

TEST_CASE("Own messages: Delete removes the selected one of My colors", "[gui][dialogs]") {
    // The key is the window's shortcut: it needs an active window, which the offscreen
    // platform has without a window on screen
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs an active window without a window on screen: run with "
             "QT_QPA_PLATFORM=offscreen");
    }
    auto& settings = kalahari::core::SettingsManager::getInstance();
    forgetColorChoices();
    settings.set<std::vector<std::string>>("ui.myColors", {"#abcdef", "#112233"});

    ColorDialog dialog{QColor(Qt::red)};
    dialog.show();
    dialog.activateWindow();
    REQUIRE(kalahari::test::waitUntil(
        [&dialog] { return QApplication::activeWindow() == &dialog; }));
    dialog.myColorsList()->setFocus(Qt::OtherFocusReason);
    dialog.myColorsList()->setCurrentRow(1);
    REQUIRE(dialog.color() == QColor(QStringLiteral("#112233")));

    QKeyEvent pressed(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QApplication::sendEvent(dialog.myColorsList(), &pressed);
    CHECK(ColorDialog::myColors() == QStringList{QStringLiteral("#abcdef")});
    CHECK(dialog.myColorsList()->count() == 1);
    dialog.hide();
    forgetColorChoices();
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

    // The color window shows all of itself, also with its numbers shown
    forgetColorChoices();
    ColorDialog colors(QColor(Qt::red), QStringLiteral("Primary"));
    colors.show();
    QApplication::processEvents();
    auto* colorArea = colors.findChild<QScrollArea*>();
    REQUIRE(colorArea != nullptr);
    CHECK_FALSE(colorArea->verticalScrollBar()->isVisible());
    const int withoutValues = colors.height();
    colors.valuesButton()->click();
    QApplication::processEvents();
    QApplication::processEvents();
    CHECK(colors.height() > withoutValues);
    CHECK_FALSE(colorArea->verticalScrollBar()->isVisible());
    colors.hide();
    forgetColorChoices();
}
