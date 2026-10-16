/// @file test_kalahari_style.cpp
/// @brief The program's style: a grey menu item the arrow keys rest on is highlighted, softer
/// than an enabled one
#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/kalahari_style.h"
#include "../editor/editor_test_utils.h"
#include <QAction>
#include <QApplication>
#include <QColor>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
#include <QPalette>

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
