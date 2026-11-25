// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_kalahari_style.cpp
/// @brief The program's style: a grey menu item the arrow keys rest on is highlighted, softer
/// than an enabled one; a checked button is clearly on
#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/kalahari_style.h"
#include "kalahari_tests/editor/editor_test_utils.h"
#include <QAction>
#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
#include <algorithm>
#include <memory>
#include <utility>

using kalahari::gui::KalahariStyle;

namespace {

/// The color of @p menu at the right end of the item of @p action, where it has no text
QColor itemColor(QMenu& menu, QAction* action) {
    const QRect item = menu.actionGeometry(action);
    const QImage image = menu.grab().toImage();
    const QPointF point(item.right() - 6, item.center().y());
    return image.pixelColor((point * image.devicePixelRatio()).toPoint());
}

/// The arrow down, pressed in @p menu
void pressDown(QMenu& menu) {
    QKeyEvent pressed(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(&menu, &pressed);
    QKeyEvent released(QEvent::KeyRelease, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(&menu, &released);
}

} // namespace

TEST_CASE("Menus: a grey item the arrow keys rest on is highlighted, softer than an enabled one",
          "[gui][style]") {
    // Fusion highlights only enabled items: on a grey one only the status bar changed
    KalahariStyle style;
    const QColor menuColor(Qt::white);
    const QColor highlight(0x00, 0x78, 0xD4);
    QPalette palette;
    palette.setColor(QPalette::Base, menuColor);
    palette.setColor(QPalette::Window, QColor(0xF0, 0xF0, 0xF0));
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::Text, Qt::black);
    palette.setColor(QPalette::HighlightedText, Qt::white);

    QMenu menu;
    menu.setStyle(&style);
    menu.setPalette(palette);
    QAction* grey = menu.addAction(QStringLiteral("Close Book"));
    grey->setEnabled(false);
    QAction* enabled = menu.addAction(QStringLiteral("Open Book"));
    menu.popup(QPoint(0, 0));
    REQUIRE(kalahari::test::waitUntil([&menu] { return menu.isVisible(); }));

    // No item under the keys: the grey item has the menu's color
    CHECK(itemColor(menu, grey) == menuColor);

    // The keys rest on the grey item: a tint of the highlight, lighter than the highlight
    pressDown(menu);
    REQUIRE(menu.activeAction() == grey);
    const QColor tint = itemColor(menu, grey);
    CHECK(tint != menuColor);
    CHECK(tint != highlight);
    CHECK(tint.blue() > tint.red() + 40);
    CHECK(tint.lightness() > highlight.lightness());

    // On the enabled item: Fusion's highlight, and the grey item has the menu's color again
    pressDown(menu);
    REQUIRE(menu.activeAction() == enabled);
    CHECK(itemColor(menu, enabled) == highlight);
    CHECK(itemColor(menu, grey) == menuColor);
    menu.hide();
}


TEST_CASE("Buttons: a checked one is clearly on, filled with the highlight or framed in it",
          "[gui][style]") {
    // Fusion drew a checked button only a little darker (the days of writing)
    KalahariStyle style;
    const QColor button(0xE0, 0xE0, 0xE0);
    const QColor highlight(0x33, 0x33, 0x33);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0xF0, 0xF0, 0xF0));
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, Qt::black);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    const auto made = [&style, &palette](QAbstractButton* made) {
        made->setStyle(&style);
        made->setPalette(palette);
        made->setCheckable(true);
        made->resize(60, 28);
        return made;
    };
    // The color inside the button's left edge, where it has no text, and the lightness of the
    // lightest and the darkest pixel of its middle, where its text is
    const auto inside = [](QAbstractButton& shown) {
        const QImage image = shown.grab().toImage();
        return image.pixelColor((QPointF(4, 14) * image.devicePixelRatio()).toPoint());
    };
    const auto middle = [](QAbstractButton& shown) {
        const QImage image = shown.grab().toImage();
        const qreal ratio = image.devicePixelRatio();
        int lightest = 0;
        int darkest = 255;
        for (int y = static_cast<int>(8 * ratio); y < static_cast<int>(20 * ratio); ++y) {
            for (int x = static_cast<int>(16 * ratio); x < static_cast<int>(44 * ratio); ++x) {
                lightest = std::max(lightest, image.pixelColor(x, y).lightness());
                darkest = std::min(darkest, image.pixelColor(x, y).lightness());
            }
        }
        return std::pair{lightest, darkest};
    };

    for (const bool push : {false, true}) {
        CAPTURE(push);
        QAbstractButton* text = push ? static_cast<QAbstractButton*>(new QPushButton())
                                     : new QToolButton();
        const std::unique_ptr<QAbstractButton> owned(made(text));
        text->setText(QStringLiteral("Mo"));
        CHECK(inside(*text) != highlight);
        CHECK(middle(*text).second < 60);  // black text

        // Text alone: filled with the highlight, its text white
        text->setChecked(true);
        CHECK(inside(*text) == highlight);
        CHECK(middle(*text).first > 240);

        // With an icon (in the color of the highlight): a tint of it, framed in it
        QPixmap pixmap(16, 16);
        pixmap.fill(highlight);
        text->setIcon(QIcon(pixmap));
        const QColor tint = inside(*text);
        CHECK(tint != highlight);
        CHECK(tint != button);
        CHECK(tint.lightness() < button.lightness());
        CHECK(tint.lightness() > highlight.lightness());
        const QImage image = text->grab().toImage();
        const QColor frame =
            image.pixelColor(0, static_cast<int>(14 * image.devicePixelRatio()));
        CHECK(frame.lightness() < tint.lightness());
    }
}
