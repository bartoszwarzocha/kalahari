// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_portrait_dialog.cpp
/// @brief The window Portrait: a portrait of a card put together from parts, its portraits,
/// Undo and the small screen

#include "kalahari_tests/changed_setting.h"
#include "kalahari_tests/editor/editor_test_utils.h"
#include "kalahari_tests/polish.h"

#include "kalahari/core/card_data.h"
#include "kalahari/gui/avatar.h"
#include "kalahari/gui/avatar_art.h"
#include "kalahari/gui/dialogs/portrait_dialog.h"
#include "kalahari/gui/dialogs/portrait_palettes_dialog.h"
#include "kalahari/gui/icon_registrar.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QImage>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QShortcut>
#include <QToolButton>

#include <string>

using kalahari::core::CardPortrait;
using kalahari::gui::Avatar;
using kalahari::gui::AvatarFrame;
using kalahari::gui::AvatarLook;
using kalahari::gui::dialogs::PortraitDialog;
using kalahari::gui::dialogs::PortraitSubject;
using kalahari::test::ChangedSetting;

namespace {

/// Anna, a woman of 67, with a picture for her portrait
PortraitSubject anna() {
    PortraitSubject subject;
    subject.name = QStringLiteral("Anna Wolska");
    subject.gender = QStringLiteral("woman");
    subject.age = QStringLiteral("67");
    subject.hasAppearance = true;
    subject.appearance = QStringLiteral("Tall.");
    subject.color = QColor(QStringLiteral("#c0504d"));
    CardPortrait picture;
    picture.image = QStringLiteral("e7");
    subject.portraits = {picture};
    return subject;
}

/// The value of part @p key of the portrait shown in @p dialog
QString valueOf(const PortraitDialog& dialog, const char* key) {
    return Avatar::resolved(dialog.portraits().at(dialog.current()).avatar)
        .value(QLatin1String(key))
        .toString();
}

/// Show the group @p id of the parts in @p dialog
void showGroup(PortraitDialog& dialog, const QString& id) {
    for (int row = 0; row < dialog.groupList()->count(); ++row) {
        if (dialog.groupList()->item(row)->data(Qt::UserRole).toString() == id) {
            dialog.groupList()->setCurrentRow(row);
            return;
        }
    }
    FAIL("No group " << id.toStdString());
}

/// Press @p key on @p widget
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &event);
}

}  // namespace

TEST_CASE("Portrait window: a new portrait from the fields Gender and Age, beside a picture",
          "[gui][avatar][portrait_dialog]") {
    kalahari::gui::registerAllIcons();
    const ChangedSetting<std::string> style(Avatar::STYLE_SETTING, AvatarLook::LINE);
    const ChangedSetting<std::string> frame(Avatar::FRAME_SETTING, Avatar::FRAME_FULL);
    const PortraitDialog dialog(anna());
    REQUIRE(dialog.portraits().size() == 2);
    CHECK(dialog.current() == 1);
    CHECK(dialog.portraits().at(0).image == QStringLiteral("e7"));
    const CardPortrait& made = dialog.portraits().at(1);
    CHECK(made.isComposed());
    CHECK(made.style == QLatin1String(AvatarLook::LINE));
    CHECK(made.avatar == Avatar::proposal(QStringLiteral("woman"), QStringLiteral("67")));
    CHECK(dialog.frame() == AvatarFrame::Full);
    CHECK(dialog.frameButtons()->checkedId() == 1);
    CHECK(dialog.styleButtons()->checkedId() == 1);
    // The strip has the portraits of parts; the picture is the main one
    REQUIRE(dialog.portraitList()->count() == 1);
    CHECK_FALSE(dialog.portraitList()->item(0)->text().startsWith(QStringLiteral("★")));
    CHECK_FALSE(dialog.preview()->pixmap().isNull());
    CHECK(dialog.windowTitle().contains(QStringLiteral("Anna Wolska")));
    CHECK(dialog.description().contains(QStringLiteral("deep wrinkles")));
    CHECK(dialog.descriptionLabel()->text() == dialog.description());
    CHECK_FALSE(dialog.undoButton()->isEnabled());
    CHECK_FALSE(dialog.appendsDescription());
    dialog.appendBox()->setChecked(true);
    CHECK(dialog.appendsDescription());
    CHECK(dialog.appendBox()->toolTip().contains(QStringLiteral("Tall.")));

    const QImage picture = dialog.picture();
    CHECK(picture.height() == 800);
    CHECK(picture.width() < picture.height());
}

TEST_CASE("Portrait window: the tiles, the colors, Undo, Randomize and Start Over",
          "[gui][avatar][portrait_dialog]") {
    kalahari::gui::registerAllIcons();
    const ChangedSetting<std::string> frame(Avatar::FRAME_SETTING, Avatar::FRAME_BUST);
    PortraitSubject subject = anna();
    CardPortrait main;
    main.label = QStringLiteral("main");
    main.avatar = Avatar::defaults();
    subject.portraits.prepend(main);
    subject.current = 0;
    PortraitDialog dialog(subject);
    dialog.setRandomSeed(11);
    REQUIRE(dialog.portraits().size() == 2);
    CHECK(dialog.current() == 0);
    CHECK(dialog.portraitList()->item(0)->text() == QStringLiteral("★ main"));
    CHECK(dialog.labelEdit()->text() == QStringLiteral("main"));

    // A tile of each value of the parts of the group, the chosen one checked
    showGroup(dialog, QStringLiteral("hair"));
    QToolButton* longHair = dialog.tile(QStringLiteral("hair"), QStringLiteral("long"));
    REQUIRE(longHair != nullptr);
    CHECK_FALSE(longHair->icon().isNull());
    CHECK(dialog.tile(QStringLiteral("hair"), QStringLiteral("short"))->isChecked());
    CHECK(dialog.tile(QStringLiteral("face"), QStringLiteral("oval")) == nullptr);
    longHair->click();
    CHECK(valueOf(dialog, "hair") == QStringLiteral("long"));
    CHECK(longHair->isChecked());
    CHECK_FALSE(dialog.tile(QStringLiteral("hair"), QStringLiteral("short"))->isChecked());
    CHECK(dialog.undoButton()->isEnabled());

    // A color of the palette, and a part of many values
    QToolButton* red = dialog.swatch(QStringLiteral("hairColor"), QStringLiteral("#b5562a"));
    REQUIRE(red != nullptr);
    red->click();
    CHECK(valueOf(dialog, "hairColor") == QStringLiteral("#b5562a"));
    CHECK(dialog.otherColorButton(QStringLiteral("hairColor")) != nullptr);
    showGroup(dialog, QStringLiteral("extras"));
    dialog.tile(QStringLiteral("extras"), QStringLiteral("freckles"))->click();
    dialog.tile(QStringLiteral("extras"), QStringLiteral("mole"))->click();
    const auto extras = [&dialog]() {
        return Avatar::resolved(dialog.portraits().at(dialog.current()).avatar)
            .value(QStringLiteral("extras"))
            .toArray();
    };
    CHECK(extras() == QJsonArray{QStringLiteral("freckles"), QStringLiteral("mole")});
    dialog.tile(QStringLiteral("extras"), QStringLiteral("freckles"))->click();
    CHECK(extras() == QJsonArray{QStringLiteral("mole")});

    // A part of the whole figure shows the whole figure
    showGroup(dialog, QStringLiteral("body"));
    CHECK(dialog.frame() == AvatarFrame::Full);
    dialog.frameButtons()->button(0)->click();
    CHECK(dialog.frame() == AvatarFrame::Bust);
    dialog.tile(QStringLiteral("height"), QStringLiteral("tall"))->click();
    CHECK(dialog.frame() == AvatarFrame::Full);

    // The sign of the gender
    showGroup(dialog, QStringLiteral("background"));
    REQUIRE(dialog.signBox() != nullptr);
    CHECK(dialog.signBox()->text().contains(QStringLiteral("♀")));
    dialog.signBox()->click();
    CHECK(dialog.portraits().at(0).avatar.value(QLatin1String(Avatar::SIGN)).toBool());

    // Undo goes back a step at a time, Ctrl+Z as well
    dialog.undoButton()->click();
    CHECK_FALSE(dialog.portraits().at(0).avatar.value(QLatin1String(Avatar::SIGN)).toBool());
    CHECK(valueOf(dialog, "height") == QStringLiteral("tall"));
    auto* undoKey = dialog.findChild<QShortcut*>();
    REQUIRE(undoKey != nullptr);
    CHECK(undoKey->key() == QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_Z)));
    emit undoKey->activated();
    CHECK(valueOf(dialog, "height") == QStringLiteral("medium"));

    // Randomize keeps the face calm, Start Over goes back to what the fields suggest
    dialog.styleButtons()->button(1)->click();
    for (int i = 0; i < 10; ++i) {
        dialog.randomButton()->click();
        CHECK((valueOf(dialog, "mouth") == QStringLiteral("neutral") ||
               valueOf(dialog, "mouth") == QStringLiteral("serious")));
        CHECK(valueOf(dialog, "beard") == QStringLiteral("none"));
    }
    CHECK(dialog.portraits().at(0).style == QLatin1String(AvatarLook::LINE));
    dialog.resetButton()->click();
    CHECK(Avatar::resolved(dialog.portraits().at(0).avatar).value(QStringLiteral("hair")) ==
          QStringLiteral("shoulder"));
    CHECK(dialog.portraits().at(0).style == QLatin1String(AvatarLook::LINE));
    CHECK(dialog.portraits().at(1).image == QStringLiteral("e7"));
}

TEST_CASE("Portrait window: the portraits of the card and their labels",
          "[gui][avatar][portrait_dialog]") {
    kalahari::gui::registerAllIcons();
    PortraitDialog dialog(anna());
    dialog.labelEdit()->setText(QStringLiteral("old"));
    emit dialog.labelEdit()->textEdited(QStringLiteral("old"));
    CHECK(dialog.portraits().at(1).label == QStringLiteral("old"));
    CHECK(dialog.portraitList()->item(0)->text() == QStringLiteral("old"));

    dialog.newButton()->click();
    REQUIRE(dialog.portraits().size() == 3);
    CHECK(dialog.current() == 2);
    CHECK(dialog.portraits().at(2).avatar == dialog.portraits().at(1).avatar);
    CHECK(dialog.portraits().at(2).label == QStringLiteral("new"));
    REQUIRE(dialog.portraitList()->count() == 2);
    CHECK(dialog.portraitList()->currentRow() == 1);
    showGroup(dialog, QStringLiteral("hair"));
    dialog.tile(QStringLiteral("hair"), QStringLiteral("bun"))->click();
    CHECK(valueOf(dialog, "hair") == QStringLiteral("bun"));

    // Back to the first portrait of parts: its own parts
    dialog.portraitList()->setCurrentRow(0);
    CHECK(dialog.current() == 1);
    CHECK(valueOf(dialog, "hair") == QStringLiteral("shoulder"));
    CHECK(dialog.labelEdit()->text() == QStringLiteral("old"));
    CHECK(dialog.tile(QStringLiteral("hair"), QStringLiteral("shoulder"))->isChecked());

    // Undo takes the new portrait back
    dialog.undoButton()->click();
    dialog.undoButton()->click();
    CHECK(dialog.portraits().size() == 2);
    CHECK(dialog.portraitList()->count() == 1);
}

TEST_CASE("Portrait window: the keys choose the tiles", "[gui][avatar][portrait_dialog]") {
    kalahari::gui::registerAllIcons();
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs an active window without a window on screen: run with "
             "QT_QPA_PLATFORM=offscreen");
    }
    PortraitDialog dialog(anna());
    dialog.show();
    dialog.activateWindow();
    REQUIRE(kalahari::test::waitUntil(
        [&dialog] { return QApplication::activeWindow() == &dialog; }));
    showGroup(dialog, QStringLiteral("hair"));
    QToolButton* shoulder = dialog.tile(QStringLiteral("hair"), QStringLiteral("shoulder"));
    REQUIRE(shoulder != nullptr);
    // The tiles in their places, which the arrows go by
    REQUIRE(kalahari::test::waitUntil([&dialog, shoulder] {
        return shoulder->isVisible() &&
               dialog.tile(QStringLiteral("hair"), QStringLiteral("long"))->x() > shoulder->x();
    }));
    shoulder->setFocus(Qt::TabFocusReason);
    REQUIRE(kalahari::test::waitUntil(
        [shoulder] { return QApplication::focusWidget() == shoulder; }));
    // Tab reaches the chosen tile of a part, not each of them
    CHECK(shoulder->focusPolicy() & Qt::TabFocus);
    CHECK_FALSE(dialog.tile(QStringLiteral("hair"), QStringLiteral("long"))->focusPolicy() &
                Qt::TabFocus);
    // The arrows choose the next value
    press(shoulder, Qt::Key_Right);
    CHECK(valueOf(dialog, "hair") == QStringLiteral("long"));
    CHECK(QApplication::focusWidget() ==
          dialog.tile(QStringLiteral("hair"), QStringLiteral("long")));
    dialog.close();
}

namespace {

/// Whether the window fits 1366x768 at 150%: 911 pixels wide and 460 high (a Mac's larger
/// controls need a few more)
void checkFits(const char* language) {
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 480;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    for (const char* frame : {Avatar::FRAME_BUST, Avatar::FRAME_FULL}) {
        const ChangedSetting<std::string> setting(Avatar::FRAME_SETTING, frame);
        const PortraitDialog dialog(anna());
        INFO("In " << language << ", " << frame << ": " << dialog.sizeHint().width() << "x"
                   << dialog.sizeHint().height());
        CHECK(dialog.sizeHint().width() <= 911);
        CHECK(dialog.sizeHint().height() <= MAX_HEIGHT);
    }
    // The window of the palettes too
    const kalahari::gui::dialogs::PortraitPalettesDialog palettes(
        kalahari::gui::AvatarPalettes::defaults());
    INFO("In " << language << ", the palettes: " << palettes.sizeHint().width() << "x"
               << palettes.sizeHint().height());
    CHECK(palettes.sizeHint().width() <= 911);
    CHECK(palettes.sizeHint().height() <= MAX_HEIGHT);
}

}  // namespace

TEST_CASE("Portrait window and Portrait Palettes: fit a small screen, in English and in Polish",
          "[gui][avatar][portrait_dialog]") {
    kalahari::gui::registerAllIcons();
    checkFits("English");
    const kalahari::test::Polish polish;
    REQUIRE(polish.loaded());
    checkFits("Polish");
    const PortraitDialog dialog(anna());
    CHECK(dialog.windowTitle() == QStringLiteral("Portret: Anna Wolska"));
}
