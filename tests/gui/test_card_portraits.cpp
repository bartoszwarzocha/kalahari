// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_card_portraits.cpp
/// @brief The portraits of a card put together from parts: on the cards, in the window
/// Portrait, the main one, the arrows, saved as a picture

#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_data.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/avatar.h"
#include "kalahari/gui/avatar_art.h"
#include "kalahari/gui/dialogs/portrait_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/card_page.h"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

#include <functional>

namespace kalahari {

using gui::Avatar;
using gui::CardPage;
using gui::dialogs::PortraitDialog;

namespace {

/// A new character @p title in the Workshop of the open book, a woman of 34 who is tall
QString addCharacter(core::ProjectManager& pm, const QString& title) {
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("character"));
    REQUIRE(kind);
    const QString id = pm.addElement(kind, title, core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    core::CardData card = pm.findElement(id)->card;
    card.color = QStringLiteral("#2e9e6a");
    card.values.insert(QStringLiteral("gender"), QStringLiteral("woman"));
    card.values.insert(QStringLiteral("age"), QStringLiteral("34"));
    card.values.insert(QStringLiteral("appearance"), QStringLiteral("Tall.\n"));
    REQUIRE(pm.setCard(id, card));
    return id;
}

/// A portrait of parts labelled @p label, with hair @p hair
core::CardPortrait composed(const QString& label, const QString& hair) {
    core::CardPortrait portrait;
    portrait.label = label;
    portrait.style = QStringLiteral("flat");
    portrait.avatar = Avatar::defaults();
    portrait.avatar.insert(QStringLiteral("hair"), hair);
    return portrait;
}

/// The portrait the card shows, @p side pixels on a side
QImage shownPortrait(const CardPage& page, int side) {
    return page.portraitButton()->icon().pixmap(QSize(side, side), 1.0).toImage();
}

/// The action of @p menu with object name @p name
QAction* actionOf(const QMenu* menu, const char* name) {
    for (QAction* action : menu->actions()) {
        if (action->objectName() == QLatin1String(name)) {
            return action;
        }
    }
    return nullptr;
}

/// Run @p command; in the window Portrait it opens, @p work works, then OK
/// @return Whether the window opened
bool composing(const std::function<void()>& command,
               const std::function<void(PortraitDialog&)>& work) {
    bool shown = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &work]() {
        auto* dialog = qobject_cast<PortraitDialog*>(test::shownDialog());
        if (dialog == nullptr || shown) {
            return;
        }
        shown = true;
        work(*dialog);
        dialog->acceptButton()->click();
    });
    timer.start();
    command();
    return shown;
}

}  // namespace

TEST_CASE("Card portraits: a portrait of parts is drawn on the card, in its color",
          "[gui][cards][avatar]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Portraits"));
    auto& pm = novel.pm;
    const QString anna = addCharacter(pm, QStringLiteral("Anna Wolska"));
    core::CardData card = pm.findElement(anna)->card;
    card.portraits = {composed(QStringLiteral("main"), QStringLiteral("long"))};
    REQUIRE(pm.setCard(anna, card));

    CardPage page(test::book(), false);
    page.showCard(anna);
    const int side = CardPage::SHORT_PORTRAIT;
    const QImage portrait = shownPortrait(page, side);
    REQUIRE(portrait.size() == QSize(side, side));
    // Not the initials: the background of the card is a gradient of its color, the face is
    // in the middle
    CHECK(portrait.pixelColor(2, 2) != QColor(QStringLiteral("#2e9e6a")));
    CHECK(portrait.pixelColor(side / 2, side / 2) != portrait.pixelColor(2, 2));
    const QImage drawn = Avatar::imageOf(card.portraits.first(), gui::AvatarFrame::Bust,
                                         QColor(QStringLiteral("#2e9e6a")),
                                         QStringLiteral("woman"), QSize(side, side));
    CHECK(portrait.pixelColor(side / 2, side / 2) == drawn.pixelColor(side / 2, side / 2));
    // The lists draw it as well
    const QImage listed =
        CardPage::portraitOf(pm, *pm.findElement(anna), side, 1.0).toImage();
    CHECK(listed.pixelColor(side / 2, side / 2) == drawn.pixelColor(side / 2, side / 2));
    // The short card has no arrows
    CHECK(page.previousPortraitButton() == nullptr);
}

TEST_CASE("Card portraits: Compose Portrait puts one together, with the description and the "
          "picture",
          "[gui][cards][avatar]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Portraits"));
    auto& pm = novel.pm;
    const QString anna = addCharacter(pm, QStringLiteral("Anna Wolska"));
    CardPage page(test::book(), true);
    QStringList added;
    QObject::connect(&page, &CardPage::materialAdded,
                     [&added](const QString& id) { added << id; });
    page.showCard(anna);

    QMenu* menu = page.portraitMenu();
    emit menu->aboutToShow();
    QAction* compose = actionOf(menu, "cardComposePortrait");
    REQUIRE(compose != nullptr);
    CHECK(compose->isEnabled());
    QString description;
    CHECK(composing([compose]() { compose->trigger(); },
                    [&description](PortraitDialog& dialog) {
                        // A new portrait, from the fields Gender and Age
                        CHECK(dialog.portraits().size() == 1);
                        dialog.groupList()->setCurrentRow(3);
                        dialog.tile(QStringLiteral("hair"), QStringLiteral("bun"))->click();
                        dialog.appendBox()->setChecked(true);
                        dialog.pictureBox()->setChecked(true);
                        description = dialog.description();
                    }));
    const core::CardData& card = pm.findElement(anna)->card;
    REQUIRE(card.portraits.size() == 1);
    CHECK(card.portraits.first().isComposed());
    CHECK(card.portraits.first().avatar.value(QStringLiteral("hair")) == QStringLiteral("bun"));
    CHECK(card.portraits.first().avatar.value(QStringLiteral("lashes")) ==
          QStringLiteral("long"));
    // The text of the writer stays, the description goes after it
    CHECK(description.contains(QStringLiteral("hair in a bun")));
    CHECK(card.value(QStringLiteral("appearance")) == QStringLiteral("Tall.\n") + description);
    // The picture in the materials, linked with the card; the portrait stays the one of parts
    REQUIRE(added.size() == 1);
    const core::ProjectElement* picture = pm.findElement(added.first());
    REQUIRE(picture != nullptr);
    CHECK(pm.isMaterial(*picture));
    CHECK(picture->linkTo(anna) != nullptr);
    CHECK(picture->title == QStringLiteral("Portrait – Anna Wolska"));
    CHECK_FALSE(QImage(pm.filePathOf(*picture)).isNull());
    CHECK(pm.findElement(anna)->card.portraits.size() == 1);

    // Compose Portrait again opens the one shown; Cancel changes nothing
    emit menu->aboutToShow();
    bool opened = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&opened]() {
        if (auto* dialog = qobject_cast<PortraitDialog*>(test::shownDialog());
            dialog != nullptr && !opened) {
            opened = true;
            CHECK(dialog->current() == 0);
            CHECK(dialog->portraits().size() == 1);
            dialog->tile(QStringLiteral("face"), QStringLiteral("round"))->click();
            dialog->reject();
        }
    });
    timer.start();
    actionOf(menu, "cardComposePortrait")->trigger();
    timer.stop();
    CHECK(opened);
    CHECK(pm.findElement(anna)->card.portraits.first().avatar.value(QStringLiteral("face")) ==
          Avatar::defaults().value(QStringLiteral("face")));
}

TEST_CASE("Card portraits: the portraits of the card, the main one and the arrows",
          "[gui][cards][avatar]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Portraits"));
    auto& pm = novel.pm;
    const QString anna = addCharacter(pm, QStringLiteral("Anna Wolska"));
    core::CardData card = pm.findElement(anna)->card;
    card.portraits = {composed(QStringLiteral("main"), QStringLiteral("short")),
                      composed(QStringLiteral("youth"), QStringLiteral("long"))};
    REQUIRE(pm.setCard(anna, card));

    CardPage page(test::book(), true);
    QStringList added;
    QObject::connect(&page, &CardPage::materialAdded,
                     [&added](const QString& id) { added << id; });
    page.showCard(anna);
    page.show();
    QApplication::processEvents();
    REQUIRE(page.nextPortraitButton() != nullptr);
    CHECK(page.nextPortraitButton()->isVisible());
    CHECK(page.portraitLabel()->text() == QStringLiteral("★ main"));
    const int side = CardPage::FULL_PORTRAIT;
    const QImage first = shownPortrait(page, side);

    // The arrows go through the portraits, around the end
    page.nextPortraitButton()->click();
    CHECK(page.shownPortrait() == 1);
    CHECK(page.portraitLabel()->text() == QStringLiteral("youth"));
    CHECK(shownPortrait(page, side) != first);
    page.nextPortraitButton()->click();
    CHECK(page.shownPortrait() == 0);
    page.previousPortraitButton()->click();
    CHECK(page.shownPortrait() == 1);

    // The submenu of the portraits: the one shown checked, Set as Main, New Portrait...
    QMenu* menu = page.portraitMenu();
    emit menu->aboutToShow();
    auto* portraits = menu->findChild<QMenu*>(QStringLiteral("cardPortraits"));
    REQUIRE(portraits != nullptr);
    REQUIRE(portraits->actions().size() >= 5);
    CHECK(portraits->actions().at(0)->text() == QStringLiteral("★ main"));
    CHECK_FALSE(portraits->actions().at(0)->isChecked());
    CHECK(portraits->actions().at(1)->isChecked());
    QAction* save = actionOf(menu, "cardSavePortrait");
    REQUIRE(save != nullptr);
    save->trigger();
    REQUIRE(added.size() == 1);
    CHECK(pm.findElement(added.first())->title ==
          QStringLiteral("Portrait – Anna Wolska (youth)"));
    CHECK(QImage(pm.filePathOf(*pm.findElement(added.first()))).size() == QSize(800, 800));

    emit menu->aboutToShow();
    portraits = menu->findChild<QMenu*>(QStringLiteral("cardPortraits"));
    REQUIRE(portraits != nullptr);
    actionOf(portraits, "cardMainPortrait")->trigger();
    CHECK(page.shownPortrait() == 0);
    REQUIRE(pm.findElement(anna)->card.portraits.size() == 2);
    CHECK(pm.findElement(anna)->card.portraits.first().label == QStringLiteral("youth"));
    CHECK(page.portraitLabel()->text() == QStringLiteral("★ youth"));

    // New Portrait... adds one, shown after the window
    emit menu->aboutToShow();
    portraits = menu->findChild<QMenu*>(QStringLiteral("cardPortraits"));
    QAction* newPortrait = actionOf(portraits, "cardNewPortrait");
    REQUIRE(newPortrait != nullptr);
    CHECK(composing([newPortrait]() { newPortrait->trigger(); },
                    [](PortraitDialog& dialog) {
                        CHECK(dialog.current() == 2);
                        dialog.labelEdit()->setText(QStringLiteral("after the battle"));
                        emit dialog.labelEdit()->textEdited(dialog.labelEdit()->text());
                    }));
    REQUIRE(pm.findElement(anna)->card.portraits.size() == 3);
    CHECK(pm.findElement(anna)->card.portraits.at(2).label ==
          QStringLiteral("after the battle"));
    CHECK(page.shownPortrait() == 2);

    // Remove Portrait takes out the one shown
    emit menu->aboutToShow();
    actionOf(menu, "cardPortraitRemove")->trigger();
    REQUIRE(pm.findElement(anna)->card.portraits.size() == 2);
    CHECK(page.shownPortrait() == 1);
    page.removePortrait();
    REQUIRE(pm.findElement(anna)->card.portraits.size() == 1);
    CHECK(pm.findElement(anna)->card.portraits.first().label == QStringLiteral("youth"));
    CHECK_FALSE(page.nextPortraitButton()->isVisible());
}

}  // namespace kalahari
